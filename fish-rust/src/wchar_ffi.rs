//! Interfaces for various FFI string types.
//!
//! We have the following string types for FFI purposes:
//!   - CxxWString: the Rust view of a C++ wstring.
//!   - W0String: an owning string with a nul terminator.
//!   - wcharz_t: a "newtyped" pointer to a nul-terminated string, implemented in C++.
//!               This is useful for FFI boundaries, to work around autocxx limitations on pointers.

pub use crate::ffi::{wchar_t, wcharz_t, wcstring_list_ffi_t, ToCppWString};
use crate::wchar::{wstr, WString};
use autocxx::WithinUniquePtr;
use once_cell::sync::Lazy;
pub use widestring::u32cstr;
pub use widestring::U32CString as W0String;

/// \return the length of a nul-terminated raw string.
pub fn wcslen(str: *const wchar_t) -> usize {
    assert!(!str.is_null(), "Null pointer");
    let mut len = 0;
    unsafe {
        while *str.offset(len) != 0 {
            len += 1;
        }
    }
    len as usize
}

impl wcharz_t {
    /// \return the chars of a wcharz_t.
    pub fn chars(&self) -> &[char] {
        assert!(!self.str_.is_null(), "Null wcharz");
        let data = self.str_ as *const char;
        let len = self.size();
        unsafe { std::slice::from_raw_parts(data, len) }
    }
}

/// W0String can be cheaply converted to a wcharz_t (but be mindful that W0String is kept alive).
impl From<&W0String> for wcharz_t {
    fn from(w0: &W0String) -> Self {
        wcharz_t {
            str_: w0.as_ptr() as *const wchar_t,
        }
    }
}

/// Convert wcharz_t to an WString.
impl From<&wcharz_t> for WString {
    fn from(wcharz: &wcharz_t) -> Self {
        WString::from_chars(wcharz.chars())
    }
}

/// Convert a wstr or WString to a W0String, which contains a nul-terminator.
/// This is useful for passing across FFI boundaries.
/// In general you don't need to use this directly - use the c_str macro below.
pub fn wstr_to_u32string<Str: AsRef<wstr>>(str: Str) -> W0String {
    W0String::from_ustr(str.as_ref()).expect("String contained intermediate NUL character")
}

/// Convert a wstr to a nul-terminated pointer.
/// This needs to be a macro so we can create a temporary with the proper lifetime.
macro_rules! c_str {
    ($string:expr) => {
        crate::wchar_ffi::wstr_to_u32string($string)
            .as_ucstr()
            .as_ptr()
            .cast::<crate::ffi::wchar_t>()
    };
}

/// Convert a wstr to a wcharz_t.
macro_rules! wcharz {
    ($string:expr) => {
        crate::wchar_ffi::wcharz_t {
            str_: crate::wchar_ffi::c_str!($string),
        }
    };
}

/// Convert a CxxVector of wcharz_t to a Vec<WString>.
pub fn wcharzs_to_vec(wcharz_vec: cxx::UniquePtr<cxx::CxxVector<wcharz_t>>) -> Vec<WString> {
    wcharz_vec
        .as_ref()
        .expect("UniquePtr was null")
        .iter()
        .map(|s| s.into())
        .collect()
}

pub(crate) use c_str;
pub(crate) use wcharz;

static EMPTY_WSTRING: Lazy<cxx::UniquePtr<cxx::CxxWString>> =
    Lazy::new(|| cxx::CxxWString::create(&[]));

/// \return a reference to a shared empty wstring.
pub fn empty_wstring() -> &'static cxx::CxxWString {
    &EMPTY_WSTRING
}

/// Implement Debug for wcharz_t.
impl std::fmt::Debug for wcharz_t {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.str_.is_null() {
            write!(f, "((null))")
        } else {
            self.chars().fmt(f)
        }
    }
}

/// Convert self to a CxxWString, in preparation for using over FFI.
/// We can't use "From" as WString is implemented in an external crate.
pub trait WCharToFFI {
    type Target;
    fn to_ffi(&self) -> Self::Target;
}

impl ToCppWString for &wstr {
    fn into_cpp(self) -> cxx::UniquePtr<cxx::CxxWString> {
        self.to_ffi()
    }
}

impl ToCppWString for WString {
    fn into_cpp(self) -> cxx::UniquePtr<cxx::CxxWString> {
        self.to_ffi()
    }
}

impl ToCppWString for &WString {
    fn into_cpp(self) -> cxx::UniquePtr<cxx::CxxWString> {
        self.to_ffi()
    }
}

/// WString may be converted to CxxWString.
impl WCharToFFI for WString {
    type Target = cxx::UniquePtr<cxx::CxxWString>;
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString> {
        cxx::CxxWString::create(self.as_char_slice())
    }
}

