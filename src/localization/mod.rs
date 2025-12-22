mod gettext;
pub use gettext::{
    LocalizableString, initialize_gettext, localizable_consts, localizable_string, wgettext,
    wgettext_fmt,
};
#[cfg(feature = "localize-messages")]
mod settings;
#[cfg(feature = "localize-messages")]
pub use settings::{
    list_available_languages, status_language, unset_from_status_language_builtin, update_from_env,
    update_from_status_language_builtin,
};
