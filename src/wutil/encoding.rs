extern "C" {
    #[cfg_attr(cygwin, link_name = "c32rtomb")]
    pub fn wcrtomb(s: *mut libc::c_char, wc: u32, ps: *mut mbstate_t) -> usize;
    #[cfg_attr(cygwin, link_name = "mbrtoc32")]
    pub fn mbrtowc(pwc: *mut u32, s: *const libc::c_char, n: usize, p: *mut mbstate_t) -> usize;
}

// HACK This should be mbstate_t from libc but that's not exposed.  Since it's only written by
// libc, we define it as opaque type that should be large enough for all implementations.
pub type mbstate_t = [u64; 16];
pub fn zero_mbstate() -> mbstate_t {
    [0; 16]
}

// HACK This should be the MB_LEN_MAX macro from libc but that's not easy to get.
pub const AT_LEAST_MB_LEN_MAX: usize = 32;
