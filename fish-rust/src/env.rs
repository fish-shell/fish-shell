// Flags that may be passed as the 'mode' in env_stack_t::set() / environment_t::get().

pub mod flags {
    use autocxx::c_int;

    /// Flag for universal variable.
    pub const ENV_UNIVERSAL: c_int = c_int(1 << 3);
}

/// Return values for `env_stack_t::set()`.
pub mod status {
    pub const ENV_OK: i32 = 0;
    pub const ENV_PERM: i32 = 1;
    pub const ENV_SCOPE: i32 = 2;
    pub const ENV_INVALID: i32 = 3;
    pub const ENV_NOT_FOUND: i32 = 4;
}
