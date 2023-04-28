#if 0
// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include "env.h"

#include <errno.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include "abbrs.h"
#include "common.h"
#include "env_dispatch.h"
#include "env_universal_common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
#include "flog.h"
#include "global_safety.h"
#include "history.h"
#include "input.h"
#include "kill.h"
#include "null_terminated_array.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "termsize.h"
#include "threads.rs.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

/// Some configuration path environment variables.
#define FISH_DATADIR_VAR L"__fish_data_dir"
#define FISH_SYSCONFDIR_VAR L"__fish_sysconf_dir"
#define FISH_HELPDIR_VAR L"__fish_help_dir"
#define FISH_BIN_DIR L"__fish_bin_dir"
#define FISH_CONFIG_DIR L"__fish_config_dir"
#define FISH_USER_DATA_DIR L"__fish_user_data_dir"

/// At init, we read all the environment variables from this array.
extern char **environ;

bool curses_initialized = false;

/// Does the terminal have the "eat_newline_glitch".
bool term_has_xn = false;

// static
env_var_t env_var_t::new_ffi(EnvVar *ptr) {
    assert(ptr != nullptr && "env_var_t::new_ffi called with null pointer");
    return env_var_t(rust::Box<EnvVar>::from_raw(ptr));
}

wchar_t env_var_t::get_delimiter() const { return impl_->get_delimiter(); }

bool env_var_t::empty() const { return impl_->is_empty(); }
bool env_var_t::exports() const { return impl_->exports(); }
bool env_var_t::is_read_only() const { return impl_->is_read_only(); }
bool env_var_t::is_pathvar() const { return impl_->is_pathvar(); }
env_var_t::env_var_flags_t env_var_t::get_flags() const { return impl_->get_flags(); }

wcstring env_var_t::as_string() const {
    wcstring res = std::move(*impl_->as_string());
    return res;
}

void env_var_t::to_list(std::vector<wcstring> &out) const {
    wcstring_list_ffi_t list{};
    impl_->to_list(list);
    out = std::move(list.vals);
}

std::vector<wcstring> env_var_t::as_list() const {
    std::vector<wcstring> res = std::move(impl_->as_list()->vals);
    return res;
}

env_var_t &env_var_t::operator=(const env_var_t &rhs) {
    this->impl_ = rhs.impl_->clone_box();
    return *this;
}

env_var_t::env_var_t(const wcstring_list_ffi_t &vals, uint8_t flags)
    : impl_(env_var_create(vals, flags)) {}

env_var_t::env_var_t(const env_var_t &rhs) : impl_(rhs.impl_->clone_box()) {}

bool env_var_t::operator==(const env_var_t &rhs) const { return impl_->equals(*rhs.impl_); }

environment_t::~environment_t() = default;

env_var_t::env_var_flags_t env_var_t::flags_for(const wchar_t *name) { return env_flags_for(name); }

wcstring environment_t::get_pwd_slash() const {
    // Return "/" if PWD is missing.
    // See https://github.com/fish-shell/fish-shell/issues/5080
    auto pwd_var = get_unless_empty(L"PWD");
    wcstring pwd;
    if (pwd_var) {
        pwd = pwd_var->as_string();
    }
    if (!string_suffixes_string(L"/", pwd)) {
        pwd.push_back(L'/');
    }
    return pwd;
}

maybe_t<env_var_t> environment_t::get_unless_empty(const wcstring &key,
                                                   env_mode_flags_t mode) const {
    if (auto variable = this->get(key, mode)) {
        if (!variable->empty()) {
            return variable;
        }
    }
    return none();
}

std::unique_ptr<env_var_t> environment_t::get_or_null(wcstring const &key,
                                                      env_mode_flags_t mode) const {
    auto variable = this->get(key, mode);
    if (!variable.has_value()) {
        return nullptr;
    }
    return make_unique<env_var_t>(variable.acquire());
}

