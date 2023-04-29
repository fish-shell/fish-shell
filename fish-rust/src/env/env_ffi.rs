use super::environment::{self, EnvDyn, EnvNull, EnvStack, EnvStackRef, Environment};
use super::var::{ElectricVar, EnvVar, EnvVarFlags, Statuses};
use crate::env::EnvMode;
use crate::event::Event;
use crate::ffi::{event_list_ffi_t, wchar_t, wcharz_t, wcstring_list_ffi_t};
use crate::null_terminated_array::OwningNullTerminatedArrayRefFFI;
use crate::signal::Signal;
use crate::wchar_ffi::WCharToFFI;
use crate::wchar_ffi::{AsWstr, WCharFromFFI};
use core::ffi::c_char;
use cxx::{CxxVector, CxxWString, UniquePtr};
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
        include!("env.h");
        include!("wutil.h");
        type event_list_ffi_t = super::event_list_ffi_t;
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
    extern "Rust" {
        type EnvNull;
        #[cxx_name = "getf"]
        fn getf_ffi(&self, name: &CxxWString, mode: u16) -> *mut EnvVar;
        #[cxx_name = "get_names"]
        fn get_names_ffi(&self, flags: u16, out: Pin<&mut wcstring_list_ffi_t>);
        #[cxx_name = "env_null_create"]
        fn env_null_create_ffi() -> Box<EnvNull>;
    }

    extern "Rust" {
        type Statuses;
        #[cxx_name = "get_status"]
        fn get_status_ffi(&self) -> i32;

        #[cxx_name = "get_pipestatus"]
        fn get_pipestatus_ffi(&self) -> &Vec<i32>;

        #[cxx_name = "get_kill_signal"]
        fn get_kill_signal_ffi(&self) -> i32;
    }

    extern "Rust" {
        #[cxx_name = "EnvDyn"]
        type EnvDynFFI;
        fn getf(&self, name: &CxxWString, mode: u16) -> *mut EnvVar;
        fn get_names(&self, flags: u16, out: Pin<&mut wcstring_list_ffi_t>);
    }

    extern "Rust" {
        #[cxx_name = "EnvStackRef"]
        type EnvStackRefFFI;
        fn getf(&self, name: &CxxWString, mode: u16) -> *mut EnvVar;
        fn get_names(&self, flags: u16, out: Pin<&mut wcstring_list_ffi_t>);
        fn is_principal(&self) -> bool;
        fn get_last_statuses(&self) -> Box<Statuses>;
        fn set_last_statuses(&self, status: i32, kill_signal: i32, pipestatus: &CxxVector<i32>);
        fn set(
            &self,
            name: &CxxWString,
            flags: u16,
            vals: &wcstring_list_ffi_t,
        ) -> EnvStackSetResult;
        fn remove(&self, name: &CxxWString, flags: u16) -> EnvStackSetResult;
        fn get_pwd_slash(&self) -> UniquePtr<CxxWString>;
        fn set_pwd_from_getcwd(&self);

        fn push(&mut self, new_scope: bool);
        fn pop(&mut self);

        // Returns a Box<OwningNullTerminatedArrayRefFFI>.into_raw() cast to a void*.
        // This is because we can't use the same C++ bindings to a Rust type from two different bridges.
        fn export_array(&self) -> *mut c_char;

        fn snapshot(&self) -> Box<EnvDynFFI>;

        // Access a variable stack that only represents globals.
        // Do not push or pop from this.
        fn env_get_globals_ffi() -> Box<EnvStackRefFFI>;

        // Access the principal variable stack.
        fn env_get_principal_ffi() -> Box<EnvStackRefFFI>;

        fn universal_sync(&self, always: bool, out_events: Pin<&mut event_list_ffi_t>);
    }

    extern "Rust" {
        #[cxx_name = "var_is_electric"]
        fn var_is_electric_ffi(name: &CxxWString) -> bool;

        #[cxx_name = "rust_env_init"]
        fn rust_env_init_ffi(do_uvars: bool);

        #[cxx_name = "env_flags_for"]
        fn env_flags_for_ffi(name: wcharz_t) -> u8;
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

fn env_null_create_ffi() -> Box<EnvNull> {
    Box::new(EnvNull::new())
}

/// FFI wrapper around EnvDyn
pub struct EnvDynFFI(EnvDyn);
impl EnvDynFFI {
    fn getf(&self, name: &CxxWString, mode: u16) -> *mut EnvVar {
        EnvironmentFFI::getf_ffi(&self.0, name, mode)
    }
    fn get_names(&self, flags: u16, out: Pin<&mut wcstring_list_ffi_t>) {
        EnvironmentFFI::get_names_ffi(&self.0, flags, out)
    }
}

/// FFI wrapper around EnvStackRef.
pub struct EnvStackRefFFI(EnvStackRef);

impl EnvStackRefFFI {
    fn getf(&self, name: &CxxWString, mode: u16) -> *mut EnvVar {
        EnvironmentFFI::getf_ffi(self.0.as_ref(), name, mode)
    }
    fn get_names(&self, flags: u16, out: Pin<&mut wcstring_list_ffi_t>) {
        EnvironmentFFI::get_names_ffi(self.0.as_ref(), flags, out)
    }
    fn is_principal(&self) -> bool {
        self.0.is_principal()
    }

    fn get_pwd_slash(&self) -> UniquePtr<CxxWString> {
        self.0.get_pwd_slash().to_ffi()
    }

    fn push(&self, new_scope: bool) {
        self.0.push(new_scope)
    }

    fn pop(&self) {
        self.0.pop()
    }

    fn get_last_statuses(&self) -> Box<Statuses> {
        Box::new(self.0.get_last_statuses())
    }

    fn set_last_statuses(&self, status: i32, kill_signal: i32, pipestatus: &CxxVector<i32>) {
        let statuses = Statuses {
            status,
            kill_signal: if kill_signal == 0 {
                None
            } else {
                Some(Signal::new(kill_signal))
            },
            pipestatus: pipestatus.as_slice().to_vec(),
        };
        self.0.set_last_statuses(statuses)
    }

    fn set_pwd_from_getcwd(&self) {
        self.0.set_pwd_from_getcwd()
    }

    fn set(&self, name: &CxxWString, flags: u16, vals: &wcstring_list_ffi_t) -> EnvStackSetResult {
        let mode = EnvMode::from_bits(flags).expect("Invalid mode bits");
        self.0.set(name.as_wstr(), mode, vals.from_ffi())
    }

    fn remove(&self, name: &CxxWString, flags: u16) -> EnvStackSetResult {
        let mode = EnvMode::from_bits(flags).expect("Invalid mode bits");
        self.0.remove(name.as_wstr(), mode)
    }

    fn export_array(&self) -> *mut c_char {
        Box::into_raw(Box::new(OwningNullTerminatedArrayRefFFI(
            self.0.export_array(),
        )))
        .cast()
    }

    fn snapshot(&self) -> Box<EnvDynFFI> {
        Box::new(EnvDynFFI(self.0.snapshot()))
    }

    fn universal_sync(
        self: &EnvStackRefFFI,
        always: bool,
        mut out_events: Pin<&mut event_list_ffi_t>,
    ) {
        let events: Vec<Box<Event>> = self.0.universal_sync(always);
        for event in events {
            out_events.as_mut().push(Box::into_raw(event).cast());
        }
    }
}

impl Statuses {
    fn get_status_ffi(&self) -> i32 {
        self.status
    }

    fn get_pipestatus_ffi(&self) -> &Vec<i32> {
        &self.pipestatus
    }

    fn get_kill_signal_ffi(&self) -> i32 {
        match self.kill_signal {
            Some(sig) => sig.code(),
            None => 0,
        }
    }
}

fn env_get_globals_ffi() -> Box<EnvStackRefFFI> {
    Box::new(EnvStackRefFFI(EnvStack::globals().clone()))
}

fn env_get_principal_ffi() -> Box<EnvStackRefFFI> {
    Box::new(EnvStackRefFFI(EnvStack::principal().clone()))
}

// We have to implement these directly to make cxx happy, even though they're implemented in the EnvironmentFFI trait.
impl EnvNull {
    pub fn getf_ffi(&self, name: &CxxWString, mode: u16) -> *mut EnvVar {
        EnvironmentFFI::getf_ffi(self, name, mode)
    }
    pub fn get_names_ffi(&self, flags: u16, out: Pin<&mut wcstring_list_ffi_t>) {
        EnvironmentFFI::get_names_ffi(self, flags, out)
    }
}

trait EnvironmentFFI: Environment {
    /// FFI helper.
    /// This returns either null, or the result of Box.into_raw().
    /// This is a workaround for the difficulty of passing an Option through FFI.
    fn getf_ffi(&self, name: &CxxWString, mode: u16) -> *mut EnvVar {
        match self.getf(
            name.as_wstr(),
            EnvMode::from_bits(mode).expect("Invalid mode bits"),
        ) {
            None => std::ptr::null_mut(),
            Some(var) => Box::into_raw(Box::new(var)),
        }
    }
    fn get_names_ffi(&self, mode: u16, mut out: Pin<&mut wcstring_list_ffi_t>) {
        let names = self.get_names(EnvMode::from_bits(mode).expect("Invalid mode bits"));
        for name in names {
            out.as_mut().push(name.to_ffi());
        }
    }
}

impl<T: Environment> EnvironmentFFI for T {}

fn var_is_electric_ffi(name: &CxxWString) -> bool {
    ElectricVar::for_name(name.as_wstr()).is_some()
}

fn rust_env_init_ffi(do_uvars: bool) {
    environment::env_init(do_uvars);
}

fn env_flags_for_ffi(name: wcharz_t) -> u8 {
    EnvVar::flags_for(name.as_wstr()).bits()
}
