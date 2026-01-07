use crate::flog::flog;
use std::cell::{Ref, RefMut};
use std::sync::MutexGuard;
use std::sync::atomic::{AtomicBool, AtomicPtr, Ordering};

#[cfg(not(target_has_atomic = "64"))]
pub use portable_atomic::AtomicU64;
#[cfg(target_has_atomic = "64")]
pub use std::sync::atomic::AtomicU64;

#[derive(Debug, Default)]
pub struct RelaxedAtomicBool(AtomicBool);

impl RelaxedAtomicBool {
    pub const fn new(value: bool) -> Self {
        Self(AtomicBool::new(value))
    }
    pub fn load(&self) -> bool {
        self.0.load(Ordering::Relaxed)
    }
    pub fn store(&self, value: bool) {
        self.0.store(value, Ordering::Relaxed)
    }
    pub fn swap(&self, value: bool) -> bool {
        self.0.swap(value, Ordering::Relaxed)
    }
}

impl Clone for RelaxedAtomicBool {
    fn clone(&self) -> Self {
        Self(AtomicBool::new(self.load()))
    }
}

/// An atomic reference type, allowing &'static values to be stored.
/// This uses relaxed ordering - it's intended for string literals.
/// Note that because string literals are fat pointers, we can't store one
/// directly in an AtomicPtr, so we store a pointer to the string literal instead!
pub struct AtomicRef<T: ?Sized + 'static>(AtomicPtr<&'static T>);

impl<T: ?Sized> AtomicRef<T> {
    pub const fn new(value: &'static &'static T) -> Self {
        Self(AtomicPtr::new(std::ptr::from_ref(value).cast_mut()))
    }

    pub fn load(&self) -> &'static T {
        unsafe { *self.0.load(Ordering::Relaxed) }
    }

    pub fn store(&self, value: &'static &'static T) {
        self.0
            .store(std::ptr::from_ref(value).cast_mut(), Ordering::Relaxed)
    }
}

pub struct DebugRef<'a, T>(Ref<'a, T>);

impl<'a, T> DebugRef<'a, T> {
    pub fn new(r: Ref<'a, T>) -> Self {
        flog!(
            refcell,
            "CREATE DebugRef",
            std::backtrace::Backtrace::capture()
        );
        Self(r)
    }
}

impl<'a, T> Drop for DebugRef<'a, T> {
    fn drop(&mut self) {
        flog!(
            refcell,
            "DROP DebugRef",
            std::backtrace::Backtrace::capture()
        );
    }
}

impl<'a, T> std::ops::Deref for DebugRef<'a, T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

pub struct DebugRefMut<'a, T>(RefMut<'a, T>);

impl<'a, T> DebugRefMut<'a, T> {
    pub fn new(r: RefMut<'a, T>) -> Self {
        flog!(
            refcell,
            "CREATE DebugRefMut",
            std::backtrace::Backtrace::capture()
        );
        Self(r)
    }
}

impl<'a, T> Drop for DebugRefMut<'a, T> {
    fn drop(&mut self) {
        flog!(
            refcell,
            "DROP DebugRefMut",
            std::backtrace::Backtrace::capture()
        );
    }
}
impl<'a, T> std::ops::Deref for DebugRefMut<'a, T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<'a, T> std::ops::DerefMut for DebugRefMut<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

pub struct DebugMutexGuard<'a, T>(MutexGuard<'a, T>);

impl<'a, T> DebugMutexGuard<'a, T> {
    pub fn new(r: MutexGuard<'a, T>) -> Self {
        flog!(
            refcell,
            "CREATE DebugMutexGuard",
            std::backtrace::Backtrace::capture()
        );
        Self(r)
    }
}

impl<'a, T> Drop for DebugMutexGuard<'a, T> {
    fn drop(&mut self) {
        flog!(
            refcell,
            "DROP DebugMutexGuard",
            std::backtrace::Backtrace::capture()
        );
    }
}

impl<'a, T> std::ops::Deref for DebugMutexGuard<'a, T> {
    type Target = T;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
impl<'a, T> std::ops::DerefMut for DebugMutexGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}