null_environment_t::null_environment_t() : impl_(env_null_create()) {}
null_environment_t::~null_environment_t() = default;

maybe_t<env_var_t> null_environment_t::get(const wcstring &key, env_mode_flags_t mode) const {
    if (auto *ptr = impl_->getf(key, mode)) {
        return env_var_t::new_ffi(ptr);
    }
    return none();
}

std::vector<wcstring> null_environment_t::get_names(env_mode_flags_t flags) const {
    wcstring_list_ffi_t names;
    impl_->get_names(flags, names);
    return std::move(names.vals);
}

/// Set up the USER and HOME variable.
static void setup_user(env_stack_t &vars) {
    auto uid = geteuid();
    auto user_var = vars.get_unless_empty(L"USER");
    struct passwd userinfo;
    struct passwd *result;
    char buf[8192];

    // If we have a $USER, we try to get the passwd entry for the name.
    // If that has the same UID that we use, we assume the data is correct.
    if (user_var) {
        std::string unam_narrow = wcs2zstring(user_var->as_string());
        int retval = getpwnam_r(unam_narrow.c_str(), &userinfo, buf, sizeof(buf), &result);
        if (!retval && result) {
            if (result->pw_uid == uid) {
                // The uid matches but we still might need to set $HOME.
                if (!vars.get_unless_empty(L"HOME")) {
                    if (userinfo.pw_dir) {
                        vars.set_one(L"HOME", ENV_GLOBAL | ENV_EXPORT,
                                     str2wcstring(userinfo.pw_dir));
                    } else {
                        vars.set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
                    }
                }
                return;
            }
        }
    }

    // Either we didn't have a $USER or it had a different uid.
    // We need to get the data *again* via the uid.
    int retval = getpwuid_r(uid, &userinfo, buf, sizeof(buf), &result);
    if (!retval && result) {
        const wcstring uname = str2wcstring(userinfo.pw_name);
        vars.set_one(L"USER", ENV_GLOBAL | ENV_EXPORT, uname);
        // Only change $HOME if it's empty, so we allow e.g. `HOME=(mktemp -d)`.
        // This is okay with common `su` and `sudo` because they set $HOME.
        if (!vars.get_unless_empty(L"HOME")) {
            if (userinfo.pw_dir) {
                vars.set_one(L"HOME", ENV_GLOBAL | ENV_EXPORT, str2wcstring(userinfo.pw_dir));
            } else {
                // We cannot get $HOME. This triggers warnings for history and config.fish already,
                // so it isn't necessary to warn here as well.
                vars.set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
            }
        }
    } else if (!vars.get_unless_empty(L"HOME")) {
        // If $USER is empty as well (which we tried to set above), we can't get $HOME.
        vars.set_empty(L"HOME", ENV_GLOBAL | ENV_EXPORT);
    }
}

/// Various things we need to initialize at run-time that don't really fit any of the other init
/// routines.
void misc_init() {
    // If stdout is open on a tty ensure stdio is unbuffered. That's because those functions might
    // be intermixed with `write()` calls and we need to ensure the writes are not reordered. See
    // issue #3748.
    if (isatty(STDOUT_FILENO)) {
        fflush(stdout);
        setvbuf(stdout, nullptr, _IONBF, 0);
    }
}

/// Make sure the PATH variable contains something.
static void setup_path() {
    auto &vars = env_stack_t::globals();
    const auto path = vars.get_unless_empty(L"PATH");
    if (!path) {
#if defined(_CS_PATH)
        // _CS_PATH: colon-separated paths to find POSIX utilities
        std::string cspath;
        cspath.resize(confstr(_CS_PATH, nullptr, 0));
        if (cspath.length() > 0) {
            confstr(_CS_PATH, &cspath[0], cspath.length());
            // remove the trailing null-terminator
            cspath.resize(cspath.length() - 1);
        }
#else
        std::string cspath = "/usr/bin:/bin";  // I doubt this is even necessary
#endif
        vars.set_one(L"PATH", ENV_GLOBAL | ENV_EXPORT, str2wcstring(cspath));
    }
}

