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
#include "env_dispatch.rs.h"
#include "env_universal_common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
#include "flog.h"
#include "global_safety.h"
#include "history.h"
#include "input.h"
#include "null_terminated_array.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "termsize.h"
#include "threads.rs.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

/// At init, we read all the environment variables from this array.
extern char **environ;

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

static std::map<wcstring, wcstring> inheriteds;

const std::map<wcstring, wcstring> &env_get_inherited() { return inheriteds; }

void set_inheriteds_ffi() {
    wcstring key, val;
    const char *const *envp = environ;
    int i = 0;
    while (envp && envp[i]) i++;
    while (i--) {
        const wcstring key_and_val = str2wcstring(envp[i]);
        size_t eql = key_and_val.find(L'=');
        if (eql == wcstring::npos) {
            // PORTING: Should this not be key_and_val?
            inheriteds[key] = L"";
        } else {
            key.assign(key_and_val, 0, eql);
            val.assign(key_and_val, eql + 1, wcstring::npos);
            inheriteds[key] = val;
        }
    }
}

bool env_stack_t::is_principal() const { return impl_->is_principal(); }

std::vector<rust::Box<Event>> env_stack_t::universal_sync(bool always) {
    event_list_ffi_t result;
    impl_->universal_sync(always, result);
    return std::move(result.events);
}

void env_stack_t::apply_inherited_ffi(const function_properties_t &props) {
    impl_->apply_inherited_ffi(props);
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
    // export_array() returns a rust::Box<OwningNullTerminatedArrayRefFFI>.
    // Acquire ownership.
    OwningNullTerminatedArrayRefFFI *ptr = impl_->export_array();
    assert(ptr && "Null pointer");
    return std::make_shared<owning_null_terminated_array_t>(
        rust::Box<OwningNullTerminatedArrayRefFFI>::from_raw(ptr));
}

maybe_t<env_var_t> env_dyn_t::get(const wcstring &key, env_mode_flags_t mode) const {
    if (auto *ptr = impl_->getf(key, mode)) {
        return env_var_t::new_ffi(ptr);
    }
    return none();
}

std::vector<wcstring> env_dyn_t::get_names(env_mode_flags_t flags) const {
    wcstring_list_ffi_t names;
    impl_->get_names(flags, names);
    return std::move(names.vals);
}

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

extern "C" {
void setenv_lock(const char *name, const char *value, int overwrite) {
    scoped_lock locker(s_setenv_lock);
    setenv(name, value, overwrite);
}

void unsetenv_lock(const char *name) {
    scoped_lock locker(s_setenv_lock);
    unsetenv(name);
}
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
