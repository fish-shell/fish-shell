use super::ElectricVar;
use super::environment_impl::{
    EnvMutex, EnvMutexGuard, EnvScopedImpl, EnvStackImpl, ModResult, UVAR_SCOPE_IS_GLOBAL,
    colon_split, uvars,
};
use crate::abbrs::{Abbreviation, Position, abbrs_get_set};
use crate::builtins::shared::{BuiltinResult, SUCCESS};
use crate::common::{
    UnescapeStringStyle, cstr2wcstring, osstr2wcstring, str2wcstring, unescape_string,
};
use crate::env::config_paths::ConfigPaths;
use crate::env::{EnvMode, EnvSetMode, EnvVar, Statuses};
use crate::env_dispatch::{VarChangeMilieu, env_dispatch_init, env_dispatch_var_change};
use crate::event::Event;
use crate::flog::flog;
use crate::global_safety::RelaxedAtomicBool;
use crate::input::{FISH_BIND_MODE_VAR, init_input};
use crate::localization::wgettext;
use crate::nix::getpid;
use crate::null_terminated_array::OwningNullTerminatedArray;
use crate::path::{
    path_emit_config_directory_messages, path_get_cache, path_get_config, path_get_data,
    path_make_canonical, paths_are_same_file,
};
use crate::prelude::*;
use crate::proc::is_interactive_session;
use crate::termsize;
use crate::universal_notifier::default_notifier;
use crate::wutil::{fish_wcstol, wgetcwd};
use fish_wcstringutil::join_strings;
use libc::c_int;
use nix::{
    NixPath,
    unistd::{Uid, User, gethostname},
};
use std::collections::HashMap;
use std::ffi::CStr;
use std::path::PathBuf;
use std::sync::{Arc, LazyLock, OnceLock};

/// Set when a universal variable has been modified but not yet been written to disk via sync().
static UVARS_LOCALLY_MODIFIED: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Return values for `EnvStack::set()`.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum EnvStackSetResult {
    #[default]
    Ok, // The variable was set successfully.
    Perm,     // The variable is read-only.
    Scope,    // Variable cannot be set in the given scope.
    Invalid,  // The variable's value is invalid (e.g. umask).
    NotFound, // The variable was not found (only possible when removing a variable).
}

impl From<EnvStackSetResult> for BuiltinResult {
    fn from(r: EnvStackSetResult) -> Self {
        match r {
            EnvStackSetResult::Ok => Ok(SUCCESS),
            EnvStackSetResult::Perm => Err(1),
            EnvStackSetResult::Scope => Err(2),
            EnvStackSetResult::Invalid => Err(3),
            EnvStackSetResult::NotFound => Err(4),
        }
    }
}

/// An environment is read-only access to variable values.
pub trait Environment {
    /// Get a variable by name using default flags.
    fn get(&self, name: &wstr) -> Option<EnvVar> {
        self.getf(name, EnvMode::default())
    }

    /// Get a variable by name using the specified flags.
    fn getf(&self, name: &wstr, mode: EnvMode) -> Option<EnvVar>;

    /// Return the list of variable names.
    fn get_names(&self, flags: EnvMode) -> Vec<WString>;

    /// Returns PWD with a terminating slash.
    fn get_pwd_slash(&self) -> WString {
        // Return "/" if PWD is missing.
        // See https://github.com/fish-shell/fish-shell/issues/5080
        let Some(var) = self.get_unless_empty(L!("PWD")) else {
            return WString::from("/");
        };
        let mut pwd = var.as_string();
        if !pwd.ends_with('/') {
            pwd.push('/');
        }
        pwd
    }

    /// Get a variable by name using default flags, unless it is empty.
    fn get_unless_empty(&self, name: &wstr) -> Option<EnvVar> {
        self.getf_unless_empty(name, EnvMode::default())
    }