static std::map<wcstring, wcstring> inheriteds;

const std::map<wcstring, wcstring> &env_get_inherited() { return inheriteds; }

void env_init(const struct config_paths_t *paths, bool do_uvars, bool default_paths) {
    env_stack_t &vars = env_stack_t::principal();
    // Import environment variables. Walk backwards so that the first one out of any duplicates wins
    // (See issue #2784).
    wcstring key, val;
    const char *const *envp = environ;
    int i = 0;
    while (envp && envp[i]) i++;
    while (i--) {
        const wcstring key_and_val = str2wcstring(envp[i]);  // like foo=bar
        size_t eql = key_and_val.find(L'=');
        if (eql == wcstring::npos) {
            // No equal-sign found so treat it as a defined var that has no value(s).
            if (!var_is_electric(key_and_val)) {
                vars.set_empty(key_and_val, ENV_EXPORT | ENV_GLOBAL);
            }
            inheriteds[key] = L"";
        } else {
            key.assign(key_and_val, 0, eql);
            val.assign(key_and_val, eql + 1, wcstring::npos);
            inheriteds[key] = val;
            if (!var_is_electric(key)) {
                // fish_user_paths should not be exported; attempting to re-import it from
                // a value we previously (due to user error) exported will cause impossibly
                // difficult to debug PATH problems.
                if (key != L"fish_user_paths") {
                    vars.set(key, ENV_EXPORT | ENV_GLOBAL, {val});
                }
            }
        }
    }

    // Set the given paths in the environment, if we have any.
    if (paths != nullptr) {
        vars.set_one(FISH_DATADIR_VAR, ENV_GLOBAL, paths->data);
        vars.set_one(FISH_SYSCONFDIR_VAR, ENV_GLOBAL, paths->sysconf);
        vars.set_one(FISH_HELPDIR_VAR, ENV_GLOBAL, paths->doc);
        vars.set_one(FISH_BIN_DIR, ENV_GLOBAL, paths->bin);
        if (default_paths) {
            wcstring scstr = paths->data;
            scstr.append(L"/functions");
            vars.set_one(L"fish_function_path", ENV_GLOBAL, scstr);
        }
    }

    // Set $USER, $HOME and $EUID
    // This involves going to passwd and stuff.
    vars.set_one(L"EUID", ENV_GLOBAL, to_string(static_cast<unsigned long long>(geteuid())));
    setup_user(vars);

    wcstring user_config_dir;
    path_get_config(user_config_dir);
    vars.set_one(FISH_CONFIG_DIR, ENV_GLOBAL, user_config_dir);

    wcstring user_data_dir;
    path_get_data(user_data_dir);
    vars.set_one(FISH_USER_DATA_DIR, ENV_GLOBAL, user_data_dir);

    // Set up a default PATH
    setup_path();

    // Set up $IFS - this used to be in share/config.fish, but really breaks if it isn't done.
    vars.set_one(L"IFS", ENV_GLOBAL, L"\n \t");

    // Ensure this var is present even before an interactive command is run so that if it is used
    // in a function like `fish_prompt` or `fish_right_prompt` it is defined at the time the first
    // prompt is written.
    vars.set_one(L"CMD_DURATION", ENV_UNEXPORT, L"0");

    // Set up the version variable.
    wcstring version = str2wcstring(get_fish_version());
    vars.set_one(L"version", ENV_GLOBAL, version);
    vars.set_one(L"FISH_VERSION", ENV_GLOBAL, version);

    // Set the $fish_pid variable.
    vars.set_one(L"fish_pid", ENV_GLOBAL, to_string(getpid()));

    // Set the $hostname variable
    wcstring hostname = L"fish";
    get_hostname_identifier(hostname);
    vars.set_one(L"hostname", ENV_GLOBAL, hostname);

    // Set up SHLVL variable. Not we can't use vars.get() because SHLVL is read-only, and therefore
    // was not inherited from the environment.
    if (is_interactive_session()) {
        wcstring nshlvl_str = L"1";
        if (const char *shlvl_var = getenv("SHLVL")) {
            // TODO: Figure out how to handle invalid numbers better. Shouldn't we issue a
            // diagnostic?
            long shlvl_i = fish_wcstol(str2wcstring(shlvl_var).c_str());
            if (!errno && shlvl_i >= 0) {
                nshlvl_str = to_string(shlvl_i + 1);
            }
        }
        vars.set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, nshlvl_str);
    } else {
        // If we're not interactive, simply pass the value along.
        if (const char *shlvl_var = getenv("SHLVL")) {
            vars.set_one(L"SHLVL", ENV_GLOBAL | ENV_EXPORT, str2wcstring(shlvl_var));
        }
    }

    // initialize the PWD variable if necessary
    // Note we may inherit a virtual PWD that doesn't match what getcwd would return; respect that
    // if and only if it matches getcwd (#5647). Note we treat PWD as read-only so it was not set in
    // vars.
    //
    // Also reject all paths that don't start with "/", this includes windows paths like "F:\foo".
    // (see #7636)
    const char *incoming_pwd_cstr = getenv("PWD");
    wcstring incoming_pwd = incoming_pwd_cstr ? str2wcstring(incoming_pwd_cstr) : wcstring{};
    if (!incoming_pwd.empty() && incoming_pwd.front() == L'/' &&
        paths_are_same_file(incoming_pwd, L".")) {
        vars.set_one(L"PWD", ENV_EXPORT | ENV_GLOBAL, incoming_pwd);
    } else {
        vars.set_pwd_from_getcwd();
    }

    // Initialize termsize variables.
    environment_t &env_vars = vars;
    auto termsize = termsize_initialize_ffi(reinterpret_cast<const unsigned char *>(&env_vars));
    if (!vars.get_unless_empty(L"COLUMNS"))
        vars.set_one(L"COLUMNS", ENV_GLOBAL, to_string(termsize.width));
    if (!vars.get_unless_empty(L"LINES"))
        vars.set_one(L"LINES", ENV_GLOBAL, to_string(termsize.height));

    // Set fish_bind_mode to "default".
    vars.set_one(FISH_BIND_MODE_VAR, ENV_GLOBAL, DEFAULT_BIND_MODE);

    // Allow changes to variables to produce events.
    env_dispatch_init(vars);

    init_input();

    // Complain about invalid config paths.
    // HACK: Assume the defaults are correct (in practice this is only --no-config anyway).
    if (!default_paths) {
        path_emit_config_directory_messages(vars);
    }

    rust_env_init(do_uvars);
}

