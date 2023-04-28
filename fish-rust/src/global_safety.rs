use std::cell::RefCell;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Weak,
};

#[derive(Debug)]
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

pub struct SharedFromThisBase<T> {
    weak: RefCell<Weak<T>>,
}

impl<T> SharedFromThisBase<T> {
    pub fn new() -> SharedFromThisBase<T> {
        SharedFromThisBase {
            weak: RefCell::new(Weak::new()),
        }
    }

    pub fn initialize(&self, r: &Arc<T>) {
        *self.weak.borrow_mut() = Arc::downgrade(r);
    }
}

pub trait SharedFromThis<T> {
    fn get_base(&self) -> &SharedFromThisBase<T>;

    fn shared_from_this(&self) -> Arc<T> {
        self.get_base().weak.borrow().upgrade().unwrap()
    }
}
