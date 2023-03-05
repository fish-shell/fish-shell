use crate::ffi;
use crate::topic_monitor::{generation_t, invalid_generations, topic_monitor_principal, topic_t};
use crate::wchar::wstr;
use crate::wchar_ffi::c_str;
use widestring::U32CStr;

/// A sigint_detector_t can be used to check if a SIGINT (or SIGHUP) has been delivered.
pub struct sigchecker_t {
    topic: topic_t,
    gen: generation_t,
}

impl sigchecker_t {
    /// Create a new checker for the given topic.
    pub fn new(topic: topic_t) -> sigchecker_t {
        let mut res = sigchecker_t { topic, gen: 0 };
        // Call check() to update our generation.
        res.check();
        res
    }

    /// Create a new checker for SIGHUP and SIGINT.
    pub fn new_sighupint() -> sigchecker_t {
        Self::new(topic_t::sighupint)
    }

    /// Check if a sigint has been delivered since the last call to check(), or since the detector
    /// was created.
    pub fn check(&mut self) -> bool {
        let tm = topic_monitor_principal();
        let gen = tm.generation_for_topic(self.topic);
        let changed = self.gen != gen;
        self.gen = gen;
        changed
    }

    /// Wait until a sigint is delivered.
    pub fn wait(&self) {
        let tm = topic_monitor_principal();
        let mut gens = invalid_generations();
        *gens.at_mut(self.topic) = self.gen;
        tm.check(&mut gens, true /* wait */);
    }
}

/// Get the integer signal value representing the specified signal.
pub fn wcs2sig(s: &wstr) -> Option<usize> {
    let sig = ffi::wcs2sig(c_str!(s));

    sig.0.try_into().ok()
}

/// Get string representation of a signal.
pub fn sig2wcs(sig: usize) -> &'static wstr {
    let s = ffi::sig2wcs(i32::try_from(sig).expect("signal should be < 2^31").into());
    let s = unsafe { U32CStr::from_ptr_str(s) };

    wstr::from_ucstr(s).expect("signal name should be valid utf-32")
}

/// Returns a description of the specified signal.
pub fn signal_get_desc(sig: usize) -> &'static wstr {
    let s = ffi::signal_get_desc(i32::try_from(sig).expect("signal should be < 2^31").into());
    let s = unsafe { U32CStr::from_ptr_str(s) };

    wstr::from_ucstr(s).expect("signal description should be valid utf-32")
}