bool env_stack_t::is_principal() const { return impl_->is_principal(); }

std::vector<rust::Box<Event>> env_stack_t::universal_sync(bool always) {
    event_list_ffi_t result;
    impl_->universal_sync(always, result);
    return std::move(result.events);
}

statuses_t env_stack_t::get_last_statuses() const {
    auto statuses_ffi = impl_->get_last_statuses();
    statuses_t res{};
    res.status = statuses_ffi->get_status();
    res.kill_signal = statuses_ffi->get_kill_signal();
    auto &pipestatus = statuses_ffi->get_pipestatus();
    res.pipestatus.assign(pipestatus.begin(), pipestatus.end());
    return res;
}

int env_stack_t::get_last_status() const { return get_last_statuses().status; }

void env_stack_t::set_last_statuses(statuses_t s) {
    return impl_->set_last_statuses(s.status, s.kill_signal, s.pipestatus);
}

/// Update the PWD variable directory from the result of getcwd().
void env_stack_t::set_pwd_from_getcwd() { impl_->set_pwd_from_getcwd(); }

maybe_t<env_var_t> env_stack_t::get(const wcstring &key, env_mode_flags_t mode) const {
    if (auto *ptr = impl_->getf(key, mode)) {
        return env_var_t::new_ffi(ptr);
    }
    return none();
}

