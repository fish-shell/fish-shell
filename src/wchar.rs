pub mod prelude {
    pub use crate::wutil::{
        LocalizableString, eprintf, localizable_consts, localizable_string, sprintf, wgettext,
        wgettext_fmt,
    };
    pub use fish_wchar::prelude::*;
}
