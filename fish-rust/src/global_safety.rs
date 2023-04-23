use std::sync::atomic::{AtomicBool, Ordering};

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
