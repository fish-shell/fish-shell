// Functions for storing and retrieving function information. These functions also take care of
// autoloading functions in the $fish_function_path. Actual function evaluation is taken care of by
// the parser and to some degree the builtin handling library.
//
#include "config.h"  // IWYU pragma: keep

// IWYU pragma: no_include <type_traits>
#include <dirent.h>
#include <pthread.h>
#include <stddef.h>

#include <algorithm>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "autoload.h"
#include "common.h"
#include "env.h"
#include "event.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "intern.h"
#include "parser.h"
#include "parser_keywords.h"
#include "reader.h"
#include "signal.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

namespace {

/// Type wrapping up the set of all functions.
/// There's only one of these; it's managed by a lock.
struct function_set_t {
    /// The map of all functions by name.
    std::unordered_map<wcstring, function_properties_ref_t> funcs;

    /// Tombstones for functions that should no longer be autoloaded.
    std::unordered_set<wcstring> autoload_tombstones;

    /// The autoloader for our functions.
    autoload_t autoloader{L"fish_function_path"};

    /// Remove a function.
    /// \return true if successful, false if it doesn't exist.
    bool remove(const wcstring &name);

    /// Get the properties for a function, or nullptr if none.
    function_properties_ref_t get_props(const wcstring &name) const {
        auto iter = funcs.find(name);
        return iter == funcs.end() ? nullptr : iter->second;
    }

    /// \return true if we should allow autoloading a given function.
    bool allow_autoload(const wcstring &name) const;

    function_set_t() = default;
};

/// The big set of all functions.
static owning_lock<function_set_t> function_set;

bool function_set_t::allow_autoload(const wcstring &name) const {
    // Prohibit autoloading if we have a non-autoload (explicit) function, or if the function is
    // tombstoned.
    auto props = get_props(name);
    bool has_explicit_func = props && !props->is_autoload;
    bool is_tombstoned = autoload_tombstones.count(name) > 0;
    return !has_explicit_func && !is_tombstoned;
}
}  // namespace

/// \return a copy of some function props, in a new shared_ptr.
std::shared_ptr<function_properties_t> copy_props(const function_properties_ref_t &props) {
    assert(props && "Null props");
    return std::make_shared<function_properties_t>(*props);
}

/// Make sure that if the specified function is a dynamically loaded function, it has been fully
/// loaded.
/// Note this executes fish script code.
static void try_autoload(const wcstring &name, parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();
    maybe_t<wcstring> path_to_autoload;
    // Note we can't autoload while holding the funcset lock.
    // Lock around a local region.
    {
        auto funcset = function_set.acquire();
        if (funcset->allow_autoload(name)) {
            path_to_autoload = funcset->autoloader.resolve_command(name, env_stack_t::globals());
        }
    }

    // Release the lock and perform any autoload, then reacquire the lock and clean up.
    if (path_to_autoload) {
        // Crucially, the lock is acquired *after* do_autoload_file_at_path().
        autoload_t::perform_autoload(*path_to_autoload, parser);
        function_set.acquire()->autoloader.mark_autoload_finished(name);
    }
}

/// Insert a list of all dynamically loaded functions into the specified list.
static void autoload_names(std::unordered_set<wcstring> &names, int get_hidden) {
    size_t i;

    // TODO: justfy this.
    auto &vars = env_stack_t::principal();
    const auto path_var = vars.get(L"fish_function_path");
    if (path_var.missing_or_empty()) return;

    const wcstring_list_t &path_list = path_var->as_list();

    for (i = 0; i < path_list.size(); i++) {
        const wcstring &ndir_str = path_list.at(i);
        dir_t dir(ndir_str);
        if (!dir.valid()) continue;

        wcstring name;
        while (dir.read(name)) {
            const wchar_t *fn = name.c_str();
            const wchar_t *suffix;
            if (!get_hidden && fn[0] == L'_') continue;

            suffix = std::wcsrchr(fn, L'.');
            if (suffix && (std::wcscmp(suffix, L".fish") == 0)) {
                wcstring name(fn, suffix - fn);
                names.insert(name);
            }
        }
    }
}

