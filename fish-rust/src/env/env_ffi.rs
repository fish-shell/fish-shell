use super::environment::{EnvDyn, EnvNull, EnvStack};
use super::var::{EnvVar, EnvVarFlags};
use crate::ffi::{wcharz_t, wcstring_list_ffi_t};
use crate::wchar_ffi::{AsWstr, WCharFromFFI};

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

        fn is_empty(self: &EnvVar) -> bool;

        fn exports(self: &EnvVar) -> bool;
        fn is_read_only(self: &EnvVar) -> bool;
        fn is_pathvar(self: &EnvVar) -> bool;

        #[cxx_name = "equals"]
        fn equals_ffi(self: &EnvVar, rhs: &EnvVar) -> bool;

        #[cxx_name = "as_string"]
        fn as_string_ffi(self: &EnvVar) -> UniquePtr<CxxWString>;

        #[cxx_name = "as_list"]
        fn as_list_ffi(self: &EnvVar) -> UniquePtr<wcstring_list_ffi_t>;

        #[cxx_name = "to_list"]
        fn to_list_ffi(self: &EnvVar, out: Pin<&mut wcstring_list_ffi_t>);

        #[cxx_name = "get_delimiter"]
        fn get_delimiter_ffi(self: &EnvVar) -> wchar_t;

        #[cxx_name = "get_flags"]
        fn get_flags_ffi(self: &EnvVar) -> u8;

        #[cxx_name = "clone_box"]
        fn clone_box_ffi(self: &EnvVar) -> Box<EnvVar>;

        #[cxx_name = "env_var_create"]
        fn env_var_create_ffi(vals: &wcstring_list_ffi_t, flags: u8) -> Box<EnvVar>;

        #[cxx_name = "env_var_create_from_name"]
        fn env_var_create_from_name_ffi(
            name: wcharz_t,
            values: &wcstring_list_ffi_t,
        ) -> Box<EnvVar>;

        type EnvNull;
        #[cxx_name = "getf"]
        fn getf_ffi(self: &EnvNull, name: &CxxWString, mode: u16) -> *mut EnvVar;
        #[cxx_name = "get_names"]
        fn get_names_ffi(self: &EnvNull, flags: u16, out: Pin<&mut wcstring_list_ffi_t>);
        #[cxx_name = "env_null_create"]
        fn env_null_create_ffi() -> Box<EnvNull>;

        type EnvStack;
        fn getf_ffi(self: &EnvStack, name: &CxxWString, mode: u16) -> *mut EnvVar;
        fn get_names_ffi(self: &EnvStack, flags: u16, out: Pin<&mut wcstring_list_ffi_t>);

        type EnvDyn;
        fn getf_ffi(self: &EnvDyn, name: &CxxWString, mode: u16) -> *mut EnvVar;
        fn get_names_ffi(self: &EnvDyn, flags: u16, out: Pin<&mut wcstring_list_ffi_t>);

    }
}
pub use env_ffi::EnvStackSetResult;

fn env_var_create_ffi(vals: &wcstring_list_ffi_t, flags: u8) -> Box<EnvVar> {
    Box::new(EnvVar::new_vec(
        vals.from_ffi(),
        EnvVarFlags::from_bits(flags).expect("invalid flags"),
    ))
}

pub fn env_var_create_from_name_ffi(name: wcharz_t, values: &wcstring_list_ffi_t) -> Box<EnvVar> {
    Box::new(EnvVar::new_from_name_vec(name.as_wstr(), values.from_ffi()))
}

fn env_null_create_ffi() -> Box<EnvNull> {
    Box::new(EnvNull::new())
}
