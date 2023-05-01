#if 0
// Functions for storing and retrieving function information. These functions also take care of
// autoloading functions in the $fish_function_path. Actual function evaluation is taken care of by
// the parser and to some degree the builtin handling library.
//
#include "config.h"  // IWYU pragma: keep

#include "function.h"

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast.h"
#include "autoload.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "maybe.h"
#include "parse_constants.h"
#include "parser.h"
#include "parser_keywords.h"
#include "signals.h"
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
static std::shared_ptr<function_properties_t> copy_props(const function_properties_ref_t &props) {
    assert(props && "Null props");
    return std::make_shared<function_properties_t>(*props);
}

/// Make sure that if the specified function is a dynamically loaded function, it has been fully
/// loaded.
/// Note this executes fish script code.
bool function_load(const wcstring &name, parser_t &parser) {
    parser.assert_can_execute();
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
        // Crucially, the lock is acquired after perform_autoload().
        autoload_t::perform_autoload(*path_to_autoload, parser);
        function_set.acquire()->autoloader.mark_autoload_finished(name);
    }
    return path_to_autoload.has_value();
}

/// Insert a list of all dynamically loaded functions into the specified list.
static void autoload_names(std::unordered_set<wcstring> &names, bool get_hidden) {
    size_t i;

    // TODO: justify this.
    auto &vars = env_stack_t::principal();
    const auto path_var = vars.get_unless_empty(L"fish_function_path");
    if (!path_var) return;

    const std::vector<wcstring> &path_list = path_var->as_list();

    for (i = 0; i < path_list.size(); i++) {
        const wcstring &ndir_str = path_list.at(i);
        dir_iter_t dir(ndir_str);
        if (!dir.valid()) continue;

        while (const auto *entry = dir.next()) {
            const wchar_t *fn = entry->name.c_str();
            const wchar_t *suffix;
            if (!get_hidden && fn[0] == L'_') continue;

            suffix = std::wcsrchr(fn, L'.');
            // We need a ".fish" *suffix*, it can't be the entire name.
            if (suffix && suffix != fn && (std::wcscmp(suffix, L".fish") == 0)) {
                // Also ignore directories.
                if (!entry->is_dir()) {
                    wcstring name(fn, suffix - fn);
                    names.insert(name);
                }
            }
        }
    }
}

