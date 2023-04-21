use std::ops::Deref;

// Raw pointer that implements Refault.
pub struct ConstPointer<T>(*const T);

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

impl<T> Deref for ConstPointer<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        unsafe { &*self.0 }
    }
}