std::vector<wcstring> env_stack_t::get_names(env_mode_flags_t flags) const {
    wcstring_list_ffi_t names;
    impl_->get_names(flags, names);
    return std::move(names.vals);
}

int env_stack_t::set(const wcstring &key, env_mode_flags_t mode, std::vector<wcstring> vals) {
    return static_cast<int>(impl_->set(key, mode, std::move(vals)));
}

int env_stack_t::set_ffi(const wcstring &key, env_mode_flags_t mode, const void *vals,
                         size_t count) {
    const wchar_t *const *ptr = static_cast<const wchar_t *const *>(vals);
    return this->set(key, mode, std::vector<wcstring>(ptr, ptr + count));
}

int env_stack_t::set_one(const wcstring &key, env_mode_flags_t mode, wcstring val) {
    std::vector<wcstring> vals;
    vals.push_back(std::move(val));
    return set(key, mode, std::move(vals));
}

int env_stack_t::set_empty(const wcstring &key, env_mode_flags_t mode) {
    return set(key, mode, {});
}

int env_stack_t::remove(const wcstring &key, int mode) {
    return static_cast<int>(impl_->remove(key, mode));
}

std::shared_ptr<owning_null_terminated_array_t> env_stack_t::export_arr() {
    // export_array() returns a rust::Box<OwningNullTerminatedArrayRef>.
    // Acquire ownership.
    OwningNullTerminatedArrayRef *ptr =
        reinterpret_cast<OwningNullTerminatedArrayRef *>(impl_->export_array());
    assert(ptr && "Null pointer");
    return std::make_shared<owning_null_terminated_array_t>(
        rust::Box<OwningNullTerminatedArrayRef>::from_raw(ptr));
}

/// Wrapper around a EnvDyn.
class env_dyn_t final : public environment_t {
   public:
    env_dyn_t(rust::Box<EnvDyn> impl) : impl_(std::move(impl)) {}

    maybe_t<env_var_t> get(const wcstring &key, env_mode_flags_t mode) const {
        if (auto *ptr = impl_->getf(key, mode)) {
            return env_var_t::new_ffi(ptr);
        }
        return none();
    }

    std::vector<wcstring> get_names(env_mode_flags_t flags) const {
        wcstring_list_ffi_t names;
        impl_->get_names(flags, names);
        return std::move(names.vals);
    }

   private:
    rust::Box<EnvDyn> impl_;
};

std::shared_ptr<environment_t> env_stack_t::snapshot() const {
    auto res = std::make_shared<env_dyn_t>(impl_->snapshot());
    return std::static_pointer_cast<environment_t>(res);
}

void env_stack_t::set_argv(std::vector<wcstring> argv) { set(L"argv", ENV_LOCAL, std::move(argv)); }

wcstring env_stack_t::get_pwd_slash() const {
    std::unique_ptr<wcstring> res = impl_->get_pwd_slash();
    return std::move(*res);
}

void env_stack_t::push(bool new_scope) { impl_->push(new_scope); }

void env_stack_t::pop() { impl_->pop(); }

env_stack_t &env_stack_t::globals() {
    static env_stack_t s_globals(env_get_globals_ffi());
    return s_globals;
}

const std::shared_ptr<env_stack_t> &env_stack_t::principal_ref() {
    static const std::shared_ptr<env_stack_t> s_principal{new env_stack_t(env_get_principal_ffi())};
    return s_principal;
}

