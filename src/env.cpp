// Functions for setting and getting environment variables.
#include "config.h"  // IWYU pragma: keep

#include "env.h"

#include <unistd.h>

#include "history.h"
#include "reader.h"

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