/// wstr (wide string slices) may be converted to CxxWString.
impl WCharToFFI for wstr {
    type Target = cxx::UniquePtr<cxx::CxxWString>;
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString> {
        cxx::CxxWString::create(self.as_char_slice())
    }
}

/// wcharz_t (wide char) may be converted to CxxWString.
impl WCharToFFI for wcharz_t {
    type Target = cxx::UniquePtr<cxx::CxxWString>;
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString> {
        cxx::CxxWString::create(self.chars())
    }
}

/// Convert from a slice of something that can be referenced as a wstr,
/// to unique_ptr<wcstring_list_ffi_t>.
impl<T: AsRef<wstr>> WCharToFFI for [T] {
    type Target = cxx::UniquePtr<wcstring_list_ffi_t>;
    fn to_ffi(&self) -> cxx::UniquePtr<wcstring_list_ffi_t> {
        let mut list_ptr = wcstring_list_ffi_t::create();
        for s in self {
            list_ptr.as_mut().unwrap().push(s.as_ref());
        }
        list_ptr
    }
}

/// Convert from a CxxWString, in preparation for using over FFI.
pub trait WCharFromFFI<Target> {
    /// Convert from a CxxWString for FFI purposes.
    #[allow(clippy::wrong_self_convention)]
    fn from_ffi(self) -> Target;
}

impl WCharFromFFI<WString> for &cxx::CxxWString {
    fn from_ffi(self) -> WString {
        WString::from_chars(self.as_chars())
    }
}

impl WCharFromFFI<WString> for &cxx::UniquePtr<cxx::CxxWString> {
    fn from_ffi(self) -> WString {
        WString::from_chars(self.as_chars())
    }
}

impl WCharFromFFI<WString> for &cxx::SharedPtr<cxx::CxxWString> {
    fn from_ffi(self) -> WString {
        WString::from_chars(self.as_chars())
    }
}

impl WCharFromFFI<Vec<u8>> for &cxx::UniquePtr<cxx::CxxString> {
    fn from_ffi(self) -> Vec<u8> {
        self.as_bytes().to_vec()
    }
}

impl WCharFromFFI<Vec<u8>> for &cxx::SharedPtr<cxx::CxxString> {
    fn from_ffi(self) -> Vec<u8> {
        self.as_bytes().to_vec()
    }
}

impl WCharFromFFI<WString> for &wcharz_t {
    fn from_ffi(self) -> WString {
        self.into()
    }
}

/// Convert wcstring_list_ffi_t to Vec<WString>.
impl WCharFromFFI<Vec<WString>> for &wcstring_list_ffi_t {
    fn from_ffi(self) -> Vec<WString> {
        let count: usize = self.size();
        (0..count).map(|i| self.at(i).from_ffi()).collect()
    }
}

// /// Convert std::vector<wcstring> to Vec<WString>.
// impl WCharFromFFI<Vec<WString>> for &cxx::CxxVector<cxx::CxxWString> {
//     fn from_ffi(self) -> Vec<WString> {
//         let count: usize = self.size();
//         (0..count).map(|i| self.at(i).from_ffi()).collect()
//     }
// }

/// Convert from the type we get back for C++ functions which return wcstring_list_ffi_t.
impl<T> WCharFromFFI<Vec<WString>> for T
where
    T: autocxx::moveit::new::New<Output = wcstring_list_ffi_t>,
{
    fn from_ffi(self) -> Vec<WString> {
        self.within_unique_ptr().as_ref().unwrap().from_ffi()
    }
}

/// Convert from FFI types to a reference to a wide string (i.e. a [`wstr`]) without allocating.
pub trait AsWstr<'a> {
    fn as_wstr(&'a self) -> &'a wstr;
}

impl<'a> AsWstr<'a> for cxx::UniquePtr<cxx::CxxWString> {
    fn as_wstr(&'a self) -> &'a wstr {
        wstr::from_char_slice(self.as_chars())
    }
}

impl<'a> AsWstr<'a> for cxx::CxxWString {
    fn as_wstr(&'a self) -> &'a wstr {
        wstr::from_char_slice(self.as_chars())
    }
}

impl AsWstr<'_> for wcharz_t {
    fn as_wstr(&self) -> &wstr {
        wstr::from_char_slice(self.chars())
    }
}

use crate::ffi_tests::add_test;
add_test!("test_wcstring_list_ffi_t", || {
    let data: Vec<WString> = wcstring_list_ffi_t::get_test_data().from_ffi();
    assert_eq!(data, vec!["foo", "bar", "baz"]);
    wcstring_list_ffi_t::check_test_data(data.to_ffi());
});