env_stack_t::~env_stack_t() = default;
env_stack_t::env_stack_t(env_stack_t &&) = default;
env_stack_t::env_stack_t(rust::Box<EnvStackRef> imp) : impl_(std::move(imp)) {}

#if defined(__APPLE__) || defined(__CYGWIN__)
static int check_runtime_path(const char *path) {
    UNUSED(path);
    return 0;
}
#else
/// Check, and create if necessary, a secure runtime path. Derived from tmux.c in tmux
/// (http://tmux.sourceforge.net/).
static int check_runtime_path(const char *path) {
    // Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
    //
    // Permission to use, copy, modify, and distribute this software for any
    // purpose with or without fee is hereby granted, provided that the above
    // copyright notice and this permission notice appear in all copies.
    //
    // THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    // WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    // MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    // ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    // WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
    // IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
    // OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
    struct stat statpath;
    uid_t uid = geteuid();

    if (mkdir(path, S_IRWXU) != 0 && errno != EEXIST) return errno;
    if (lstat(path, &statpath) != 0) return errno;
    if (!S_ISDIR(statpath.st_mode) || statpath.st_uid != uid ||
        (statpath.st_mode & (S_IRWXG | S_IRWXO)) != 0)
        return EACCES;
    return 0;
}
#endif

/// Return the path of an appropriate runtime data directory.
wcstring env_get_runtime_path() {
    wcstring result;
    const char *dir = getenv("XDG_RUNTIME_DIR");

    // Check that the path is actually usable. Technically this is guaranteed by the fdo spec but in
    // practice it is not always the case: see #1828 and #2222.
    if (dir != nullptr && check_runtime_path(dir) == 0) {
        result = str2wcstring(dir);
    } else {
        // Don't rely on $USER being set, as setup_user() has not yet been called.
        // See https://github.com/fish-shell/fish-shell/issues/5180
        // getpeuid() can't fail, but getpwuid sure can.
        auto pwuid = getpwuid(geteuid());
        const char *uname = pwuid ? pwuid->pw_name : nullptr;
        // /tmp/fish.user
        std::string tmpdir = get_path_to_tmp_dir() + "/fish.";
        if (uname) {
            tmpdir.append(uname);
        }

        if (!uname || check_runtime_path(tmpdir.c_str()) != 0) {
            FLOG(error, L"Runtime path not available.");
            FLOGF(error, L"Try deleting the directory %s and restarting fish.", tmpdir.c_str());
            return result;
        }

        result = str2wcstring(tmpdir);
    }
    return result;
}

static std::mutex s_setenv_lock{};

void setenv_lock(const char *name, const char *value, int overwrite) {
    scoped_lock locker(s_setenv_lock);
    setenv(name, value, overwrite);
}

void unsetenv_lock(const char *name) {
    scoped_lock locker(s_setenv_lock);
    unsetenv(name);
}

wcstring_list_ffi_t get_history_variable_text_ffi(const wcstring &fish_history_val) {
    wcstring_list_ffi_t out{};
    std::shared_ptr<history_t> history = commandline_get_state().history;
    if (!history) {
        // Effective duplication of history_session_id().
        wcstring session_id{};
        if (fish_history_val.empty()) {
            // No session.
            session_id.clear();
        } else if (!valid_var_name(fish_history_val)) {
            session_id = L"fish";
            FLOGF(error,
                  _(L"History session ID '%ls' is not a valid variable name. "
                    L"Falling back to `%ls`."),
                  fish_history_val.c_str(), session_id.c_str());
        } else {
            // Valid session.
            session_id = fish_history_val;
        }
        history = history_t::with_name(session_id);
    }
    if (history) {
        history->get_history(out.vals);
    }
    return out;
}

event_list_ffi_t::event_list_ffi_t() = default;

void event_list_ffi_t::push(void *event_vp) {
    auto event = static_cast<Event *>(event_vp);
    assert(event && "Null event");
    events.push_back(rust::Box<Event>::from_raw(event));
}
#endif
