// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include "env.h"

#include <pwd.h>
#include <unistd.h>

#include "history.h"
#include "path.h"
#include "reader.h"

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

bool env_stack_t::is_principal() const { return impl_->is_principal(); }

static std::map<wcstring, wcstring> inheriteds;

void set_inheriteds_ffi() {
    wcstring key, val;
    const char *const *envp = environ;
    int i = 0;
    while (envp && envp[i]) i++;
    while (i--) {
        const wcstring key_and_val = str2wcstring(envp[i]);
        size_t eql = key_and_val.find(L'=');
        if (eql == wcstring::npos) {
            continue;
        }
        key.assign(key_and_val, 0, eql);
        val.assign(key_and_val, eql + 1, wcstring::npos);
        inheriteds[key] = val;
    }
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

int env_stack_t::remove(const wcstring &key, int mode) {
    return static_cast<int>(impl_->remove(key, mode));
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
env_stack_t::env_stack_t(uint8_t *imp)
    : impl_(rust::Box<EnvStackRef>::from_raw(reinterpret_cast<EnvStackRef *>(imp))) {}

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
    maybe_t<rust::Box<HistorySharedPtr>> history = commandline_get_state().history;
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
        history = history_with_name(session_id);
    }
    if (history) {
        out = *(*history)->get_history();
    }
    return out;
}

const EnvStackRef &env_stack_t::get_impl_ffi() const { return *impl_; }