void function_add(wcstring name, std::shared_ptr<function_properties_t> props) {
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

wcstring function_get_definition_file(const function_properties_t &props) {
    return props.definition_file ? *props.definition_file : L"";
}

wcstring function_get_copy_definition_file(const function_properties_t &props) {
    return props.copy_definition_file ? *props.copy_definition_file : L"";
}
bool function_is_copy(const function_properties_t &props) { return props.is_copy; }
int function_get_definition_lineno(const function_properties_t &props) {
    return props.definition_lineno();
}
int function_get_copy_definition_lineno(const function_properties_t &props) {
    return props.copy_definition_lineno;
}

wcstring function_get_annotated_definition(const function_properties_t &props,
                                           const wcstring &name) {
    return props.annotated_definition(name);
}

function_properties_ref_t function_get_props_autoload(const wcstring &name, parser_t &parser) {
    parser.assert_can_execute();
    if (parser_keywords_is_reserved(name)) return nullptr;
    function_load(name, parser);
    return function_get_props(name);
}

bool function_exists(const wcstring &cmd, parser_t &parser) {
    parser.assert_can_execute();
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
    if (props.func_node->header().ptr()->try_source_range() &&
        props.func_node->end().try_source_range()) {
        auto header_src = props.func_node->header().ptr()->source_range();
        auto end_kw_src = props.func_node->end().range();
        uint32_t body_start = header_src.start + header_src.length;
        uint32_t body_end = end_kw_src.start;
        assert(body_start <= body_end && "end keyword should come after header");
        return wcstring(props.parsed_source->src(), body_start, body_end - body_start);
    }
    return wcstring{};
}

void function_set_desc(const wcstring &name, const wcstring &desc, parser_t &parser) {
    parser.assert_can_execute();
    function_load(name, parser);
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

bool function_copy(const wcstring &name, const wcstring &new_name, parser_t &parser) {
    auto filename = parser.current_filename();
    auto lineno = parser.get_lineno();

    auto funcset = function_set.acquire();
    auto props = funcset->get_props(name);
    if (!props) {
        // No such function.
        return false;
    }
    // Copy the function's props.
    auto new_props = copy_props(props);
    new_props->is_autoload = false;
    new_props->is_copy = true;
    new_props->copy_definition_file = filename;
    new_props->copy_definition_lineno = lineno;

    // Note this will NOT overwrite an existing function with the new name.
    // TODO: rationalize if this behavior is desired.
    funcset->funcs.emplace(new_name, std::move(new_props));
    return true;
}

std::vector<wcstring> function_get_names(bool get_hidden) {
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
    return std::vector<wcstring>(names.begin(), names.end());
}

void function_invalidate_path() {
    // Remove all autoloaded functions and update the autoload path.
    // Note we don't want to risk removal during iteration; we expect this to be called
    // infrequently.
    auto funcset = function_set.acquire();
    std::vector<wcstring> autoloadees;
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

function_properties_t::function_properties_t() : parsed_source(empty_parsed_source_ref()) {}

function_properties_t::function_properties_t(const function_properties_t &other)
    : parsed_source(empty_parsed_source_ref()) {
    *this = other;
}

function_properties_t &function_properties_t::operator=(const function_properties_t &other) {
    parsed_source = other.parsed_source->clone();
    func_node = other.func_node;
    named_arguments = other.named_arguments;
    description = other.description;
    inherit_vars = other.inherit_vars;
    shadow_scope = other.shadow_scope;
    is_autoload = other.is_autoload;
    definition_file = other.definition_file;
    return *this;
}

wcstring function_properties_t::annotated_definition(const wcstring &name) const {
    wcstring out;
    wcstring desc = this->localized_description();
    wcstring def = get_function_body_source(*this);
    auto handlers = event_get_function_handler_descs(name);

    out.append(L"function ");

    // Typically we prefer to specify the function name first, e.g. "function foo --description bar"
    // But if the function name starts with a -, we'll need to output it after all the options.
    bool defer_function_name = (name.at(0) == L'-');
    if (!defer_function_name) {
        out.append(escape_string(name));
    }

    // Output wrap targets.
    for (const wcstring &wrap : complete_get_wrap_targets(name)) {
        out.append(L" --wraps=");
        out.append(escape_string(wrap));
    }

    if (!desc.empty()) {
        out.append(L" --description ");
        out.append(escape_string(desc));
    }

    if (!this->shadow_scope) {
        out.append(L" --no-scope-shadowing");
    }

    for (const auto &d : handlers) {
        switch (d.typ) {
            case event_type_t::signal: {
                append_format(out, L" --on-signal %ls", sig2wcs(d.signal)->c_str());
                break;
            }
            case event_type_t::variable: {
                append_format(out, L" --on-variable %ls", d.str_param1->c_str());
                break;
            }
            case event_type_t::process_exit: {
                append_format(out, L" --on-process-exit %d", d.pid);
                break;
            }
            case event_type_t::job_exit: {
                append_format(out, L" --on-job-exit %d", d.pid);
                break;
            }
            case event_type_t::caller_exit: {
                append_format(out, L" --on-job-exit caller");
                break;
            }
            case event_type_t::generic: {
                append_format(out, L" --on-event %ls", d.str_param1->c_str());
                break;
            }
            case event_type_t::any:
            default: {
                DIE("unexpected next->typ");
            }
        }
    }

    const std::vector<wcstring> &named = this->named_arguments;
    if (!named.empty()) {
        append_format(out, L" --argument");
        for (const auto &name : named) {
            append_format(out, L" %ls", name.c_str());
        }
    }

    // Output the function name if we deferred it.
    if (defer_function_name) {
        out.append(L" -- ");
        out.append(escape_string(name));
    }

    // Output any inherited variables as `set -l` lines.
    for (const auto &kv : this->inherit_vars) {
        // We don't know what indentation style the function uses,
        // so we do what fish_indent would.
        append_format(out, L"\n    set -l %ls", kv.first.c_str());
        for (const auto &arg : kv.second) {
            out.push_back(L' ');
            out.append(escape_string(arg));
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
    assert(func_node->try_source_range() && "Function has no source range");
    auto source_range = func_node->source_range();
    uint32_t func_start = source_range.start;
    const wcstring &source = parsed_source->src();
    assert(func_start <= source.size() && "function start out of bounds");
    return 1 + std::count(source.begin(), source.begin() + func_start, L'\n');
}
#endif
