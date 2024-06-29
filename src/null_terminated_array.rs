use crate::common::{assert_send, assert_sync};
use std::ffi::{c_char, CStr, CString};
use std::marker::PhantomData;
use std::pin::Pin;
use std::ptr;

pub trait NulTerminatedString {
    type CharType: Copy;

    /// Return a pointer to the null-terminated string.
    fn c_str(&self) -> *const Self::CharType;
}

impl NulTerminatedString for CStr {
    type CharType = c_char;

    fn c_str(&self) -> *const c_char {
        self.as_ptr()
    }
}

pub trait AsNullTerminatedArray {
    type CharType;
    fn get(&self) -> *mut *const Self::CharType;
}

/// This supports the null-terminated array of NUL-terminated strings consumed by exec.
/// Given a list of strings, construct a vector of pointers to those strings contents.
/// This is used for building null-terminated arrays of null-terminated strings.
pub struct NullTerminatedArray<'p, T: NulTerminatedString + ?Sized> {
    pointers: Box<[*const T::CharType]>,
    _phantom: PhantomData<&'p T>,
}

impl<'p, Str: NulTerminatedString + ?Sized> AsNullTerminatedArray for NullTerminatedArray<'p, Str> {
    type CharType = Str::CharType;
    /// Return the list of pointers, appropriate for envp or argv.
    /// Note this returns a mutable array of const strings. The caller may rearrange the strings but
    /// not modify their contents.
    /// We freely give out mutable pointers even though we are not mut; this is because most of the uses
    /// expect the array to be mutable even though fish does not mutate it, so it's either this or cast
    /// away the const at the call site.
    fn get(&self) -> *mut *const Str::CharType {
        assert!(
            !self.pointers.is_empty() && self.pointers.last().unwrap().is_null(),
            "Should have null terminator"
        );
        self.pointers.as_ptr() as *mut *const Str::CharType
    }
}
impl<'p, Str: NulTerminatedString + ?Sized> NullTerminatedArray<'p, Str> {
    /// Construct from a list of "strings".
    /// This holds pointers into the strings.
    pub fn new<S: AsRef<Str>>(strs: &'p [S]) -> Self {
        let mut pointers = Vec::new();
        pointers.reserve_exact(1 + strs.len());
        for s in strs {
            pointers.push(s.as_ref().c_str());
        }
        pointers.push(ptr::null());
        NullTerminatedArray {
            pointers: pointers.into_boxed_slice(),
            _phantom: PhantomData,
        }
    }
}

/// Safety: NullTerminatedArray is Send and Sync because it's immutable.
unsafe impl<T: NulTerminatedString + ?Sized + Send> Send for NullTerminatedArray<'_, T> {}
unsafe impl<T: NulTerminatedString + ?Sized + Sync> Sync for NullTerminatedArray<'_, T> {}

/// A container which exposes a null-terminated array of pointers to strings that it owns.
/// This is useful for persisted null-terminated arrays, e.g. the exported environment variable
/// list. This assumes u8, since we don't need this for wide chars.
pub struct OwningNullTerminatedArray {
    // Note that null_terminated_array holds pointers into our boxed strings.
    // The 'static is a lie.
    _strings: Pin<Box<[CString]>>,
    null_terminated_array: NullTerminatedArray<'static, CStr>,
}

const _: () = assert_send::<OwningNullTerminatedArray>();
const _: () = assert_sync::<OwningNullTerminatedArray>();

impl AsNullTerminatedArray for OwningNullTerminatedArray {
    type CharType = c_char;
    /// Cover over null_terminated_array.get().
    fn get(&self) -> *mut *const c_char {
        self.null_terminated_array.get()
    }
}

impl OwningNullTerminatedArray {
    pub fn get_mut(&self) -> *mut *mut c_char {
        self.get().cast()
    }
    /// Construct, taking ownership of a list of strings.
    pub fn new(strs: Vec<CString>) -> Self {
        let strings = strs.into_boxed_slice();
        // Safety: we're pinning the strings, so they won't move.
        let string_slice: &'static [CString] = unsafe { std::mem::transmute(&*strings) };
        OwningNullTerminatedArray {
            _strings: Pin::from(strings),
            null_terminated_array: NullTerminatedArray::new(string_slice),
        }
    }
}

/// Return the length of a null-terminated array of pointers to something.
pub(crate) fn null_terminated_array_length<T>(mut arr: *const *const T) -> usize {
    let mut len = 0;
    // Safety: caller must ensure that arr is null-terminated.
    unsafe {
        while !arr.read().is_null() {
            arr = arr.offset(1);
            len += 1;
        }
    }
    len
}

#[test]
fn test_null_terminated_array_length() {
    let arr = [&1, &2, &3, std::ptr::null()];
    assert_eq!(null_terminated_array_length(arr.as_ptr()), 3);
    let arr: &[*const u64] = &[std::ptr::null()];
    assert_eq!(null_terminated_array_length(arr.as_ptr()), 0);
}

#[test]
fn test_null_terminated_array() {
    let owned_strs = &[CString::new("foo").unwrap(), CString::new("bar").unwrap()];
    let strs = owned_strs.iter().map(|s| s.as_c_str()).collect::<Vec<_>>();
    let arr = NullTerminatedArray::new(&strs);
    let ptr = arr.get();
    unsafe {
        assert_eq!(CStr::from_ptr(*ptr).to_str().unwrap(), "foo");
        assert_eq!(CStr::from_ptr(*ptr.offset(1)).to_str().unwrap(), "bar");
        assert_eq!(*ptr.offset(2), ptr::null());
    }
}

#[test]
fn test_owning_null_terminated_array() {
    let owned_strs = vec![CString::new("foo").unwrap(), CString::new("bar").unwrap()];
    let arr = OwningNullTerminatedArray::new(owned_strs);
    let ptr = arr.get();
    unsafe {
        assert_eq!(CStr::from_ptr(*ptr).to_str().unwrap(), "foo");
        assert_eq!(CStr::from_ptr(*ptr.offset(1)).to_str().unwrap(), "bar");
        assert_eq!(*ptr.offset(2), ptr::null());
    }
}
