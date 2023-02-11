use std::mem;

/// A scoped manager to save the current value of some variable, and optionally set it to a new
/// value. When dropped, it restores the variable to its old value.
///
/// This can be handy when there are multiple code paths to exit a block.
pub struct ScopedPush<'a, T> {
    var: &'a mut T,
    saved_value: Option<T>,
}

impl<'a, T> ScopedPush<'a, T> {
    pub fn new(var: &'a mut T, new_value: T) -> Self {
        let saved_value = mem::replace(var, new_value);

        Self {
            var,
            saved_value: Some(saved_value),
        }
    }

    pub fn restore(&mut self) {
        if let Some(saved_value) = self.saved_value.take() {
            *self.var = saved_value;
        }
    }
}

impl<'a, T> Drop for ScopedPush<'a, T> {
    fn drop(&mut self) {
        self.restore()
    }
}
