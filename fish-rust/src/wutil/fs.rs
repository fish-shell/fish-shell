use std::os::unix::prelude::OsStrExt;

use crate::common::str2wcstring;
use crate::wchar::WString;

/// Wide character version of getcwd().
pub fn wgetcwd() -> Result<WString, std::io::Error> {
    let cwd = std::env::current_dir()?;
    Ok(str2wcstring(cwd.as_os_str().as_bytes()))
}
