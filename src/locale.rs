/// Support for the "current locale."
pub use fish_printf::locale::{C_LOCALE, Locale};
use std::{ffi::CStr, sync::Mutex};

/// Lock guarding libc `setlocale()` or `localeconv()` calls to avoid races.
pub(crate) static LOCALE_LOCK: Mutex<()> = Mutex::new(());

/// # Safety
/// Call this either before starting any locale-using thread, or while holding a lock on the
/// above mutex.
pub unsafe fn set_libc_locales() -> Option<&'static CStr> {
    setlocale(libc::LC_ALL, Some(c""))
}

fn setlocale(category: libc::c_int, locale: Option<&CStr>) -> Option<&'static CStr> {
    let loc_ptr = {
        let locale = locale.map_or(std::ptr::null(), |loc| loc.as_ptr());
        unsafe { libc::setlocale(category, locale) }
    };
    (!loc_ptr.is_null()).then(||
        // Safety: setlocale did not return a null-pointer, so it is a valid pointer
        unsafe{CStr::from_ptr(loc_ptr)})
}

/// It's CHAR_MAX.
const CHAR_MAX: libc::c_char = libc::c_char::MAX;

/// Return the first character of a C string, or None if null, empty, has a length more than 1, or negative.
unsafe fn first_char(s: *const libc::c_char) -> Option<char> {
    unsafe {
        #[allow(unused_comparisons, clippy::absurd_extreme_comparisons)]
        if !s.is_null() && *s > 0 && *s <= 127 && *s.offset(1) == 0 {
            Some((*s as u8) as char)
        } else {
            None
        }
    }
}

/// Convert a libc lconv to a Locale.
unsafe fn lconv_to_locale(lconv: &libc::lconv) -> Locale {
    let decimal_point = unsafe { first_char(lconv.decimal_point).unwrap_or('.') };
    let thousands_sep = unsafe { first_char(lconv.thousands_sep) };
    let empty = &[0 as libc::c_char];

    // Up to 4 groups.
    // group_cursor is terminated by either a 0 or CHAR_MAX.
    let mut group_cursor = lconv.grouping as *const libc::c_char;
    if group_cursor.is_null() {
        group_cursor = empty.as_ptr();
    }

    let mut grouping = [0; 4];
    let mut last_group: u8 = 0;
    let mut group_repeat = false;
    for group in grouping.iter_mut() {
        let gc = unsafe { *group_cursor };
        if gc == 0 {
            // Preserve last_group, do not advance cursor.
            group_repeat = true;
        } else if gc == CHAR_MAX {
            // Remaining groups are 0, do not advance cursor.
            last_group = 0;
            group_repeat = false;
        } else {
            // Record last group, advance cursor.
            last_group = gc as u8;
            group_cursor = unsafe { group_cursor.offset(1) };
        }
        *group = last_group;
    }
    Locale {
        decimal_point,
        thousands_sep,
        grouping,
        group_repeat,
    }
}

/// Read the numeric locale, or None on any failure.
#[cfg(localeconv_l)]
unsafe fn read_locale() -> Option<Locale> {
    unsafe extern "C" {
        unsafe fn localeconv_l(loc: libc::locale_t) -> *const libc::lconv;
    }

    // We create a new locale (pass 0 locale_t base)
    // and pass no "locale", so everything else is taken from the environment.
    // This is fine because we're only using this for numbers.
    let loc = unsafe { libc::newlocale(libc::LC_NUMERIC_MASK, c"".as_ptr(), 0 as libc::locale_t) };
    if loc.is_null() {
        return None;
    }

    let lconv = unsafe { localeconv_l(loc) };
    let result = if lconv.is_null() {
        None
    } else {
        Some(unsafe { lconv_to_locale(&*lconv) })
    };

    unsafe { libc::freelocale(loc) };
    result
}

#[cfg(not(localeconv_l))]
unsafe fn read_locale() -> Option<Locale> {
    // Bleh, we have to go through localeconv, which races with setlocale.
    // TODO: There has to be a better way to do this.
    let _guard = LOCALE_LOCK.lock().unwrap();

    unsafe {
        libc::setlocale(libc::LC_NUMERIC, c"".as_ptr());
    }

    let lconv = unsafe { libc::localeconv() };
    let result = (!lconv.is_null()).then(|| unsafe { lconv_to_locale(&*lconv) });
    // Note we *always* use a C-locale for numbers, because we always want "." except for in printf.
    unsafe {
        libc::setlocale(libc::LC_NUMERIC, c"C".as_ptr());
    }
    result
}

// Current numeric locale.
static NUMERIC_LOCALE: Mutex<Option<Locale>> = Mutex::new(None);

pub fn get_numeric_locale() -> Locale {
    let mut locale = NUMERIC_LOCALE.lock().unwrap();
    if locale.is_none() {
        let new_locale = (unsafe { read_locale() }).unwrap_or(C_LOCALE);
        *locale = Some(new_locale);
    }
    locale.unwrap()
}

/// Invalidate the cached numeric locale.
pub fn invalidate_numeric_locale() {
    *NUMERIC_LOCALE.lock().unwrap() = None;
}
