use std::sync::{
    Arc, Condvar, Mutex,
    atomic::{AtomicU32, Ordering},
};
use std::time::Duration;

use crate::global_safety::RelaxedAtomicBool;
use crate::reader::{Reader, fake_scoped_reader};
use crate::tests::prelude::*;
use crate::threads::{ThreadPool, debounce::Debounce};

#[test]
fn test_debounce() {
    let parser = TestParser::new();
    let pool = ThreadPool::new(1, 16);
    // Run 8 functions using a condition variable.
    // Only the first and last should run.
    let db = pool.debouncer(Duration::from_secs(0));
    const count: usize = 8;

    struct Context {
        handler_ran: [RelaxedAtomicBool; count],
        completion_ran: [RelaxedAtomicBool; count],
        ready_to_go: Mutex<bool>,
        cv: Condvar,
    }

    let ctx = Arc::new(Context {
        handler_ran: std::array::from_fn(|_i| RelaxedAtomicBool::new(false)),
        completion_ran: std::array::from_fn(|_i| RelaxedAtomicBool::new(false)),
        ready_to_go: Mutex::new(false),
        cv: Condvar::new(),
    });

    // "Enqueue" all functions. Each one waits until ready_to_go.
    for idx in 0..count {
        assert!(!ctx.handler_ran[idx].load());
        let performer = {
            let ctx = ctx.clone();
            move || {
                let guard = ctx.ready_to_go.lock().unwrap();
                let _guard = ctx.cv.wait_while(guard, |ready| !*ready).unwrap();
                ctx.handler_ran[idx].store(true);
                idx
            }
        };
        let completer = {
            let ctx = ctx.clone();
            move |_ctx: &mut Reader, idx: usize| {
                ctx.completion_ran[idx].store(true);
            }
        };
        db.perform_with_completion(performer, completer);
    }

    // We're ready to go.
    *ctx.ready_to_go.lock().unwrap() = true;
    ctx.cv.notify_all();

    // Wait until the last completion is done.
    let mut reader = fake_scoped_reader(&parser);
    while !ctx.completion_ran.last().unwrap().load() {
        pool.invoke_completions(&mut reader);
    }
    pool.drain_all(&mut reader);

    // Each perform() call may displace an existing queued operation.
    // Each operation waits until all are queued.
    // Therefore we expect the last perform() to have run, and at most one more.
    assert!(ctx.handler_ran.last().unwrap().load());
    assert!(ctx.completion_ran.last().unwrap().load());

    let mut total_ran = 0;
    for idx in 0..count {
        if ctx.handler_ran[idx].load() {
            total_ran += 1;
        }
        assert_eq!(ctx.handler_ran[idx].load(), ctx.completion_ran[idx].load());
    }
    assert!(total_ran <= 2);
}

#[test]
fn test_debounce_timeout() {
    // Verify that debounce doesn't wait forever.
    // Use a shared_ptr so we don't have to join our threads.
    let pool = ThreadPool::new(1, 16);
    let timeout = Duration::from_millis(500);
    let db = pool.debouncer(timeout);

    struct Data {
        db: Debounce,
        exit_ok: Mutex<bool>,
        cv: Condvar,
        running: AtomicU32,
    }

    let data = Arc::new(Data {
        db,
        exit_ok: Mutex::new(false),
        cv: Condvar::new(),
        running: AtomicU32::new(0),
    });

    // Our background handler. Note this just blocks until exit_ok is set.
    let handler = {
        let data = data.clone();
        move || {
            data.running.fetch_add(1, Ordering::Relaxed);
            let guard = data.exit_ok.lock().unwrap();
            let _guard = data.cv.wait_while(guard, |exit_ok| !*exit_ok);
        }
    };

    // Spawn the handler twice. This should not modify the thread token.
    let token1 = data.db.perform(handler.clone());
    let token2 = data.db.perform(handler.clone());
    assert_eq!(token1, token2);

    // Wait 75 msec, then enqueue something else; this should spawn a new thread.
    std::thread::sleep(timeout + timeout / 2);
    assert!(data.running.load(Ordering::Relaxed) == 1);
    let token3 = data.db.perform(handler);
    assert!(token3 > token2);

    // Release all the threads.
    let mut exit_ok = data.exit_ok.lock().unwrap();
    *exit_ok = true;
    data.cv.notify_all();
}
