use crate::common::{scoped_push, truncate_at_nul, ScopeGuard, ScopeGuarding};
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
fn test_truncate_at_nul() {
    assert_eq!(truncate_at_nul(L!("abc\0def")), L!("abc"));
    assert_eq!(truncate_at_nul(L!("abc")), L!("abc"));
    assert_eq!(truncate_at_nul(L!("\0abc")), L!(""));
}
