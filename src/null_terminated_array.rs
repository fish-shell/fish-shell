use crate::common::{assert_send, assert_sync};
use std::ffi::{CStr, CString, c_char};
use std::marker::PhantomData;
use std::pin::Pin;
use std::ptr;

/// This supports the null-terminated array of NUL-terminated strings consumed by exec.
/// Given a list of strings, construct a vector of pointers to those strings contents.
/// This is used for building null-terminated arrays of null-terminated strings.
struct NullTerminatedArray<'p> {
    pointers: Box<[*const c_char]>,
    _phantom: PhantomData<&'p CStr>,
}

impl<'p> NullTerminatedArray<'p> {
    /// Return the list of pointers, appropriate for envp or argv.
    /// Note this returns a mutable array of const strings. The caller may rearrange the strings but
    /// not modify their contents.
    /// We freely give out mutable pointers even though we are not mut; this is because most of the uses
    /// expect the array to be mutable even though fish does not mutate it, so it's either this or cast
    /// away the const at the call site.
    pub fn get(&self) -> *mut *const c_char {
        assert!(
            !self.pointers.is_empty() && self.pointers.last().unwrap().is_null(),
            "Should have null terminator"
        );
        self.pointers.as_ptr().cast_mut()
    }
    /// Construct from a list of "strings".
    /// This holds pointers into the strings.
    pub fn new<S: AsRef<CStr>>(strs: &'p [S]) -> Self {
        let mut pointers = Vec::new();
        pointers.reserve_exact(1 + strs.len());
        for s in strs {
            pointers.push(s.as_ref().as_ptr());
        }
        pointers.push(ptr::null());
        NullTerminatedArray {
            pointers: pointers.into_boxed_slice(),
            _phantom: PhantomData,
        }
    }
}

/// Safety: NullTerminatedArray is Send and Sync because it's immutable.
unsafe impl Send for NullTerminatedArray<'_> {}
unsafe impl Sync for NullTerminatedArray<'_> {}

/// A container which exposes a null-terminated array of pointers to strings that it owns.
/// This is useful for persisted null-terminated arrays, e.g. the exported environment variable
/// list. This assumes u8, since we don't need this for wide chars.
pub struct OwningNullTerminatedArray {
    // Note that null_terminated_array holds pointers into our boxed strings.
    // The 'static is a lie.
    strings: Pin<Box<[CString]>>,
    null_terminated_array: NullTerminatedArray<'static>,
}

const _: () = assert_send::<OwningNullTerminatedArray>();
const _: () = assert_sync::<OwningNullTerminatedArray>();

impl OwningNullTerminatedArray {
    pub fn get(&self) -> *mut *const c_char {
        self.null_terminated_array.get()
    }
    pub fn get_mut(&self) -> *mut *mut c_char {
        self.get().cast()
    }
    /// Construct, taking ownership of a list of strings.
    pub fn new(strs: Vec<CString>) -> Self {
        let strings = strs.into_boxed_slice();
        // Safety: we're pinning the strings, so they won't move.
        let string_slice: &'static [CString] = unsafe { std::mem::transmute(&*strings) };
        OwningNullTerminatedArray {
            strings: Pin::from(strings),
            null_terminated_array: NullTerminatedArray::new(string_slice),
        }
    }
    pub fn len(&self) -> usize {
        self.strings.len()
    }
    pub fn iter(&self) -> impl Iterator<Item = &CString> {
        self.strings.iter()
    }
}

#[cfg(test)]
mod tests {
    use super::{NullTerminatedArray, OwningNullTerminatedArray};
    use std::ffi::{CStr, CString};
    use std::ptr;

    #[test]
    fn test_null_terminated_array() {
        let owned_strs = &[CString::new("foo").unwrap(), CString::new("bar").unwrap()];
        let strs = owned_strs.iter().map(|s| s.as_c_str()).collect::<Vec<_>>();
        let arr = NullTerminatedArray::new(&strs);
        let ptr = arr.get();
        unsafe {
            assert_eq!(CStr::from_ptr(*ptr).to_str().unwrap(), "foo");
            assert_eq!(CStr::from_ptr(*ptr.add(1)).to_str().unwrap(), "bar");
            assert_eq!(*ptr.add(2), ptr::null());
        }
    }

    #[test]
    fn test_owning_null_terminated_array() {
        let owned_strs = vec![CString::new("foo").unwrap(), CString::new("bar").unwrap()];
        let arr = OwningNullTerminatedArray::new(owned_strs);
        let ptr = arr.get();
        unsafe {
            assert_eq!(CStr::from_ptr(*ptr).to_str().unwrap(), "foo");
            assert_eq!(CStr::from_ptr(*ptr.add(1)).to_str().unwrap(), "bar");
            assert_eq!(*ptr.add(2), ptr::null());
        }
        assert_eq!(arr.len(), 2);
        let mut iter = arr.iter();
        assert_eq!(iter.next().map(|s| s.to_str().unwrap()), Some("foo"));
        assert_eq!(iter.next().map(|s| s.to_str().unwrap()), Some("bar"));
        assert_eq!(iter.next(), None);
    }
}
