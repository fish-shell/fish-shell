/// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().
pub mod flags {
    use autocxx::c_int;
    use bitflags::bitflags;

    bitflags! {
        /// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().
        #[repr(C)]
        pub struct EnvMode: u16 {
            /// Default mode. Used with `env_stack_t::get()` to indicate the caller doesn't care what scope
            /// the var is in or whether it is exported or unexported.
            const DEFAULT = 0;
            /// Flag for local (to the current block) variable.
            const LOCAL = 1 << 0;
            const FUNCTION = 1 << 1;
            /// Flag for global variable.
            const GLOBAL = 1 << 2;
            /// Flag for universal variable.
            const UNIVERSAL = 1 << 3;
            /// Flag for exported (to commands) variable.
            const EXPORT = 1 << 4;
            /// Flag for unexported variable.
            const UNEXPORT = 1 << 5;
            /// Flag to mark a variable as a path variable.
            const PATHVAR = 1 << 6;
            /// Flag to unmark a variable as a path variable.
            const UNPATHVAR = 1 << 7;
            /// Flag for variable update request from the user. All variable changes that are made directly
            /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
            /// serves one purpose: to indicate that an error should be returned if the user is attempting
            /// to modify a var that should not be modified by direct user action; e.g., a read-only var.
            const USER = 1 << 8;
        }
    }

    impl Into<c_int> for EnvMode {
        fn into(self) -> c_int {
            c_int(i32::from(self.bits()))
        }
    }
}

/// Return values for `env_stack_t::set()`.
pub mod status {
    pub const ENV_OK: i32 = 0;
    pub const ENV_PERM: i32 = 1;
    pub const ENV_SCOPE: i32 = 2;
    pub const ENV_INVALID: i32 = 3;
    pub const ENV_NOT_FOUND: i32 = 4;
}