void function_add(wcstring name, std::shared_ptr<function_properties_t> props) {
    ASSERT_IS_MAIN_THREAD();
    assert(props && "Null props");
    auto funcset = function_set.acquire();

    // Historical check. TODO: rationalize this.
    if (name.empty()) {
        return;
    }

    // Remove the old function.
    funcset->remove(name);

    // Check if this is a function that we are autoloading.
    props->is_autoload = funcset->autoloader.autoload_in_progress(name);

    // Create and store a new function.
    auto ins = funcset->funcs.emplace(std::move(name), std::move(props));
    assert(ins.second && "Function should not already be present in the table");
    (void)ins;
}

function_properties_ref_t function_get_props(const wcstring &name) {
    if (parser_keywords_is_reserved(name)) return nullptr;
    return function_set.acquire()->get_props(name);
}

function_properties_ref_t function_get_props_autoload(const wcstring &name, parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();
    if (parser_keywords_is_reserved(name)) return nullptr;
    try_autoload(name, parser);
    return function_get_props(name);
}

bool function_exists(const wcstring &cmd, parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();
    if (!valid_func_name(cmd)) return false;
    return function_get_props_autoload(cmd, parser) != nullptr;
}

bool function_exists_no_autoload(const wcstring &cmd) {
    if (!valid_func_name(cmd)) return false;
    if (parser_keywords_is_reserved(cmd)) return false;
    auto funcset = function_set.acquire();

    // Check if we either have the function, or it could be autoloaded.
    return funcset->get_props(cmd) || funcset->autoloader.can_autoload(cmd);
}

bool function_set_t::remove(const wcstring &name) {
    size_t amt = funcs.erase(name);
    if (amt > 0) {
        event_remove_function_handlers(name);
    }
    return amt > 0;
}

void function_remove(const wcstring &name) {
    auto funcset = function_set.acquire();
    funcset->remove(name);
    // Prevent (re-)autoloading this function.
    funcset->autoload_tombstones.insert(name);
}

// \return the body of a function (everything after the header, up to but not including the 'end').
static wcstring get_function_body_source(const function_properties_t &props) {
    // We want to preserve comments that the AST attaches to the header (#5285).
    // Take everything from the end of the header to the 'end' keyword.
    auto header_src = props.func_node->header->try_source_range();
    auto end_kw_src = props.func_node->end.try_source_range();
    if (header_src && end_kw_src) {
        uint32_t body_start = header_src->start + header_src->length;
        uint32_t body_end = end_kw_src->start;
        assert(body_start <= body_end && "end keyword should come after header");
        return wcstring(props.parsed_source->src, body_start, body_end - body_start);
    }
    return wcstring{};
}

void function_set_desc(const wcstring &name, const wcstring &desc, parser_t &parser) {
    ASSERT_IS_MAIN_THREAD();
    try_autoload(name, parser);
    auto funcset = function_set.acquire();
    auto iter = funcset->funcs.find(name);
    if (iter != funcset->funcs.end()) {
        // Note the description is immutable, as it may be accessed on another thread, so we copy
        // the properties to modify it.
        auto new_props = copy_props(iter->second);
        new_props->description = desc;
        iter->second = new_props;
    }
}

bool function_copy(const wcstring &name, const wcstring &new_name) {
    auto funcset = function_set.acquire();
    auto props = funcset->get_props(name);
    if (!props) {
        // No such function.
        return false;
    }
    // Copy the function's props.
    // This new instance of the function shouldn't be tied to the definition file of the
    // original, so clear the filename, etc.
    auto new_props = copy_props(props);
    new_props->is_autoload = false;
    new_props->definition_file = nullptr;

    // Note this will NOT overwrite an existing function with the new name.
    // TODO: rationalize if this behavior is desired.
    funcset->funcs.emplace(new_name, std::move(new_props));
    return true;
}

wcstring_list_t function_get_names(int get_hidden) {
    std::unordered_set<wcstring> names;
    auto funcset = function_set.acquire();
    autoload_names(names, get_hidden);
    for (const auto &func : funcset->funcs) {
        const wcstring &name = func.first;

        // Maybe skip hidden.
        if (!get_hidden && (name.empty() || name.at(0) == L'_')) {
            continue;
        }
        names.insert(name);
    }
    return wcstring_list_t(names.begin(), names.end());
}

