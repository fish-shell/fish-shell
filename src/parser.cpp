// The fish parser. Contains functions for parsing and evaluating code.
#include "config.h"  // IWYU pragma: keep

#include "parser.h"

#include <fcntl.h>
#include <stdio.h>

#include <algorithm>
#include <cwchar>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "ast.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fds.h"
#include "flog.h"
#include "function.h"
#include "job_group.rs.h"
#include "parse_constants.h"
#include "parse_execution.h"
#include "proc.h"
#include "signals.h"
#include "threads.rs.h"
#include "wutil.h"  // IWYU pragma: keep

class io_chain_t;

// Given a file path, return something nicer. Currently we just "unexpand" tildes.
static wcstring user_presentable_path(const wcstring &path, const environment_t &vars) {
    return replace_home_directory_with_tilde(path, vars);
}

parser_t::parser_t(std::shared_ptr<env_stack_t> vars, bool is_principal)
    : wait_handles(new_wait_handle_store_ffi()),
      variables(std::move(vars)),
      is_principal_(is_principal) {
    assert(variables.get() && "Null variables in parser initializer");
    int cwd = open_cloexec(".", O_RDONLY);
    if (cwd < 0) {
        perror("Unable to open the current working directory");
        return;
    }
    libdata().cwd_fd = std::make_shared<const autoclose_fd_t>(cwd);
}

// Out of line destructor to enable forward declaration of parse_execution_context_t
parser_t::~parser_t() = default;

parser_t &parser_t::principal_parser() {
    static const std::shared_ptr<parser_t> principal{
        new parser_t(env_stack_t::principal_ref(), true)};
    principal->assert_can_execute();
    return *principal;
}

parser_t *parser_t::principal_parser_ffi() { return &principal_parser(); }

void parser_t::assert_can_execute() const { ASSERT_IS_MAIN_THREAD(); }

rust::Box<WaitHandleStoreFFI> &parser_t::get_wait_handles_ffi() { return wait_handles; }

const rust::Box<WaitHandleStoreFFI> &parser_t::get_wait_handles_ffi() const { return wait_handles; }

int parser_t::set_var_and_fire(const wcstring &key, env_mode_flags_t mode,
                               std::vector<wcstring> vals) {
    int res = vars().set(key, mode, std::move(vals));
    if (res == ENV_OK) {
        event_fire(*this, *new_event_variable_set(key));
    }
    return res;
}

int parser_t::set_var_and_fire(const wcstring &key, env_mode_flags_t mode, wcstring val) {
    std::vector<wcstring> vals;
    vals.push_back(std::move(val));
    return set_var_and_fire(key, mode, std::move(vals));
}

void parser_t::sync_uvars_and_fire(bool always) {
    if (this->syncs_uvars_) {
        auto evts = this->vars().universal_sync(always);
        for (const auto &evt : evts) {
            event_fire(*this, *evt);
        }
    }
}

block_t *parser_t::push_block(block_t &&block) {
    block.src_lineno = parser_t::get_lineno();
    block.src_filename = parser_t::current_filename();
    if (block.type() != block_type_t::top) {
        bool new_scope = (block.type() == block_type_t::function_call);
        vars().push(new_scope);
        block.wants_pop_env = true;
    }

    // Push it onto our list and return a pointer to it.
    // Note that deques do not move their contents so this is safe.
    this->block_list.push_front(std::move(block));
    return &this->block_list.front();
}

void parser_t::pop_block(const block_t *expected) {
    assert(expected && expected == &this->block_list.at(0) && "Unexpected block");
    bool pop_env = expected->wants_pop_env;
    block_list.pop_front();  // beware, this deallocates 'expected'.
    if (pop_env) vars().pop();
}

const block_t *parser_t::block_at_index(size_t idx) const {
    return idx < block_list.size() ? &block_list[idx] : nullptr;
}

block_t *parser_t::block_at_index(size_t idx) {
    return idx < block_list.size() ? &block_list[idx] : nullptr;
}