    /// Get a variable by name using the given flags, unless it is empty.
    fn getf_unless_empty(&self, name: &wstr, mode: EnvMode) -> Option<EnvVar> {
        let var = self.getf(name, mode)?;
        if !var.is_empty() {
            return Some(var);
        }
        None
    }
}

/// The null environment contains nothing.
pub struct EnvNull;

impl EnvNull {
    pub fn new() -> EnvNull {
        EnvNull
    }
}

impl Environment for EnvNull {
    fn getf(&self, _name: &wstr, _mode: EnvMode) -> Option<EnvVar> {
        None
    }

    fn get_names(&self, _flags: EnvMode) -> Vec<WString> {
        Vec::new()
    }
}

/// A helper type for wrapping a type-erased Environment.
pub struct EnvDyn {
    inner: Box<dyn Environment + Send + Sync>,
}

impl EnvDyn {
    fn new(inner: Box<dyn Environment + Send + Sync>) -> Self {
        Self { inner }
    }
}

impl Environment for EnvDyn {
    fn getf(&self, key: &wstr, mode: EnvMode) -> Option<EnvVar> {
        self.inner.getf(key, mode)
    }

    fn get_names(&self, flags: EnvMode) -> Vec<WString> {
        self.inner.get_names(flags)
    }

    fn get_pwd_slash(&self) -> WString {
        self.inner.get_pwd_slash()
    }
}

/// An immutable environment, used in snapshots.
pub struct EnvScoped {
    inner: EnvMutex<EnvScopedImpl>,
}

impl EnvScoped {
    fn from_impl(inner: EnvMutex<EnvScopedImpl>) -> EnvScoped {
        EnvScoped { inner }
    }

    fn lock(&self) -> EnvMutexGuard<'_, EnvScopedImpl> {
        self.inner.lock()
    }
}

/// A mutable environment which allows scopes to be pushed and popped.
/// This backs the parser's "vars".
pub struct EnvStack {
    inner: EnvMutex<EnvStackImpl>,
    can_push_pop: bool, // If false, panic on push/pop. Used for the global stack.
    dispatches_var_changes: bool, // controls whether we react to non-global variable changes, like to TZ
}

impl EnvStack {
    // Creates a new EnvStack which does not dispatch variable changes.
    pub fn new() -> EnvStack {
        EnvStack {
            inner: EnvStackImpl::new(),
            can_push_pop: true,
            dispatches_var_changes: false,
        }
    }

    // Create a "sub-stack" of the given stack.
    // This shares all nodes (variable scopes) with the parent stack.
    // can_push_pop is always set.
    pub fn create_child(&self, dispatches_var_changes: bool) -> EnvStack {
        let inner = EnvMutex::new(self.inner.lock().clone());
        EnvStack {
            inner,
            can_push_pop: true,
            dispatches_var_changes,
        }
    }

