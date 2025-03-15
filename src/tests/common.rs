use crate::common::{scoped_push, truncate_at_nul, ScopeGuard, ScopedCell, ScopedRefCell};
use crate::wchar::prelude::*;

#[test]
fn test_scoped_cell() {
    let cell = ScopedCell::new(42);

    {
        let _guard = cell.scoped_mod(|x| *x += 1);
        assert_eq!(cell.get(), 43);
    }

    assert_eq!(cell.get(), 42);
}

#[test]
fn test_scoped_refcell() {
    #[derive(Debug, PartialEq, Clone)]
    struct Data {
        x: i32,
        y: i32,
    }

    let cell = ScopedRefCell::new(Data { x: 1, y: 2 });

    {
        let _guard = cell.scoped_set(10, |d| &mut d.x);
        assert_eq!(cell.borrow().x, 10);
    }
    assert_eq!(cell.borrow().x, 1);

    {
        let _guard = cell.scoped_replace(Data { x: 42, y: 99 });
        assert_eq!(*cell.borrow(), Data { x: 42, y: 99 });
    }
    assert_eq!(*cell.borrow(), Data { x: 1, y: 2 });
}

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
        ScopeGuard::commit(guard);
        assert_eq!(counter.load(relaxed), 2);
    }
}

#[test]
fn test_truncate_at_nul() {
    assert_eq!(truncate_at_nul(L!("abc\0def")), L!("abc"));
    assert_eq!(truncate_at_nul(L!("abc")), L!("abc"));
    assert_eq!(truncate_at_nul(L!("\0abc")), L!(""));
}
