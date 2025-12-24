mod gettext;
pub use gettext::{
    LocalizableString, localizable_consts, localizable_string, wgettext, wgettext_fmt,
};
#[cfg(feature = "localize-messages")]
mod settings;
#[cfg(feature = "localize-messages")]
pub use settings::{
    list_available_languages, status_language, unset_from_status_language_builtin, update_from_env,
    update_from_status_language_builtin,
};

#[cfg(feature = "localize-messages")]
/// This function only exists to provide a way for initializing gettext before an `EnvStack` is
/// available. Without this, early error messages cannot be localized.
pub fn initialize_localization() {
    use crate::env::EnvStack;
    use fish_wchar::L;

    let env = EnvStack::new();
    env_stack_set_from_env!(env, "LANGUAGE");
    env_stack_set_from_env!(env, "LC_ALL");
    env_stack_set_from_env!(env, "LC_MESSAGES");
    env_stack_set_from_env!(env, "LANG");
    update_from_env(&env);
}
