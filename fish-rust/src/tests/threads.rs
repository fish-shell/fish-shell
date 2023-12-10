use crate::threads::spawn;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::{Arc, Condvar, Mutex};

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
    });
    assert!(made);
    let mut lock = mutex.lock().unwrap();
    loop {
        lock = ctx.condvar.wait(lock).unwrap();
        let v = ctx.val.load(Ordering::Acquire);
        if v == 5 {
            return;
        }
        println!("test_pthread: value did not yet reach goal")
    }
}