/// Print profiling information to the specified stream.
static void print_profile(const std::deque<profile_item_t> &items, FILE *out) {
    for (size_t idx = 0; idx < items.size(); idx++) {
        const profile_item_t &item = items.at(idx);
        if (item.skipped || item.cmd.empty()) continue;

        long long total_time = item.duration;

        // Compute the self time as the total time, minus the total time consumed by subsequent
        // items exactly one eval level deeper.
        long long self_time = item.duration;
        for (size_t i = idx + 1; i < items.size(); i++) {
            const profile_item_t &nested_item = items.at(i);
            if (nested_item.skipped) continue;

            // If the eval level is not larger, then we have exhausted nested items.
            if (nested_item.level <= item.level) break;

            // If the eval level is exactly one more than our level, it is a directly nested item.
            if (nested_item.level == item.level + 1) self_time -= nested_item.duration;
        }

        if (std::fwprintf(out, L"%lld\t%lld\t", self_time, total_time) < 0) {
            wperror(L"fwprintf");
            return;
        }

        for (size_t i = 0; i < item.level; i++) {
            if (std::fwprintf(out, L"-") < 0) {
                wperror(L"fwprintf");
                return;
            }
        }

        if (std::fwprintf(out, L"> %ls\n", item.cmd.c_str()) < 0) {
            wperror(L"fwprintf");
            return;
        }
    }
}

void parser_t::clear_profiling() { profile_items.clear(); }

void parser_t::emit_profiling(const char *path) const {
    // Save profiling information. OK to not use CLO_EXEC here because this is called while fish is
    // exiting (and hence will not fork).
    FILE *f = fopen(path, "w");
    if (!f) {
        FLOGF(warning, _(L"Could not write profiling information to file '%s'"), path);
    } else {
        if (std::fwprintf(f, _(L"Time\tSum\tCommand\n"), profile_items.size()) < 0) {
            wperror(L"fwprintf");
        } else {
            print_profile(profile_items, f);
        }

        if (fclose(f)) {
            wperror(L"fclose");
        }
    }
}

completion_list_t parser_t::expand_argument_list(const wcstring &arg_list_src,
                                                 expand_flags_t eflags,
                                                 const operation_context_t &ctx) {
    // Parse the string as an argument list.
    auto ast = ast_parse_argument_list(arg_list_src);
    if (ast->errored()) {
        // Failed to parse. Here we expect to have reported any errors in test_args.
        return {};
    }

    // Get the root argument list and extract arguments from it.
    completion_list_t result;
    const ast::freestanding_argument_list_t &list = ast->top()->as_freestanding_argument_list();
    for (size_t i = 0; i < list.arguments().count(); i++) {
        const ast::argument_t &arg = *list.arguments().at(i);
        wcstring arg_src = *arg.source(arg_list_src);
        if (expand_string(arg_src, &result, eflags, ctx) == expand_result_t::error) {
            break;  // failed to expand a string
        }
    }
    return result;
}

void parser_t::set_cwd_fd(int fd) {
    assert(fd >= 0 && "Invalid fd");
    this->libdata().cwd_fd = std::make_shared<autoclose_fd_t>(fd);
}

std::shared_ptr<parser_t> parser_t::shared() { return shared_from_this(); }

cancel_checker_t parser_t::cancel_checker() const {
    return [] { return signal_check_cancel() != 0; };
}

operation_context_t parser_t::context() {
    return operation_context_t{this->shared(), this->vars(), this->cancel_checker()};
}

