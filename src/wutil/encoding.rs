extern "C" {
    #[cfg_attr(cygwin, link_name = "c32rtomb")]
    pub fn wcrtomb(s: *mut libc::c_char, wc: u32, ps: *mut mbstate_t) -> usize;
    #[cfg_attr(cygwin, link_name = "mbrtoc32")]
    pub fn mbrtowc(pwc: *mut u32, s: *const libc::c_char, n: usize, p: *mut mbstate_t) -> usize;
}

// HACK This should be mbstate_t from libc but that's not exposed.  Since it's only written by
// libc, we define it as opaque type that should be large enough for all implementations.
pub type mbstate_t = [u64; 16];

#[inline]
pub fn zero_mbstate() -> mbstate_t {
    [0; 16]
}

// HACK This should be the MB_LEN_MAX macro from libc but that's not easy to get.
pub const AT_LEAST_MB_LEN_MAX: usize = 32;

/// Return true if we believe we are in a multibyte locale.
/// Note this reads the current locale and is modestly expensive - prefer the cached
/// values in `common.rs` which is set by `fish_setlocale`.
pub fn probe_is_multibyte_locale() -> bool {
    // In general we would like to read MB_CUR_MAX, but that is not exposed by Rust libc.
    // Instead, check if mbrtowc for any byte in the range 0-255 returns (size_t)(-2) which indicates
    // the presence of a multibyte locale.
    #[inline]
    fn is_mb_lead(b: u8) -> bool {
        let mut st = zero_mbstate();
        let mut wc: libc::wchar_t = 0;
        let c = b as libc::c_char;
        let n = unsafe {
            mbrtowc(
                std::ptr::addr_of_mut!(wc).cast::<u32>(),
                std::ptr::addr_of!(c),
                1,
                std::ptr::addr_of_mut!(st),
            )
        };
        n == (-2_i64 as libc::size_t)
    }

    // Fast path: check common lead bytes.
    if is_mb_lead(0xE0) || is_mb_lead(0xC2) {
        return true;
    }

    // Scan non-ASCII high bytes.
    (0x80_u8..=0xFF).any(is_mb_lead)
}
