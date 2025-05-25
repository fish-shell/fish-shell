use crate::{
    common::{str2wcstring, wcs2zstring},
    wchar::prelude::*,
    FLOG,
};
use errno::errno;
use std::{ffi::CString, fs::File, os::fd::FromRawFd};

// Replacement for mkostemp(str, O_CLOEXEC)
// This uses mkostemp if available,
// otherwise it uses mkstemp followed by fcntl
fn fish_mkstemp_cloexec(name_template: CString) -> std::io::Result<(File, CString)> {
    let name = name_template.into_raw();
    #[cfg(not(apple))]
    let fd = {
        use libc::O_CLOEXEC;
        unsafe { libc::mkostemp(name, O_CLOEXEC) }
    };
    #[cfg(apple)]
    let fd = {
        use libc::{FD_CLOEXEC, F_SETFD};
        let fd = unsafe { libc::mkstemp(name) };
        if fd != -1 {
            unsafe { libc::fcntl(fd, F_SETFD, FD_CLOEXEC) };
        }
        fd
    };
    if fd == -1 {
        Err(std::io::Error::from(errno()))
    } else {
        unsafe { Ok((File::from_raw_fd(fd), CString::from_raw(name))) }
    }
}

/// Creates a temporary file created according to the template and its name if successful.
pub fn create_temporary_file(name_template: &wstr) -> std::io::Result<(File, WString)> {
    let (fd, c_string_template) = loop {
        match fish_mkstemp_cloexec(wcs2zstring(name_template)) {
            Ok(tmp_file_data) => break tmp_file_data,
            Err(e) => match e.kind() {
                std::io::ErrorKind::Interrupted => {}
                _ => {
                    FLOG!(
                        error,
                        wgettext_fmt!(
                            "Unable to create temporary file '%ls': %s",
                            name_template,
                            e
                        )
                    );

                    return Err(e);
                }
            },
        }
    };
    Ok((fd, str2wcstring(c_string_template.to_bytes())))
}