/// Append stack trace info for the block \p b to \p trace.
static void append_block_description_to_stack_trace(const parser_t &parser, const block_t &b,
                                                    wcstring &trace) {
    bool print_call_site = false;
    switch (b.type()) {
        case block_type_t::function_call:
        case block_type_t::function_call_no_shadow: {
            append_format(trace, _(L"in function '%ls'"), b.function_name.c_str());
            // Print arguments on the same line.
            wcstring args_str;
            for (const wcstring &arg : b.function_args) {
                if (!args_str.empty()) args_str.push_back(L' ');
                // We can't quote the arguments because we print this in quotes.
                // As a special-case, add the empty argument as "".
                if (!arg.empty()) {
                    args_str.append(escape_string(arg, ESCAPE_NO_QUOTED));
                } else {
                    args_str.append(L"\"\"");
                }
            }
            if (!args_str.empty()) {
                // TODO: Escape these.
                append_format(trace, _(L" with arguments '%ls'"), args_str.c_str());
            }
            trace.push_back('\n');
            print_call_site = true;
            break;
        }
        case block_type_t::subst: {
            append_format(trace, _(L"in command substitution\n"));
            print_call_site = true;
            break;
        }
        case block_type_t::source: {
            const filename_ref_t &source_dest = b.sourced_file;
            append_format(trace, _(L"from sourcing file %ls\n"),
                          user_presentable_path(*source_dest, parser.vars()).c_str());
            print_call_site = true;
            break;
        }
        case block_type_t::event: {
            assert(b.event && "Should have an event");
            wcstring description = *event_get_desc(parser, **b.event);
            append_format(trace, _(L"in event handler: %ls\n"), description.c_str());
            print_call_site = true;
            break;
        }

        case block_type_t::top:
        case block_type_t::begin:
        case block_type_t::switch_block:
        case block_type_t::while_block:
        case block_type_t::for_block:
        case block_type_t::if_block:
        case block_type_t::breakpoint:
        case block_type_t::variable_assignment:
            break;
    }

    if (print_call_site) {
        // Print where the function is called.
        const auto &file = b.src_filename;
        if (file) {
            append_format(trace, _(L"\tcalled on line %d of file %ls\n"), b.src_lineno,
                          user_presentable_path(*file, parser.vars()).c_str());
        } else if (parser.libdata().within_fish_init) {
            append_format(trace, _(L"\tcalled during startup\n"));
        }
    }
}

wcstring parser_t::stack_trace() const {
    wcstring trace;
    for (const auto &b : blocks()) {
        append_block_description_to_stack_trace(*this, b, trace);

        // Stop at event handler. No reason to believe that any other code is relevant.
        //
        // It might make sense in the future to continue printing the stack trace of the code
        // that invoked the event, if this is a programmatic event, but we can't currently
        // detect that.
        if (b.type() == block_type_t::event) break;
    }
    return trace;
}

bool parser_t::is_function() const {
    for (const auto &b : block_list) {
        if (b.is_function_call()) {
            return true;
        } else if (b.type() == block_type_t::source) {
            // If a function sources a file, don't descend further.
            break;
        }
    }
    return false;
}

bool parser_t::is_block() const {
    // Note historically this has descended into 'source', unlike 'is_function'.
    for (const auto &b : block_list) {
        if (b.type() != block_type_t::top && b.type() != block_type_t::subst) {
            return true;
        }
    }
    return false;
}

bool parser_t::is_breakpoint() const {
    for (const auto &b : block_list) {
        if (b.type() == block_type_t::breakpoint) {
            return true;
        }
    }
    return false;
}

bool parser_t::is_command_substitution() const {
    for (const auto &b : block_list) {
        if (b.type() == block_type_t::subst) {
            return true;
        } else if (b.type() == block_type_t::source) {
            // If a function sources a file, don't descend further.
            break;
        }
    }
    return false;
}

wcstring parser_t::get_function_name_ffi(int level) {
    auto name = get_function_name(level);
    if (name.has_value()) {
        return name.acquire();
    } else {
        return wcstring();
    }
}

maybe_t<wcstring> parser_t::get_function_name(int level) {
    if (level == 0) {
        // Return the function name for the level preceding the most recent breakpoint. If there
        // isn't one return the function name for the current level.
        // Walk until we find a breakpoint, then take the next function.
        bool found_breakpoint = false;
        for (const auto &b : block_list) {
            if (b.type() == block_type_t::breakpoint) {
                found_breakpoint = true;
            } else if (found_breakpoint && b.is_function_call()) {
                return b.function_name;
            }
        }
        return none();  // couldn't find a breakpoint frame
    }

    // Level 1 is the topmost function call. Level 2 is its caller. Etc.
    int funcs_seen = 0;
    for (const auto &b : block_list) {
        if (b.is_function_call()) {
            funcs_seen++;
            if (funcs_seen == level) {
                return b.function_name;
            }
        } else if (b.type() == block_type_t::source && level == 1) {
            // Historical: If we want the topmost function, but we are really in a file sourced by a
            // function, don't consider ourselves to be in a function.
            break;
        }
    }
    return none();
}

