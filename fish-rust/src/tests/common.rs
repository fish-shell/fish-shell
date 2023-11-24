use crate::common::{
    cstr2wcstring, format_llong_safe, format_size_safe, scoped_push, ScopeGuard, ScopeGuarding,
};
use crate::wchar::prelude::*;

#[test]
fn test_scoped_push() {
    struct Context {
        value: i32,
    }

    let mut value = 0;
    let mut ctx = Context { value };
    {
        let mut ctx = scoped_push(&mut ctx, |ctx| &mut ctx.value, value + 1);
        value = ctx.value;
        assert_eq!(value, 1);
        {
            let mut ctx = scoped_push(&mut ctx, |ctx| &mut ctx.value, value + 1);
            assert_eq!(ctx.value, 2);
            ctx.value = 5;
            assert_eq!(ctx.value, 5);
        }
        assert_eq!(ctx.value, 1);
    }
    assert_eq!(ctx.value, 0);
}

#[test]
fn test_scope_guard() {
    let relaxed = std::sync::atomic::Ordering::Relaxed;
    let counter = std::sync::atomic::AtomicUsize::new(0);
    {
        let guard = ScopeGuard::new(123, |arg| {
            assert_eq!(*arg, 123);
            counter.fetch_add(1, relaxed);
        });
        assert_eq!(counter.load(relaxed), 0);
        std::mem::drop(guard);
        assert_eq!(counter.load(relaxed), 1);
    }
    // commit also invokes the callback.
    {
        let guard = ScopeGuard::new(123, |arg| {
            assert_eq!(*arg, 123);
            counter.fetch_add(1, relaxed);
        });
        assert_eq!(counter.load(relaxed), 1);
        let val = ScopeGuard::commit(guard);
        assert_eq!(counter.load(relaxed), 2);
        assert_eq!(val, 123);
    }
}

#[test]
fn test_scope_guard_consume() {
    // The following pattern works.
    struct Storage {
        value: &'static str,
    }
    let obj = Storage { value: "nu" };
    assert_eq!(obj.value, "nu");
    let obj = scoped_push(obj, |obj| &mut obj.value, "mu");
    assert_eq!(obj.value, "mu");
    let obj = scoped_push(obj, |obj| &mut obj.value, "mu2");
    assert_eq!(obj.value, "mu2");
    let obj = ScopeGuarding::commit(obj);
    assert_eq!(obj.value, "mu");
    let obj = ScopeGuarding::commit(obj);
    assert_eq!(obj.value, "nu");
}

#[test]
fn test_assert_is_locked() {
    let lock = std::sync::Mutex::new(());
    let _guard = lock.lock().unwrap();
    assert_is_locked!(&lock);
}

#[test]
fn test_format() {
    // Testing formatting functions
    struct Test {
        val: u64,
        expected: &'static str,
    }
    let tests = [
        Test {
            val: 0,
            expected: "empty",
        },
        Test {
            val: 1,
            expected: "1B",
        },
        Test {
            val: 2,
            expected: "2B",
        },
        Test {
            val: 1024,
            expected: "1kB",
        },
        Test {
            val: 1870,
            expected: "1.8kB",
        },
        Test {
            val: 4322911,
            expected: "4.1MB",
        },
    ];

    for test in tests {
        let mut buff = [0_u8; 128];
        format_size_safe(&mut buff, test.val);
        assert_eq!(cstr2wcstring(&buff), WString::from_str(test.expected));
    }

    for j in -129..=129 {
        let mut buff1 = [0_u8; 64];
        let mut buff2 = [0_u8; 64];
        format_llong_safe(&mut buff1, j);
        unsafe { libc::snprintf(buff2.as_mut_ptr().cast(), 64, "%d\0".as_ptr().cast(), j) };
        assert_eq!(cstr2wcstring(&buff1), cstr2wcstring(&buff2));
    }

    let q = i64::MIN;
    let mut buff1 = [0_u8; 64];
    let mut buff2 = [0_u8; 64];
    format_llong_safe(&mut buff1, q);
    unsafe { libc::snprintf(buff2.as_mut_ptr().cast(), 128, "%ld\0".as_ptr().cast(), q) };
    assert_eq!(cstr2wcstring(&buff1), cstr2wcstring(&buff2));
}