    fn lock(&self) -> EnvMutexGuard<'_, EnvStackImpl> {
        self.inner.lock()
    }

    /// Helpers to get and set the proc statuses.
    /// These correspond to $status and $pipestatus.
    pub fn get_last_statuses(&self) -> Statuses {
        self.lock().base.get_last_statuses().clone()
    }

    pub fn get_last_status(&self) -> c_int {
        self.lock().base.get_last_statuses().status
    }

    pub fn set_last_statuses(&self, statuses: Statuses) {
        self.lock().base.set_last_statuses(statuses);
    }

    /// Sets the variable with the specified name to the given values.
    pub fn set(&self, key: &wstr, mode: EnvSetMode, mut vals: Vec<WString>) -> EnvStackSetResult {
        // Historical behavior.
        if vals.len() == 1 && (key == "PWD" || key == "HOME") {
            path_make_canonical(vals.first_mut().unwrap());
        }

        // Hacky stuff around PATH and CDPATH: #3914.
        // Not MANPATH; see #4158.
        // Replace empties with dot. Note we ignore pathvar here.
        if key == "PATH" || key == "CDPATH" {
            // Split on colons.
            let mut munged_vals = colon_split(&vals);
            // Replace empties with dots.
            for val in munged_vals.iter_mut() {
                if val.is_empty() {
                    val.push('.');
                }
            }
            vals = munged_vals;
        }

        let ret: ModResult = self.lock().set(key, mode, vals);
        if ret.status == EnvStackSetResult::Ok {
            // Dispatch changes if we modified the global state or have 'dispatches_var_changes' set.
            // Important to not hold the lock here.
            if ret.global_modified || self.dispatches_var_changes {
                env_dispatch_var_change(
                    VarChangeMilieu {
                        is_repainting: mode.is_repainting,
                        global_or_universal: ret.global_modified || ret.uvar_modified,
                    },
                    key,
                    self,
                );
            }
        }
        // Mark if we modified a uvar.
        if ret.uvar_modified {
            UVARS_LOCALLY_MODIFIED.store(true);
        }
        ret.status
    }

    /// Sets the variable with the specified name to a single value.
    pub fn set_one(&self, key: &wstr, mode: EnvSetMode, val: WString) -> EnvStackSetResult {
        self.set(key, mode, vec![val])
    }

    /// Sets the variable with the specified name to no values.
    pub fn set_empty(&self, key: &wstr, mode: EnvSetMode) -> EnvStackSetResult {
        self.set(key, mode, Vec::new())
    }

    /// Update the PWD variable based on the result of getcwd.
    pub fn set_pwd_from_getcwd(&self) {
        let cwd = wgetcwd();
        if cwd.is_empty() {
            flog!(
                error,
                wgettext!(
                    "Could not determine current working directory. Is your locale set correctly?"
                )
            );
        }
        let global_exported_mode =
            EnvSetMode::new_at_early_startup(EnvMode::GLOBAL | EnvMode::EXPORT);
        self.set_one(L!("PWD"), global_exported_mode, cwd);
    }

    /// Remove environment variable.
    ///
    /// \param key The name of the variable to remove
    /// \param mode If this is a user request, read-only variables can not be removed. The mode
    /// may also specify the scope of the variable that should be erased.
    ///
    /// Return the set result.
    pub fn remove(&self, key: &wstr, mode: EnvSetMode) -> EnvStackSetResult {
        let ret = self.lock().remove(key, mode);
        #[allow(clippy::collapsible_if)]
        if ret.status == EnvStackSetResult::Ok {
            if ret.global_modified || self.dispatches_var_changes {
                // Important to not hold the lock here.
                env_dispatch_var_change(
                    VarChangeMilieu {
                        is_repainting: mode.is_repainting,
                        global_or_universal: ret.global_modified || ret.uvar_modified,
                    },
                    key,
                    self,
                );
            }
        }
        if ret.uvar_modified {
            UVARS_LOCALLY_MODIFIED.store(true);
        }
        ret.status
    }

    /// Push the variable stack. Used for implementing local variables for functions and for-loops.
    pub fn push(&self, new_scope: bool) {
        assert!(self.can_push_pop, "push/pop not allowed on global stack");
        let mut imp = self.lock();
        if new_scope {
            imp.push_shadowing();
        } else {
            imp.push_nonshadowing();
        }
    }

    /// Pop the variable stack. Used for implementing local variables for functions and for-loops.
    pub fn pop(&self, is_repainting: bool) {
        assert!(self.can_push_pop, "push/pop not allowed on global stack");
        let popped = self.lock().pop();
        if self.dispatches_var_changes {
            // TODO: we would like to coalesce locale changes, so that we only re-initialize
            // once.
            for key in popped {
                env_dispatch_var_change(
                    VarChangeMilieu {
                        is_repainting,
                        global_or_universal: false,
                    },
                    &key,
                    self,
                );
            }
        }
    }

    /// Returns an array containing all exported variables in a format suitable for execv.
    pub fn export_array(&self) -> Arc<OwningNullTerminatedArray> {
        self.lock().base.export_array()
    }

    /// Snapshot this environment. This means returning a read-only copy. Local variables are copied
    /// but globals are shared (i.e. changes to global will be visible to this snapshot).
    pub fn snapshot(&self) -> EnvDyn {
        let scoped = EnvScoped::from_impl(self.lock().base.snapshot());
        EnvDyn::new(Box::new(scoped) as Box<dyn Environment + Send + Sync>)
    }

    /// Synchronizes universal variable changes.
    /// If `always` is set, perform synchronization even if there's no pending changes from this
    /// instance (that is, look for changes from other fish instances).
    /// Return a list of events for changed variables.
    pub fn universal_sync(&self, always: bool, is_repainting: bool) -> Vec<Event> {
        if UVAR_SCOPE_IS_GLOBAL.load() {
            return Vec::new();
        }
        if !always && !UVARS_LOCALLY_MODIFIED.load() {
            return Vec::new();
        }
        UVARS_LOCALLY_MODIFIED.store(false);

        let (changed, callbacks) = uvars().sync();
        if changed {
            default_notifier().post_notification();
        }
        // React internally to changes to special variables like LANG, and populate on-variable events.
        let mut result = Vec::new();
        if let Some(callbacks) = callbacks {
            for callback in callbacks {
                let name = callback.key;
                env_dispatch_var_change(
                    VarChangeMilieu {
                        is_repainting,
                        global_or_universal: true,
                    },
                    &name,
                    self,
                );
                let evt = if callback.val.is_none() {
                    Event::variable_erase(name)
                } else {
                    Event::variable_set(name)
                };
                result.push(evt);
            }
        }
        result
    }

    /// A variable stack that only represents globals.
    /// Do not push or pop from this.
    pub fn globals() -> &'static EnvStack {
        use std::sync::OnceLock;
        static GLOBALS: OnceLock<EnvStack> = OnceLock::new();
        GLOBALS.get_or_init(|| EnvStack {
            inner: EnvStackImpl::new(),
            can_push_pop: false,
            // Do not dispatch variable changes - this is used at startup when we are importing env vars.
            dispatches_var_changes: false,
        })
    }

    pub fn set_argv(&self, argv: Vec<WString>, is_repainting: bool) {
        self.set(
            L!("argv"),
            EnvSetMode::new(EnvMode::LOCAL, is_repainting),
            argv,
        );
    }
}