void function_invalidate_path() {
    // Remove all autoloaded functions and update the autoload path.
    // Note we don't want to risk removal during iteration; we expect this to be called
    // infrequently.
    auto funcset = function_set.acquire();
    wcstring_list_t autoloadees;
    for (const auto &kv : funcset->funcs) {
        if (kv.second->is_autoload) {
            autoloadees.push_back(kv.first);
        }
    }
    for (const wcstring &name : autoloadees) {
        funcset->remove(name);
    }
    funcset->autoloader.clear();
}

wcstring function_properties_t::annotated_definition(const wcstring &name) const {
    wcstring out;
    wcstring desc = this->localized_description();
    wcstring def = get_function_body_source(*this);
    std::vector<std::shared_ptr<event_handler_t>> ev = event_get_function_handlers(name);

    out.append(L"function ");

    // Typically we prefer to specify the function name first, e.g. "function foo --description bar"
    // But if the function name starts with a -, we'll need to output it after all the options.
    bool defer_function_name = (name.at(0) == L'-');
    if (!defer_function_name) {
        out.append(escape_string(name, ESCAPE_ALL));
    }

    // Output wrap targets.
    for (const wcstring &wrap : complete_get_wrap_targets(name)) {
        out.append(L" --wraps=");
        out.append(escape_string(wrap, ESCAPE_ALL));
    }

    if (!desc.empty()) {
        wcstring esc_desc = escape_string(desc, ESCAPE_ALL);
        out.append(L" --description ");
        out.append(esc_desc);
    }

    if (!this->shadow_scope) {
        out.append(L" --no-scope-shadowing");
    }

    for (const auto &next : ev) {
        const event_description_t &d = next->desc;
        switch (d.type) {
            case event_type_t::signal: {
                append_format(out, L" --on-signal %ls", sig2wcs(d.param1.signal));
                break;
            }
            case event_type_t::variable: {
                append_format(out, L" --on-variable %ls", d.str_param1.c_str());
                break;
            }
            case event_type_t::process_exit: {
                append_format(out, L" --on-process-exit %d", d.param1.pid);
                break;
            }
            case event_type_t::job_exit: {
                append_format(out, L" --on-job-exit %d", d.param1.jobspec.pid);
                break;
            }
            case event_type_t::caller_exit: {
                append_format(out, L" --on-job-exit caller");
                break;
            }
            case event_type_t::generic: {
                append_format(out, L" --on-event %ls", d.str_param1.c_str());
                break;
            }
            case event_type_t::any:
            default: {
                DIE("unexpected next->type");
            }
        }
    }

    const wcstring_list_t &named = this->named_arguments;
    if (!named.empty()) {
        append_format(out, L" --argument");
        for (const auto &name : named) {
            append_format(out, L" %ls", name.c_str());
        }
    }

    // Output the function name if we deferred it.
    if (defer_function_name) {
        out.append(L" -- ");
        out.append(escape_string(name, ESCAPE_ALL));
    }

    // Output any inherited variables as `set -l` lines.
    for (const auto &kv : this->inherit_vars) {
        // We don't know what indentation style the function uses,
        // so we do what fish_indent would.
        append_format(out, L"\n    set -l %ls", kv.first.c_str());
        for (const auto &arg : kv.second) {
            wcstring earg = escape_string(arg, ESCAPE_ALL);
            out.push_back(L' ');
            out.append(earg);
        }
    }
    out.push_back('\n');
    out.append(def);

    // Append a newline before the 'end', unless there already is one there.
    if (!string_suffixes_string(L"\n", def)) {
        out.push_back(L'\n');
    }
    out.append(L"end\n");
    return out;
}

const wchar_t *function_properties_t::localized_description() const {
    if (description.empty()) return L"";
    return _(description.c_str());
}

int function_properties_t::definition_lineno() const {
    // return one plus the number of newlines at offsets less than the start of our function's
    // statement (which includes the header).
    // TODO: merge with line_offset_of_character_at_offset?
    auto source_range = func_node->try_source_range();
    assert(source_range && "Function has no source range");
    uint32_t func_start = source_range->start;
    const wcstring &source = parsed_source->src;
    assert(func_start <= source.size() && "function start out of bounds");
    return 1 + std::count(source.begin(), source.begin() + func_start, L'\n');
}