int parser_t::get_lineno() const {
    int lineno = -1;
    if (execution_context) {
        lineno = execution_context->get_current_line_number();
    }
    return lineno;
}

filename_ref_t parser_t::current_filename() const {
    for (const auto &b : block_list) {
        if (b.is_function_call()) {
            auto props = function_get_props(b.function_name);
            return props ? props->definition_file : nullptr;
        } else if (b.type() == block_type_t::source) {
            return b.sourced_file;
        }
    }
    // Fall back to the file being sourced.
    return libdata().current_filename;
}

// FFI glue
wcstring parser_t::current_filename_ffi() const {
    auto filename = current_filename();
    if (filename) {
        return wcstring(*filename);
    } else {
        return wcstring();
    }
}

bool parser_t::function_stack_is_overflowing() const {
    // We are interested in whether the count of functions on the stack exceeds
    // FISH_MAX_STACK_DEPTH. We don't separately track the number of functions, but we can have a
    // fast path through the eval_level. If the eval_level is in bounds, so must be the stack depth.
    if (eval_level <= FISH_MAX_STACK_DEPTH) {
        return false;
    }
    // Count the functions.
    int depth = 0;
    for (const auto &b : block_list) {
        depth += b.is_function_call();
    }
    return depth > FISH_MAX_STACK_DEPTH;
}

wcstring parser_t::current_line() {
    if (!execution_context) {
        return wcstring();
    }
    int source_offset = execution_context->get_current_source_offset();
    if (source_offset < 0) {
        return wcstring();
    }

    const int lineno = this->get_lineno();
    filename_ref_t file = this->current_filename();

    wcstring prefix;

    // If we are not going to print a stack trace, at least print the line number and filename.
    if (!is_interactive() || is_function()) {
        if (file) {
            append_format(prefix, _(L"%ls (line %d): "),
                          user_presentable_path(*file, vars()).c_str(), lineno);
        } else if (libdata().within_fish_init) {
            append_format(prefix, L"%ls (line %d): ", _(L"Startup"), lineno);
        } else {
            append_format(prefix, L"%ls (line %d): ", _(L"Standard input"), lineno);
        }
    }

    bool skip_caret = is_interactive() && !is_function();

    // Use an error with empty text.
    assert(source_offset >= 0);
    parse_error_t empty_error = {};
    empty_error.text = std::make_unique<wcstring>();
    empty_error.source_start = source_offset;

    wcstring line_info = *empty_error.describe_with_prefix(execution_context->get_source(), prefix,
                                                           is_interactive(), skip_caret);
    if (!line_info.empty()) {
        line_info.push_back(L'\n');
    }

    line_info.append(this->stack_trace());
    return line_info;
}

void parser_t::job_add(shared_ptr<job_t> job) {
    assert(job != nullptr);
    assert(!job->processes.empty());
    job_list.insert(job_list.begin(), std::move(job));
}

void parser_t::job_promote(job_list_t::iterator job_it) {
    // Move the job to the beginning.
    std::rotate(job_list.begin(), job_it, std::next(job_it));
}

void parser_t::job_promote(const job_t *job) {
    job_list_t::iterator loc;
    for (loc = job_list.begin(); loc != job_list.end(); ++loc) {
        if (loc->get() == job) {
            break;
        }
    }
    assert(loc != job_list.end());
    job_promote(loc);
}

void parser_t::job_promote_at(size_t job_pos) {
    assert(job_pos < job_list.size());
    job_promote(job_list.begin() + job_pos);
}