impl Environment for EnvScoped {
    fn getf(&self, key: &wstr, mode: EnvMode) -> Option<EnvVar> {
        self.lock().getf(key, mode)
    }

    fn get_names(&self, flags: EnvMode) -> Vec<WString> {
        self.lock().get_names(flags)
    }

    fn get_pwd_slash(&self) -> WString {
        self.lock().get_pwd_slash()
    }
}

/// Necessary for [`Arc<EnvStack>`] to be sync.
/// Safety: again, the global lock.
unsafe impl Send for EnvStack {}

impl Environment for EnvStack {
    fn getf(&self, key: &wstr, mode: EnvMode) -> Option<EnvVar> {
        self.lock().getf(key, mode)
    }

    fn get_names(&self, flags: EnvMode) -> Vec<WString> {
        self.lock().get_names(flags)
    }

    fn get_pwd_slash(&self) -> WString {
        self.lock().get_pwd_slash()
    }
}

/// Some configuration path environment variables.
const FISH_DATADIR_VAR: &wstr = L!("__fish_data_dir");
const FISH_SYSCONFDIR_VAR: &wstr = L!("__fish_sysconf_dir");
const FISH_HELPDIR_VAR: &wstr = L!("__fish_help_dir");
const FISH_MANDIR_VAR: &wstr = L!("__fish_man_dir");
const FISH_BIN_DIR: &wstr = L!("__fish_bin_dir");
const FISH_CONFIG_DIR: &wstr = L!("__fish_config_dir");
const FISH_USER_DATA_DIR: &wstr = L!("__fish_user_data_dir");
const FISH_CACHE_DIR: &wstr = L!("__fish_cache_dir");

