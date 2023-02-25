//! Interfaces for various FFI string types.
//!
//! We have the following string types for FFI purposes:
//!   - CxxWString: the Rust view of a C++ wstring.
//!   - W0String: an owning string with a nul terminator.
//!   - wcharz_t: a "newtyped" pointer to a nul-terminated string, implemented in C++.
//!               This is useful for FFI boundaries, to work around autocxx limitations on pointers.

use crate::ffi;
pub use cxx::CxxWString;
pub use ffi::{wchar_t, wcharz_t};
use once_cell::sync::Lazy;
pub use widestring::U32CString as W0String;
pub use widestring::{u32cstr, utf32str};
pub use widestring::{Utf32Str as wstr, Utf32String as WString};

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
        crate::wchar::wcharz_t {
            str_: crate::wchar_ffi::c_str!($string),
        }
    };
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
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString>;
}

/// WString may be converted to CxxWString.
impl WCharToFFI for WString {
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString> {
        cxx::CxxWString::create(self.as_char_slice())
    }
}

/// wstr (wide string slices) may be converted to CxxWString.
impl WCharToFFI for wstr {
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString> {
        cxx::CxxWString::create(self.as_char_slice())
    }
}

/// wcharz_t (wide char) may be converted to CxxWString.
impl WCharToFFI for wcharz_t {
    fn to_ffi(&self) -> cxx::UniquePtr<cxx::CxxWString> {
        cxx::CxxWString::create(self.chars())
    }
}

/// Convert from a CxxWString, in preparation for using over FFI.
pub trait WCharFromFFI<Target> {
    /// Convert from a CxxWString for FFI purposes.
    #[allow(clippy::wrong_self_convention)]
    fn from_ffi(&self) -> Target;
}

impl WCharFromFFI<WString> for cxx::CxxWString {
    fn from_ffi(&self) -> WString {
        WString::from_chars(self.as_chars())
    }
}

impl WCharFromFFI<WString> for cxx::UniquePtr<cxx::CxxWString> {
    fn from_ffi(&self) -> WString {
        WString::from_chars(self.as_chars())
    }
}

impl WCharFromFFI<WString> for cxx::SharedPtr<cxx::CxxWString> {
    fn from_ffi(&self) -> WString {
        WString::from_chars(self.as_chars())
    }
}

impl WCharFromFFI<Vec<u8>> for cxx::UniquePtr<cxx::CxxString> {
    fn from_ffi(&self) -> Vec<u8> {
        self.as_bytes().to_vec()
    }
}

impl WCharFromFFI<Vec<u8>> for cxx::SharedPtr<cxx::CxxString> {
    fn from_ffi(&self) -> Vec<u8> {
        self.as_bytes().to_vec()
    }
}