const job_t *parser_t::job_with_id(job_id_t id) const {
    for (const auto &job : job_list) {
        if (id <= 0 || job->job_id() == id) return job.get();
    }
    return nullptr;
}

job_t *parser_t::job_get_from_pid(pid_t pid) const {
    size_t job_pos{};
    return job_get_from_pid(pid, job_pos);
}

job_t *parser_t::job_get_from_pid(int pid, size_t &job_pos) const {
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        for (const process_ptr_t &p : (*it)->processes) {
            if (p->pid == pid) {
                job_pos = it - job_list.begin();
                return (*it).get();
            }
        }
    }
    return nullptr;
}

library_data_pod_t *parser_t::ffi_libdata_pod() { return &library_data; }

job_t *parser_t::ffi_job_get_from_pid(int pid) const { return job_get_from_pid(pid); }
const library_data_pod_t &parser_t::ffi_libdata_pod_const() const { return library_data; }

profile_item_t *parser_t::create_profile_item() {
    if (g_profiling_active) {
        profile_items.emplace_back();
        return &profile_items.back();
    }
    return nullptr;
}

eval_res_t parser_t::eval(const wcstring &cmd, const io_chain_t &io) {
    return eval_with(cmd, io, {}, block_type_t::top);
}

eval_res_t parser_t::eval_with(const wcstring &cmd, const io_chain_t &io,
                               const job_group_ref_t &job_group, enum block_type_t block_type) {
    // Parse the source into a tree, if we can.
    auto error_list = new_parse_error_list();
    auto ps = parse_source(wcstring{cmd}, parse_flag_none, &*error_list);
    if (ps->has_value()) {
        return this->eval_parsed_source(*ps, io, job_group, block_type);
    } else {
        // Get a backtrace. This includes the message.
        wcstring backtrace_and_desc;
        this->get_backtrace(cmd, *error_list, backtrace_and_desc);

        // Print it.
        std::fwprintf(stderr, L"%ls\n", backtrace_and_desc.c_str());

        // Set a valid status.
        this->set_last_statuses(statuses_t::just(STATUS_ILLEGAL_CMD));
        bool break_expand = true;
        return eval_res_t{proc_status_t::from_exit_code(STATUS_ILLEGAL_CMD), break_expand};
    }
}

eval_res_t parser_t::eval_string_ffi1(const wcstring &cmd) { return eval(cmd, io_chain_t()); }

eval_res_t parser_t::eval_parsed_source(const parsed_source_ref_t &ps, const io_chain_t &io,
                                        const job_group_ref_t &job_group,
                                        enum block_type_t block_type) {
    assert(block_type == block_type_t::top || block_type == block_type_t::subst);
    const auto &job_list = ps.ast().top()->as_job_list();
    if (!job_list.empty()) {
        // Execute the top job list.
        return this->eval_node(ps, job_list, io, job_group, block_type);
    } else {
        auto status = proc_status_t::from_exit_code(get_last_status());
        bool break_expand = false;
        bool was_empty = true;
        bool no_status = true;
        return eval_res_t{status, break_expand, was_empty, no_status};
    }
}

