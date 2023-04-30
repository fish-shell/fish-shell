use super::var::{EnvVar, EnvVarFlags};
use crate::ffi::{wchar_t, wcharz_t, wcstring_list_ffi_t};
use crate::wchar_ffi::WCharToFFI;
use crate::wchar_ffi::{AsWstr, WCharFromFFI};
use cxx::{CxxWString, UniquePtr};
use std::pin::Pin;

#[allow(clippy::module_inception)]
#[cxx::bridge]
mod env_ffi {
    /// Return values for `EnvStack::set()`.
    #[repr(u8)]
    #[cxx_name = "env_stack_set_result_t"]
    enum EnvStackSetResult {
        ENV_OK,
        ENV_PERM,
        ENV_SCOPE,
        ENV_INVALID,
        ENV_NOT_FOUND,
    }

    extern "C++" {
        include!("wutil.h");
        type wcstring_list_ffi_t = super::wcstring_list_ffi_t;
        type wcharz_t = super::wcharz_t;
    }

    extern "Rust" {
        type EnvVar;

        fn is_empty(&self) -> bool;

        fn exports(&self) -> bool;
        fn is_read_only(&self) -> bool;
        fn is_pathvar(&self) -> bool;

        #[cxx_name = "equals"]
        fn equals_ffi(&self, rhs: &EnvVar) -> bool;

        #[cxx_name = "as_string"]
        fn as_string_ffi(&self) -> UniquePtr<CxxWString>;

        #[cxx_name = "as_list"]
        fn as_list_ffi(&self) -> UniquePtr<wcstring_list_ffi_t>;

        #[cxx_name = "to_list"]
        fn to_list_ffi(&self, out: Pin<&mut wcstring_list_ffi_t>);

        #[cxx_name = "get_delimiter"]
        fn get_delimiter_ffi(&self) -> wchar_t;

        #[cxx_name = "get_flags"]
        fn get_flags_ffi(&self) -> u8;

        #[cxx_name = "clone_box"]
        fn clone_box_ffi(&self) -> Box<EnvVar>;

        #[cxx_name = "env_var_create"]
        fn env_var_create_ffi(vals: &wcstring_list_ffi_t, flags: u8) -> Box<EnvVar>;

        #[cxx_name = "env_var_create_from_name"]
        fn env_var_create_from_name_ffi(
            name: wcharz_t,
            values: &wcstring_list_ffi_t,
        ) -> Box<EnvVar>;
    }
}
pub use env_ffi::EnvStackSetResult;

impl Default for EnvStackSetResult {
    fn default() -> Self {
        EnvStackSetResult::ENV_OK
    }
}

/// FFI bits.
impl EnvVar {
    pub fn equals_ffi(&self, rhs: &EnvVar) -> bool {
        self == rhs
    }

    pub fn as_string_ffi(&self) -> UniquePtr<CxxWString> {
        self.as_string().to_ffi()
    }

    pub fn as_list_ffi(&self) -> UniquePtr<wcstring_list_ffi_t> {
        self.as_list().to_ffi()
    }

    pub fn to_list_ffi(&self, mut out: Pin<&mut wcstring_list_ffi_t>) {
        out.as_mut().clear();
        for val in self.as_list() {
            out.as_mut().push(val);
        }
    }

    pub fn clone_box_ffi(&self) -> Box<Self> {
        Box::new(self.clone())
    }

    pub fn get_flags_ffi(&self) -> u8 {
        self.get_flags().bits()
    }

    pub fn get_delimiter_ffi(self: &EnvVar) -> wchar_t {
        self.get_delimiter().into()
    }
}

fn env_var_create_ffi(vals: &wcstring_list_ffi_t, flags: u8) -> Box<EnvVar> {
    Box::new(EnvVar::new_vec(
        vals.from_ffi(),
        EnvVarFlags::from_bits(flags).expect("invalid flags"),
    ))
}

pub fn env_var_create_from_name_ffi(name: wcharz_t, values: &wcstring_list_ffi_t) -> Box<EnvVar> {
    Box::new(EnvVar::new_from_name_vec(name.as_wstr(), values.from_ffi()))
}