/// Set up the USER and HOME variable.
fn setup_user(global_exported_mode: EnvSetMode, vars: &EnvStack) {
    let uid = Uid::effective();
    let user_var = vars.get_unless_empty(L!("USER"));

    // If we have a $USER, we try to get the passwd entry for the name.
    // If that has the same UID that we use, we assume the data is correct.
    if let Some(user_var) = user_var {
        // POSIX.1-2017 requires that user names are a subset of ASCII, so converting to String here
        // is ok.
        // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_437
        // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_282
        if let Ok(Some(userinfo)) = User::from_name(&user_var.as_string().to_string()) {
            if userinfo.uid == uid {
                // The uid matches but we still might need to set $HOME.
                if vars.get_unless_empty(L!("HOME")).is_none() {
                    if !userinfo.dir.is_empty() {
                        vars.set_one(
                            L!("HOME"),
                            global_exported_mode,
                            osstr2wcstring(userinfo.dir),
                        );
                    } else {
                        vars.set_empty(L!("HOME"), global_exported_mode);
                    }
                }
                return;
            }
        }
    }

    if let Ok(Some(userinfo)) = User::from_uid(uid) {
        let uname = str2wcstring(userinfo.name);
        vars.set_one(L!("USER"), global_exported_mode, uname);
        // Only change $HOME if it's empty, so we allow e.g. `HOME=(mktemp -d)`.
        // This is okay with common `su` and `sudo` because they set $HOME.
        if vars.get_unless_empty(L!("HOME")).is_none() {
            if !userinfo.dir.is_empty() {
                vars.set_one(
                    L!("HOME"),
                    global_exported_mode,
                    osstr2wcstring(userinfo.dir),
                );
            } else {
                // We cannot get $HOME. This triggers warnings for history and config.fish already,
                // so it isn't necessary to warn here as well.
                vars.set_empty(L!("HOME"), global_exported_mode);
            }
        }
    } else if vars.get_unless_empty(L!("HOME")).is_none() {
        // If $USER is empty as well (which we tried to set above), we can't get $HOME.
        vars.set_empty(L!("HOME"), global_exported_mode);
    }
}

pub(crate) static FALLBACK_PATH: LazyLock<&[WString]> = LazyLock::new(|| {
    // _CS_PATH: colon-separated paths to find POSIX utilities. Same as USER_CS_PATH.
    let cs_path = libc::_CS_PATH;

    let buf_size = unsafe { libc::confstr(cs_path, std::ptr::null_mut(), 0) };
    let paths: Vec<WString> = if buf_size > 0 {
        let mut buf = vec![b'\0' as libc::c_char; buf_size];
        unsafe { libc::confstr(cs_path, buf.as_mut_ptr(), buf_size) };
        let buf = buf;
        // safety: buf should contain a null-byte, and is not mutable unless we move ownership
        let cstr = unsafe { CStr::from_ptr(buf.as_ptr()) };
        colon_split(&[cstr2wcstring(cstr)])
    } else {
        vec![
            str2wcstring(env!("PREFIX")) + L!("/bin"),
            L!("/usr/bin").to_owned(),
            L!("/bin").to_owned(),
        ]
    };
    Box::leak(paths.into_boxed_slice())
});

/// Make sure the PATH variable contains something.
fn setup_path(global_exported_mode: EnvSetMode) {
    let vars = EnvStack::globals();
    let path = vars.get_unless_empty(L!("PATH"));
    if path.is_none() {
        vars.set(L!("PATH"), global_exported_mode, FALLBACK_PATH.to_vec());
    }
}

/// The originally inherited variables and their values.
/// This is a simple key->value map and not e.g. cut into paths.
pub static INHERITED_VARS: OnceLock<HashMap<WString, WString>> = OnceLock::new();

