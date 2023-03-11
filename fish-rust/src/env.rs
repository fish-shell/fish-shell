/// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().
pub mod flags {
    use autocxx::c_int;

    /// Default mode. Used with `env_stack_t::get()` to indicate the caller doesn't care what scope
    /// the var is in or whether it is exported or unexported.
    pub const ENV_DEFAULT: c_int = c_int(0);
    /// Flag for local (to the current block) variable.
    pub const ENV_LOCAL: c_int = c_int(1 << 0);
    pub const ENV_FUNCTION: c_int = c_int(1 << 1);
    /// Flag for global variable.
    pub const ENV_GLOBAL: c_int = c_int(1 << 2);
    /// Flag for universal variable.
    pub const ENV_UNIVERSAL: c_int = c_int(1 << 3);
    /// Flag for exported (to commands) variable.
    pub const ENV_EXPORT: c_int = c_int(1 << 4);
    /// Flag for unexported variable.
    pub const ENV_UNEXPORT: c_int = c_int(1 << 5);
    /// Flag to mark a variable as a path variable.
    pub const ENV_PATHVAR: c_int = c_int(1 << 6);
    /// Flag to unmark a variable as a path variable.
    pub const ENV_UNPATHVAR: c_int = c_int(1 << 7);
    /// Flag for variable update request from the user. All variable changes that are made directly
    /// by the user, such as those from the `read` and `set` builtin must have this flag set. It
    /// serves one purpose: to indicate that an error should be returned if the user is attempting
    /// to modify a var that should not be modified by direct user action; e.g., a read-only var.
    pub const ENV_USER: c_int = c_int(1 << 8);
}

/// Return values for `env_stack_t::set()`.
pub mod status {
    pub const ENV_OK: i32 = 0;
    pub const ENV_PERM: i32 = 1;
    pub const ENV_SCOPE: i32 = 2;
    pub const ENV_INVALID: i32 = 3;
    pub const ENV_NOT_FOUND: i32 = 4;
}

#[repr(u16)]
pub enum EnvMode {
    Default = 0,
    Local = 1 << 0,
    Function = 1 << 1,
    Global = 1 << 2,
    Universal = 1 << 3,
    Export = 1 << 4,
    Unexport = 1 << 5,
    Pathvar = 1 << 6,
    Unpathvar = 1 << 7,
    User = 1 << 8,
}
