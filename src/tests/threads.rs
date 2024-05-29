use crate::threads::spawn;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::Duration;

#[test]
fn test_pthread() {
    struct Context {
        val: AtomicI32,
        condvar: Condvar,
    }
    let ctx = Arc::new(Context {
        val: AtomicI32::new(3),
        condvar: Condvar::new(),
    });
    let mutex = Mutex::new(());
    let ctx2 = ctx.clone();
    let made = spawn(move || {
        ctx2.val.fetch_add(2, Ordering::Release);
        ctx2.condvar.notify_one();
        println!("condvar signalled");
    });
    assert!(made);

    let lock = mutex.lock().unwrap();
    let (_lock, timeout) = ctx
        .condvar
        .wait_timeout_while(lock, Duration::from_secs(5), |()| {
            println!("looping with lock held");
            if ctx.val.load(Ordering::Acquire) != 5 {
                println!("test_pthread: value did not yet reach goal");
                return true;
            }
            false
        })
        .unwrap();
    if timeout.timed_out() {
        panic!(concat!(
            "Timeout waiting for condition variable to be notified! ",
            "Does the platform support signalling a condvar without the mutex held?"
        ));
    }
}