pub fn env_init(paths: Option<&ConfigPaths>, do_uvars: bool, default_paths: bool) {
    let vars = EnvStack::globals();

    let global_mode = EnvSetMode::new_at_early_startup(EnvMode::GLOBAL);
    let global_exported_mode = EnvSetMode::new_at_early_startup(EnvMode::GLOBAL | EnvMode::EXPORT);

    let env_iter: Vec<_> = std::env::vars_os()
        .map(|(k, v)| (osstr2wcstring(k), osstr2wcstring(v)))
        .collect();

    let mut inherited_vars = HashMap::new();
    // Import environment variables. Walk backwards so that the first one out of any duplicates wins
    // (See issue #2784).
    // PORTING: this should behave like in C++ in regards to the above comment, but that might be
    // with assumptions on the standard library's behavior (is it okay for them to remove duplicates?).
    for (key, val) in env_iter.into_iter().rev() {
        // PORTING: This is making assumptions that env::vars_os set val to empty if no equal sign
        // PORTING: That assumption appears to be wrong https://github.com/rust-lang/rust/blob/2ceed0b6cb9e9866225d7cfcfcbb4a62db047163/library/std/src/sys/unix/os.rs#L584C30-L584C30
        // it appears they allow names starting with =, but do not turn malformed lines
        // into the variable name with an empty value
        if ElectricVar::for_name(&key).is_none() {
            // fish_user_paths should not be exported; attempting to re-import it from
            // a value we previously (due to user error) exported will cause impossibly
            // difficult to debug PATH problems.
            if key != "fish_user_paths" {
                vars.set(&key, global_exported_mode, vec![val.clone()]);
            }
        }
        inherited_vars.insert(key, val);
    }

    INHERITED_VARS
        .set(inherited_vars)
        .expect("env_init is being called multiple times");

    // Set $USER, $HOME and $EUID
    // This involves going to passwd and stuff.
    vars.set_one(
        L!("EUID"),
        global_mode,
        Uid::effective().as_raw().to_wstring(),
    );
    setup_user(global_exported_mode, vars);

    if let Some(paths) = paths {
        let set_path = |key: &wstr, maybe_path: Option<&PathBuf>| {
            vars.set(
                key,
                global_mode,
                maybe_path
                    .map(|path| vec![osstr2wcstring(path)])
                    .unwrap_or_default(),
            );
        };

        set_path(FISH_SYSCONFDIR_VAR, Some(&paths.sysconf));
        set_path(FISH_BIN_DIR, paths.bin.as_ref());

        set_path(FISH_DATADIR_VAR, paths.data.as_ref());
        set_path(FISH_MANDIR_VAR, paths.man.as_ref());
        set_path(FISH_HELPDIR_VAR, paths.doc.as_ref());
    }

    let user_config_dir = path_get_config();
    vars.set_one(
        FISH_CONFIG_DIR,
        global_mode,
        user_config_dir.unwrap_or_default(),
    );

    let user_data_dir = path_get_data();
    vars.set_one(
        FISH_USER_DATA_DIR,
        global_mode,
        user_data_dir.unwrap_or_default(),
    );

    let user_cache_dir = path_get_cache();
    vars.set_one(
        FISH_CACHE_DIR,
        global_mode,
        user_cache_dir.unwrap_or_default(),
    );
    // Set up a default PATH
    setup_path(global_exported_mode);

    // Set up $IFS - this used to be in share/config.fish, but really breaks if it isn't done.
    vars.set_one(L!("IFS"), global_mode, "\n \t".into());

    // Ensure this var is present even before an interactive command is run so that if it is used
    // in a function like `fish_prompt` or `fish_right_prompt` it is defined at the time the first
    // prompt is written.
    vars.set_one(
        L!("CMD_DURATION"),
        EnvSetMode::new_at_early_startup(EnvMode::UNEXPORT),
        "0".into(),
    );

    // Set up the version variable.
    let version = str2wcstring(crate::BUILD_VERSION);
    vars.set_one(L!("version"), global_mode, version.clone());
    vars.set_one(L!("FISH_VERSION"), global_mode, version);

    // Set the $fish_pid variable.
    vars.set_one(L!("fish_pid"), global_mode, getpid().to_wstring());

    // Set the $hostname variable
    let hostname: WString = gethostname().map_or("fish".into(), osstr2wcstring);
    vars.set_one(L!("hostname"), global_mode, hostname);

    // Set up SHLVL variable. Not we can't use vars.get() because SHLVL is read-only, and therefore
    // was not inherited from the environment.
    if is_interactive_session() {
        let nshlvl_str = if let Some(shlvl_var) = std::env::var_os("SHLVL") {
            // TODO: Figure out how to handle invalid numbers better. Shouldn't we issue a
            // diagnostic?
            match fish_wcstol(&osstr2wcstring(shlvl_var)) {
                Ok(shlvl_i) if shlvl_i >= 0 => (shlvl_i + 1).to_wstring(),
                _ => L!("1").to_owned(),
            }
        } else {
            L!("1").to_owned()
        };
        vars.set_one(L!("SHLVL"), global_exported_mode, nshlvl_str);
    } else {
        // If we're not interactive, simply pass the value along.
        if let Some(shlvl_var) = std::env::var_os("SHLVL") {
            vars.set_one(L!("SHLVL"), global_exported_mode, osstr2wcstring(shlvl_var));
        }
    }

    // initialize the PWD variable if necessary
    // Note we may inherit a virtual PWD that doesn't match what getcwd would return; respect that
    // if and only if it matches getcwd (#5647). Note we treat PWD as read-only so it was not set in
    // vars.
    //
    // Also reject all paths that don't start with "/", this includes windows paths like "F:\foo".
    // (see #7636)
    let incoming_pwd_cstr = std::env::var_os("PWD");
    let incoming_pwd = incoming_pwd_cstr.map(osstr2wcstring).unwrap_or_default();
    if !incoming_pwd.is_empty()
        && incoming_pwd.char_at(0) == '/'
        && paths_are_same_file(&incoming_pwd, L!("."))
    {
        vars.set_one(L!("PWD"), global_exported_mode, incoming_pwd);
    } else {
        vars.set_pwd_from_getcwd();
    }

    // Initialize termsize variables.
    let termsize = termsize::SHARED_CONTAINER.initialize(vars as &dyn Environment);
    if vars.get_unless_empty(L!("COLUMNS")).is_none() {
        vars.set_one(L!("COLUMNS"), global_mode, termsize.width().to_wstring());
    }
    if vars.get_unless_empty(L!("LINES")).is_none() {
        vars.set_one(L!("LINES"), global_mode, termsize.height().to_wstring());
    }

    // Set fish_bind_mode to "default".
    vars.set_one(FISH_BIND_MODE_VAR, global_mode, "default".into());

    // Allow changes to variables to produce events.
    env_dispatch_init(vars);

    init_input();

    // Complain about invalid config paths.
    // HACK: Assume the defaults are correct (in practice this is only --no-config anyway).
    if !default_paths {
        path_emit_config_directory_messages(vars);
    }

    if !do_uvars {
        UVAR_SCOPE_IS_GLOBAL.store(true);
        return;
    }

    // Set up universal variables using the default path.
    let callbacks = uvars().initialize().unwrap_or_default();
    for callback in callbacks {
        env_dispatch_var_change(
            VarChangeMilieu {
                is_repainting: false,
                global_or_universal: true,
            },
            &callback.key,
            vars,
        );
    }

    // Do not import variables that have the same name and value as
    // an exported universal variable. See issues #5258 and #5348.
    let globals_to_skip = {
        let mut to_skip = vec![];
        let uvars_locked = uvars();
        for (name, uvar) in uvars_locked.get_table() {
            if !uvar.exports() {
                continue;
            }

            // Look for a global exported variable with the same name.
            let global = EnvStack::globals().getf(name, EnvMode::GLOBAL | EnvMode::EXPORT);
            if global.is_some_and(|x| x.as_string() == uvar.as_string()) {
                to_skip.push(name.to_owned());
            }
        }
        to_skip
    };
    for name in &globals_to_skip {
        EnvStack::globals().remove(name, global_exported_mode);
    }

    // Import any abbreviations from uvars.
    // Note we do not dynamically react to changes.
    let prefix = L!("_fish_abbr_");
    let prefix_len = prefix.char_count();
    let from_universal = true;
    let mut abbrs = abbrs_get_set();
    let uvars_locked = uvars();
    for (name, uvar) in uvars_locked.get_table() {
        if !name.starts_with(prefix) {
            continue;
        }
        let escaped_name = name.slice_from(prefix_len);
        if let Some(name) = unescape_string(escaped_name, UnescapeStringStyle::Var) {
            let key = name.clone();
            let replacement: WString = join_strings(uvar.as_list(), ' ');
            abbrs.add(Abbreviation::new(
                name,
                key,
                replacement,
                Position::Command,
                from_universal,
            ));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{EnvMode, EnvStack, Environment};
    use crate::env::EnvSetMode;
    use crate::prelude::*;
    use crate::tests::prelude::*;

    #[test]
    #[serial]
    fn test_env_snapshot() {
        let _cleanup = test_init();
        std::fs::create_dir_all("test/fish_env_snapshot_test/").unwrap();
        let parser = TestParser::new();
        let vars = parser.vars();
        parser.pushd("test/fish_env_snapshot_test/");
        vars.push(true);
        let before_pwd = vars.get(L!("PWD")).unwrap().as_string();
        vars.set_one(
            L!("test_env_snapshot_var"),
            EnvSetMode::default(),
            L!("before").to_owned(),
        );
        let snapshot = vars.snapshot();
        vars.set_one(L!("PWD"), EnvSetMode::default(), L!("/newdir").to_owned());
        vars.set_one(
            L!("test_env_snapshot_var"),
            EnvSetMode::default(),
            L!("after").to_owned(),
        );
        vars.set_one(
            L!("test_env_snapshot_var_2"),
            EnvSetMode::default(),
            L!("after").to_owned(),
        );

        // vars should be unaffected by the snapshot
        assert_eq!(vars.get(L!("PWD")).unwrap().as_string(), L!("/newdir"));
        assert_eq!(
            vars.get(L!("test_env_snapshot_var")).unwrap().as_string(),
            L!("after")
        );
        assert_eq!(
            vars.get(L!("test_env_snapshot_var_2")).unwrap().as_string(),
            L!("after")
        );

        // snapshot should have old values of vars
        assert_eq!(snapshot.get(L!("PWD")).unwrap().as_string(), before_pwd);
        assert_eq!(
            snapshot
                .get(L!("test_env_snapshot_var"))
                .unwrap()
                .as_string(),
            L!("before")
        );
        assert_eq!(snapshot.get(L!("test_env_snapshot_var_2")), None);

        // snapshots see global var changes except for perproc like PWD
        vars.set_one(
            L!("test_env_snapshot_var_3"),
            EnvSetMode::new(EnvMode::GLOBAL, false),
            L!("reallyglobal").to_owned(),
        );
        assert_eq!(
            vars.get(L!("test_env_snapshot_var_3")).unwrap().as_string(),
            L!("reallyglobal")
        );
        assert_eq!(
            snapshot
                .get(L!("test_env_snapshot_var_3"))
                .unwrap()
                .as_string(),
            L!("reallyglobal")
        );

        vars.pop(false);
        parser.popd();
    }

    // Can't push/pop from globals.
    #[test]
    #[should_panic]
    fn test_no_global_push() {
        EnvStack::globals().push(true);
    }

    #[test]
    #[should_panic]
    fn test_no_global_pop() {
        EnvStack::globals().pop(false);
    }
}