template <typename T>
eval_res_t parser_t::eval_node(const parsed_source_ref_t &ps, const T &node,
                               const io_chain_t &block_io, const job_group_ref_t &job_group,
                               block_type_t block_type) {
    static_assert(
        std::is_same<T, ast::statement_t>::value || std::is_same<T, ast::job_list_t>::value,
        "Unexpected node type");

    // Only certain blocks are allowed.
    assert((block_type == block_type_t::top || block_type == block_type_t::subst) &&
           "Invalid block type");

    // If fish itself got a cancel signal, then we want to unwind back to the principal parser.
    // If we are the principal parser and our block stack is empty, then we want to clear the
    // signal.
    // Note this only happens in interactive sessions. In non-interactive sessions, SIGINT will
    // cause fish to exit.
    if (int sig = signal_check_cancel()) {
        if (is_principal_ && block_list.empty()) {
            signal_clear_cancel();
        } else {
            return proc_status_t::from_signal(sig);
        }
    }

    // A helper to detect if we got a signal.
    // This includes both signals sent to fish (user hit control-C while fish is foreground) and
    // signals from the job group (e.g. some external job terminated with SIGQUIT).
    auto check_cancel_signal = [=] {
        // Did fish itself get a signal?
        int sig = signal_check_cancel();
        // Has this job group been cancelled?
        if (!sig && job_group) sig = job_group->get_cancel_signal();
        return sig;
    };

    // If we have a job group which is cancelled, then do nothing.
    if (int sig = check_cancel_signal()) {
        return proc_status_t::from_signal(sig);
    }

    job_reap(*this, false);  // not sure why we reap jobs here

    // Start it up
    operation_context_t op_ctx = this->context();
    block_t *scope_block = this->push_block(block_t::scope_block(block_type));

    // Propagate our job group.
    op_ctx.job_group = job_group;

    // Replace the context's cancel checker with one that checks the job group's signal.
    op_ctx.cancel_checker = [=] { return check_cancel_signal() != 0; };

    // Create and set a new execution context.
    using exc_ctx_ref_t = std::unique_ptr<parse_execution_context_t>;
    scoped_push<exc_ctx_ref_t> exc(
        &execution_context, make_unique<parse_execution_context_t>(ps.clone(), op_ctx, block_io));

    // Check the exec count so we know if anything got executed.
    const size_t prev_exec_count = libdata().exec_count;
    const size_t prev_status_count = libdata().status_count;
    end_execution_reason_t reason = execution_context->eval_node(node, scope_block);
    const size_t new_exec_count = libdata().exec_count;
    const size_t new_status_count = libdata().status_count;

    exc.restore();
    this->pop_block(scope_block);

    job_reap(*this, false);  // reap again

    if (int sig = check_cancel_signal()) {
        return proc_status_t::from_signal(sig);
    } else {
        auto status = proc_status_t::from_exit_code(this->get_last_status());
        bool break_expand = (reason == end_execution_reason_t::error);
        bool was_empty = !break_expand && prev_exec_count == new_exec_count;
        bool no_status = prev_status_count == new_status_count;
        return eval_res_t{status, break_expand, was_empty, no_status};
    }
}

// Explicit instantiations. TODO: use overloads instead?
template eval_res_t parser_t::eval_node(const parsed_source_ref_t &, const ast::statement_t &,
                                        const io_chain_t &, const job_group_ref_t &, block_type_t);
template eval_res_t parser_t::eval_node(const parsed_source_ref_t &, const ast::job_list_t &,
                                        const io_chain_t &, const job_group_ref_t &, block_type_t);

void parser_t::get_backtrace(const wcstring &src, const parse_error_list_t &errors,
                             wcstring &output) const {
    if (!errors.empty()) {
        const auto *err = errors.at(0);

        // Determine if we want to try to print a caret to point at the source error. The
        // err.source_start() <= src.size() check is due to the nasty way that slices work, which is
        // by rewriting the source.
        size_t which_line = 0;
        bool skip_caret = true;
        if (err->source_start() != SOURCE_LOCATION_UNKNOWN && err->source_start() <= src.size()) {
            // Determine which line we're on.
            which_line = 1 + std::count(src.begin(), src.begin() + err->source_start(), L'\n');

            // Don't include the caret if we're interactive, this is the first line of text, and our
            // source is at its beginning, because then it's obvious.
            skip_caret = (is_interactive() && which_line == 1 && err->source_start() == 0);
        }

        wcstring prefix;
        filename_ref_t filename = this->current_filename();
        if (filename) {
            if (which_line > 0) {
                prefix =
                    format_string(_(L"%ls (line %lu): "),
                                  user_presentable_path(*filename, vars()).c_str(), which_line);
            } else {
                prefix =
                    format_string(_(L"%ls: "), user_presentable_path(*filename, vars()).c_str());
            }
        } else {
            prefix = L"fish: ";
        }

        const wcstring description =
            *err->describe_with_prefix(src, prefix, is_interactive(), skip_caret);
        if (!description.empty()) {
            output.append(description);
            output.push_back(L'\n');
        }
        output.append(this->stack_trace());
    }
}

