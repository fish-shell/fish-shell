use std::ops::Deref;

/// Raw pointer that implements Default.
/// Additionally it implements Deref so it's more ergonomic than Option<std::ptr::NonNull>.
pub struct ConstPointer<T>(*const T);

impl<T> From<&T> for ConstPointer<T> {
    fn from(value: &T) -> Self {
        Self(value)
    }
}

impl<T> Default for ConstPointer<T> {
    fn default() -> Self {
        Self(std::ptr::null())
    }
}

impl<T> Clone for ConstPointer<T> {
    fn clone(&self) -> Self {
        Self(self.0)
    }
}

impl<T> Copy for ConstPointer<T> {}

impl<T> Deref for ConstPointer<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        assert!(!self.0.is_null());
        unsafe { &*self.0 }
    }
}