RustFFIJobList parser_t::ffi_jobs() const {
    return RustFFIJobList{const_cast<job_ref_t *>(job_list.data()), job_list.size()};
}

bool parser_t::ffi_has_funtion_block() const {
    for (const auto &b : blocks()) {
        if (b.is_function_call()) {
            return true;
        }
    }
    return false;
}

uint64_t parser_t::ffi_global_event_blocks() const { return global_event_blocks; }
void parser_t::ffi_incr_global_event_blocks() { ++global_event_blocks; }
void parser_t::ffi_decr_global_event_blocks() { --global_event_blocks; }

size_t parser_t::ffi_blocks_size() const { return block_list.size(); }

block_t::block_t(block_type_t t) : block_type(t) {}

wcstring block_t::description() const {
    wcstring result;
    switch (this->type()) {
        case block_type_t::while_block: {
            result.append(L"while");
            break;
        }
        case block_type_t::for_block: {
            result.append(L"for");
            break;
        }
        case block_type_t::if_block: {
            result.append(L"if");
            break;
        }
        case block_type_t::function_call: {
            result.append(L"function_call");
            break;
        }
        case block_type_t::function_call_no_shadow: {
            result.append(L"function_call_no_shadow");
            break;
        }
        case block_type_t::switch_block: {
            result.append(L"switch");
            break;
        }
        case block_type_t::subst: {
            result.append(L"substitution");
            break;
        }
        case block_type_t::top: {
            result.append(L"top");
            break;
        }
        case block_type_t::begin: {
            result.append(L"begin");
            break;
        }
        case block_type_t::source: {
            result.append(L"source");
            break;
        }
        case block_type_t::event: {
            result.append(L"event");
            break;
        }
        case block_type_t::breakpoint: {
            result.append(L"breakpoint");
            break;
        }
        case block_type_t::variable_assignment: {
            result.append(L"variable_assignment");
            break;
        }
    }

    if (this->src_lineno >= 0) {
        append_format(result, L" (line %d)", this->src_lineno);
    }
    if (this->src_filename) {
        append_format(result, L" (file %ls)", this->src_filename->c_str());
    }
    return result;
}

bool block_t::is_function_call() const {
    return type() == block_type_t::function_call || type() == block_type_t::function_call_no_shadow;
}

// Various block constructors.

block_t block_t::if_block() { return block_t(block_type_t::if_block); }

block_t block_t::event_block(const void *evt_) {
    const auto &evt = *static_cast<const Event *>(evt_);
    block_t b{block_type_t::event};
    b.event =
        std::make_shared<rust::Box<Event>>(evt.clone());  // TODO Post-FFI: move instead of clone.
    return b;
}

block_t block_t::function_block(wcstring name, std::vector<wcstring> args, bool shadows) {
    block_t b{shadows ? block_type_t::function_call : block_type_t::function_call_no_shadow};
    b.function_name = std::move(name);
    b.function_args = std::move(args);
    return b;
}

block_t block_t::source_block(filename_ref_t src) {
    block_t b{block_type_t::source};
    b.sourced_file = std::move(src);
    return b;
}

block_t block_t::for_block() { return block_t{block_type_t::for_block}; }
block_t block_t::while_block() { return block_t{block_type_t::while_block}; }
block_t block_t::switch_block() { return block_t{block_type_t::switch_block}; }
block_t block_t::scope_block(block_type_t type) {
    assert(
        (type == block_type_t::begin || type == block_type_t::top || type == block_type_t::subst) &&
        "Invalid scope type");
    return block_t(type);
}
block_t block_t::breakpoint_block() { return block_t(block_type_t::breakpoint); }
block_t block_t::variable_assignment_block() { return block_t(block_type_t::variable_assignment); }

void block_t::ffi_incr_event_blocks() { ++event_blocks; }
