// The fish parser. Contains functions for parsing and evaluating code.
#include "config.h"  // IWYU pragma: keep

#include "parser.h"

#include <fcntl.h>
#include <stdio.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <utility>

#include "common.h"
#include "env.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "intern.h"
#include "parse_constants.h"
#include "parse_execution.h"
#include "parse_util.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"
#include "tnode.h"
#include "wutil.h"  // IWYU pragma: keep

class io_chain_t;

/// Error for evaluating in illegal scope.
#define INVALID_SCOPE_ERR_MSG _(L"Tried to evaluate commands using invalid block type '%ls'")

/// While block description.
#define WHILE_BLOCK N_(L"'while' block")

/// For block description.
#define FOR_BLOCK N_(L"'for' block")

/// Breakpoint block.
#define BREAKPOINT_BLOCK N_(L"block created by breakpoint")

/// If block description.
#define IF_BLOCK N_(L"'if' conditional block")

/// Function invocation block description.
#define FUNCTION_CALL_BLOCK N_(L"function invocation block")

/// Function invocation block description.
#define FUNCTION_CALL_NO_SHADOW_BLOCK N_(L"function invocation block with no variable shadowing")

/// Switch block description.
#define SWITCH_BLOCK N_(L"'switch' block")

/// Top block description.
#define TOP_BLOCK N_(L"global root block")

/// Command substitution block description.
#define SUBST_BLOCK N_(L"command substitution block")

/// Begin block description.
#define BEGIN_BLOCK N_(L"'begin' unconditional block")

/// Source block description.
#define SOURCE_BLOCK N_(L"block created by the . builtin")

/// Source block description.
#define EVENT_BLOCK N_(L"event handler block")

/// Unknown block description.
#define UNKNOWN_BLOCK N_(L"unknown/invalid block")

/// Data structure to describe a block type, like while blocks, command substitution blocks, etc.
struct block_lookup_entry {
    // The block type id. The legal values are defined in parser.h.
    block_type_t type;
    // The name of the builtin that creates this type of block, if any.
    const wchar_t *name;
    // A description of this block type.
    const wchar_t *desc;
};

/// List of all legal block types.
static const struct block_lookup_entry block_lookup[] = {
    {WHILE, L"while", WHILE_BLOCK},
    {FOR, L"for", FOR_BLOCK},
    {IF, L"if", IF_BLOCK},
    {FUNCTION_CALL, nullptr, FUNCTION_CALL_BLOCK},
    {FUNCTION_CALL_NO_SHADOW, nullptr, FUNCTION_CALL_NO_SHADOW_BLOCK},
    {SWITCH, L"switch", SWITCH_BLOCK},
    {TOP, nullptr, TOP_BLOCK},
    {SUBST, nullptr, SUBST_BLOCK},
    {BEGIN, L"begin", BEGIN_BLOCK},
    {SOURCE, L"source", SOURCE_BLOCK},
    {EVENT, nullptr, EVENT_BLOCK},
    {BREAKPOINT, L"breakpoint", BREAKPOINT_BLOCK},
    {static_cast<block_type_t>(0), nullptr, nullptr}};

// Given a file path, return something nicer. Currently we just "unexpand" tildes.
wcstring parser_t::user_presentable_path(const wcstring &path) const {
    return replace_home_directory_with_tilde(path, vars());
}

parser_t::parser_t(std::shared_ptr<env_stack_t> vars) : variables(std::move(vars)) {
    assert(variables.get() && "Null variables in parser initializer");
    int cwd = open_cloexec(".", O_RDONLY);
    if (cwd < 0) {
        perror("Unable to open the current working directory");
        abort();
    }
    libdata().cwd_fd = std::make_shared<const autoclose_fd_t>(cwd);
}

parser_t::parser_t() : parser_t(env_stack_t::principal_ref()) {}

// Out of line destructor to enable forward declaration of parse_execution_context_t
parser_t::~parser_t() = default;

std::shared_ptr<parser_t> parser_t::principal{new parser_t()};

parser_t &parser_t::principal_parser() {
    ASSERT_IS_MAIN_THREAD();
    return *principal;
}

void parser_t::skip_all_blocks() {
    // Tell all blocks to skip.
    // This may be called from a signal handler!
    principal->cancellation_requested = true;
}

// Given a new-allocated block, push it onto our block stack, acquiring ownership
block_t *parser_t::push_block(block_t &&block) {
    block_t new_current{std::move(block)};
    const enum block_type_t type = new_current.type();
    new_current.src_lineno = parser_t::get_lineno();

    const wchar_t *filename = parser_t::current_filename();
    if (filename != nullptr) {
        new_current.src_filename = intern(filename);
    }

    // Types TOP and SUBST are not considered blocks for the purposes of `status is-block`.
    if (type != TOP && type != SUBST) {
        libdata().is_block = true;
    }

    if (type == BREAKPOINT) {
        libdata().is_breakpoint = true;
    }

    if (new_current.type() != TOP) {
        vars().push(type == FUNCTION_CALL);
        new_current.wants_pop_env = true;
    }

    // Push it onto our stack and return a pointer to it.
    // Note that deques do not move their contents so this is safe.
    this->block_stack.push_back(std::move(new_current));
    return &this->block_stack.back();
}

void parser_t::pop_block(const block_t *expected) {
    assert(expected == this->current_block());
    if (block_stack.empty()) {
        debug(1, L"function %s called on empty block stack.", __func__);
        bugreport();
        return;
    }

    // Acquire ownership out of the block stack.
    block_t old = std::move(block_stack.back());
    block_stack.pop_back();

    if (old.wants_pop_env) vars().pop();

    // Figure out if `status is-block` should consider us to be in a block now.
    bool new_is_block = false;
    for (const auto &b : block_stack) {
        const enum block_type_t type = b.type();
        if (type != TOP && type != SUBST) {
            new_is_block = true;
            break;
        }
    }
    libdata().is_block = new_is_block;

    // Are we still in a breakpoint?
    bool new_is_breakpoint = false;
    for (const auto &b : block_stack) {
        const enum block_type_t type = b.type();
        if (type == BREAKPOINT) {
            new_is_breakpoint = true;
            break;
        }
    }
    libdata().is_breakpoint = new_is_breakpoint;
}

const wchar_t *parser_t::get_block_desc(int block) const {
    for (size_t i = 0; block_lookup[i].desc; i++) {
        if (block_lookup[i].type == block) {
            return _(block_lookup[i].desc);
        }
    }
    return _(UNKNOWN_BLOCK);
}

#if 0
// TODO: Lint says this isn't used (which is true). Should this be removed?
wcstring parser_t::block_stack_description() const {
    wcstring result;
    size_t idx = this->block_count();
    size_t spaces = 0;
    while (idx--) {
        if (spaces > 0) {
            result.push_back(L'\n');
        }
        for (size_t j = 0; j < spaces; j++) {
            result.push_back(L' ');
        }
        result.append(this->block_at_index(idx)->description());
        spaces++;
    }
    return result;
}
#endif

const block_t *parser_t::block_at_index(size_t idx) const {
    // Zero corresponds to the last element in our vector.
    size_t count = block_stack.size();
    return idx < count ? &block_stack.at(count - idx - 1) : nullptr;
}

block_t *parser_t::block_at_index(size_t idx) {
    size_t count = block_stack.size();
    return idx < count ? &block_stack.at(count - idx - 1) : nullptr;
}

block_t *parser_t::current_block() { return block_stack.empty() ? nullptr : &block_stack.back(); }

/// Print profiling information to the specified stream.
static void print_profile(const std::vector<std::unique_ptr<profile_item_t>> &items, FILE *out) {
    for (size_t pos = 0; pos < items.size(); pos++) {
        const profile_item_t *me, *prev;
        size_t i;
        int my_time;

        me = items.at(pos).get();
        if (me->skipped) {
            continue;
        }

        my_time = me->parse + me->exec;
        for (i = pos + 1; i < items.size(); i++) {
            prev = items.at(i).get();
            if (prev->skipped) {
                continue;
            }
            if (prev->level <= me->level) {
                break;
            }
            if (prev->level > me->level + 1) {
                continue;
            }

            my_time -= prev->parse + prev->exec;
        }

        if (me->cmd.empty()) {
            continue;
        }

        if (std::fwprintf(out, L"%d\t%d\t", my_time, me->parse + me->exec) < 0) {
            wperror(L"fwprintf");
            return;
        }

        for (i = 0; i < me->level; i++) {
            if (std::fwprintf(out, L"-") < 0) {
                wperror(L"fwprintf");
                return;
            }
        }

        if (std::fwprintf(out, L"> %ls\n", me->cmd.c_str()) < 0) {
            wperror(L"fwprintf");
            return;
        }
    }
}

void parser_t::emit_profiling(const char *path) const {
    // Save profiling information. OK to not use CLO_EXEC here because this is called while fish is
    // dying (and hence will not fork).
    FILE *f = fopen(path, "w");
    if (!f) {
        debug(1, _(L"Could not write profiling information to file '%s'"), path);
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

std::vector<completion_t> parser_t::expand_argument_list(const wcstring &arg_list_src,
                                                         expand_flags_t eflags,
                                                         const environment_t &vars,
                                                         const std::shared_ptr<parser_t> &parser) {
    // Parse the string as an argument list.
    parse_node_tree_t tree;
    if (!parse_tree_from_string(arg_list_src, parse_flag_none, &tree, nullptr /* errors */,
                                symbol_freestanding_argument_list)) {
        // Failed to parse. Here we expect to have reported any errors in test_args.
        return {};
    }

    // Get the root argument list and extract arguments from it.
    std::vector<completion_t> result;
    assert(!tree.empty());
    tnode_t<grammar::freestanding_argument_list> arg_list(&tree, &tree.at(0));
    while (auto arg = arg_list.next_in_list<grammar::argument>()) {
        const wcstring arg_src = arg.get_source(arg_list_src);
        if (expand_string(arg_src, &result, eflags, vars, parser, nullptr /* errors */) ==
            expand_result_t::error) {
            break;  // failed to expand a string
        }
    }
    return result;
}

wcstring parser_t::stack_trace() const {
    wcstring trace;
    this->stack_trace_internal(0, &trace);
    return trace;
}

std::shared_ptr<parser_t> parser_t::shared() { return shared_from_this(); }

void parser_t::stack_trace_internal(size_t block_idx, wcstring *buff) const {
    // Check if we should end the recursion.
    if (block_idx >= this->block_count()) return;

    const block_t *b = this->block_at_index(block_idx);

    if (b->type() == EVENT) {
        // This is an event handler.
        assert(b->event && "Should have an event");
        wcstring description = event_get_desc(*b->event);
        append_format(*buff, _(L"in event handler: %ls\n"), description.c_str());

        // Stop recursing at event handler. No reason to believe that any other code is relevant.
        //
        // It might make sense in the future to continue printing the stack trace of the code that
        // invoked the event, if this is a programmatic event, but we can't currently detect that.
        return;
    }

    if (b->type() == FUNCTION_CALL || b->type() == FUNCTION_CALL_NO_SHADOW || b->type() == SOURCE ||
        b->type() == SUBST) {
        // These types of blocks should be printed.
        switch (b->type()) {
            case SOURCE: {
                const wchar_t *source_dest = b->sourced_file;
                append_format(*buff, _(L"from sourcing file %ls\n"),
                              user_presentable_path(source_dest).c_str());
                break;
            }
            case FUNCTION_CALL:
            case FUNCTION_CALL_NO_SHADOW: {
                append_format(*buff, _(L"in function '%ls'"), b->function_name.c_str());
                // Print arguments on the same line.
                wcstring args_str;
                for (const wcstring &arg : b->function_args) {
                    if (!args_str.empty()) args_str.push_back(L' ');
                    // We can't quote the arguments because we print this in quotes.
                    // As a special-case, add the empty argument as "".
                    if (!arg.empty()) {
                        args_str.append(escape_string(arg, ESCAPE_ALL | ESCAPE_NO_QUOTED));
                    } else {
                        args_str.append(L"\"\"");
                    }
                }
                if (!args_str.empty()) {
                    // TODO: Escape these.
                    append_format(*buff, _(L" with arguments '%ls'"), args_str.c_str());
                }
                buff->push_back('\n');
                break;
            }
            case SUBST: {
                append_format(*buff, _(L"in command substitution\n"));
                break;
            }
            default: {
                break;  // can't get here
            }
        }

        // Print where the function is called.
        const wchar_t *file = b->src_filename;

        if (file) {
            append_format(*buff, _(L"\tcalled on line %d of file %ls\n"), b->src_lineno,
                          user_presentable_path(file).c_str());
        } else if (is_within_fish_initialization()) {
            append_format(*buff, _(L"\tcalled during startup\n"));
        } else {
            // This one is way too noisy
            // append_format(*buff, _(L"\tcalled on standard input\n"));
        }
    }

    // Recursively print the next block.
    parser_t::stack_trace_internal(block_idx + 1, buff);
}

/// Returns the name of the currently evaluated function if we are currently evaluating a function,
/// NULL otherwise. This is tested by moving down the block-scope-stack, checking every block if it
/// is of type FUNCTION_CALL. If the caller doesn't specify a starting position in the stack we
/// begin with the current block.
const wchar_t *parser_t::is_function(size_t idx) const {
    // PCA: Have to make this a string somehow.
    ASSERT_IS_MAIN_THREAD();

    const wchar_t *result = nullptr;
    for (size_t block_idx = idx; block_idx < this->block_count(); block_idx++) {
        const block_t *b = this->block_at_index(block_idx);
        if (b->type() == FUNCTION_CALL || b->type() == FUNCTION_CALL_NO_SHADOW) {
            result = b->function_name.c_str();
            break;
        } else if (b->type() == SOURCE) {
            // If a function sources a file, obviously that function's offset doesn't contribute.
            break;
        }
    }
    return result;
}

/// Return the function name for the specified stack frame. Default is zero (current frame).
/// The special value zero means the function frame immediately above the closest breakpoint frame.
const wchar_t *parser_t::get_function_name(int level) {
    if (level == 0) {
        // Return the function name for the level preceding the most recent breakpoint. If there
        // isn't one return the function name for the current level.
        int idx = 0;
        for (const auto &b : block_stack) {
            const enum block_type_t type = b.type();
            if (type == BREAKPOINT) {
                return this->is_function(idx);
            }
            idx++;
        }
        return nullptr;  // couldn't find a breakpoint frame
    } else if (level == 1) {
        // Return the function name for the current level.
        return this->is_function();
    }

    // Return the function name for the specific function stack frame.
    int idx = 0;
    for (const auto &b : block_stack) {
        const enum block_type_t type = b.type();
        if (type == FUNCTION_CALL || type == FUNCTION_CALL_NO_SHADOW) {
            if (--level == 0) return this->is_function(idx);
        }
        idx++;
    }
    return nullptr;  // couldn't find that function level
}

int parser_t::get_lineno() const {
    int lineno = -1;
    if (execution_context) {
        lineno = execution_context->get_current_line_number();

        // If we are executing a function, we have to add in its offset.
        const wchar_t *function_name = is_function();
        if (function_name != nullptr) {
            lineno += function_get_definition_lineno(function_name);
        }
    }
    return lineno;
}

const wchar_t *parser_t::current_filename() const {
    ASSERT_IS_MAIN_THREAD();

    for (size_t i = 0; i < this->block_count(); i++) {
        const block_t *b = this->block_at_index(i);
        if (b->type() == FUNCTION_CALL || b->type() == FUNCTION_CALL_NO_SHADOW) {
            return function_get_definition_file(b->function_name);
        } else if (b->type() == SOURCE) {
            return b->sourced_file;
        }
    }

    // Fall back to the file being sourced.
    return libdata().current_filename;
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
    for (const auto &b : block_stack) {
        depth += (b.type() == FUNCTION_CALL || b.type() == FUNCTION_CALL_NO_SHADOW);
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
    const wchar_t *file = this->current_filename();

    wcstring prefix;

    // If we are not going to print a stack trace, at least print the line number and filename.
    if (!is_interactive() || is_function()) {
        if (file) {
            append_format(prefix, _(L"%ls (line %d): "), user_presentable_path(file).c_str(),
                          lineno);
        } else if (is_within_fish_initialization()) {
            append_format(prefix, L"%ls (line %d): ", _(L"Startup"), lineno);
        } else {
            append_format(prefix, L"%ls (line %d): ", _(L"Standard input"), lineno);
        }
    }

    bool skip_caret = is_interactive() && !is_function();

    // Use an error with empty text.
    assert(source_offset >= 0);
    parse_error_t empty_error = {};
    empty_error.source_start = source_offset;

    wcstring line_info = empty_error.describe_with_prefix(execution_context->get_source(), prefix,
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
    job_list.push_front(std::move(job));
}

void parser_t::job_promote(job_t *job) {
    job_list_t::iterator loc;
    for (loc = job_list.begin(); loc != job_list.end(); ++loc) {
        if (loc->get() == job) {
            break;
        }
    }
    assert(loc != job_list.end());

    // Move the job to the beginning.
    std::rotate(job_list.begin(), loc, job_list.end());
}

job_t *parser_t::job_get(job_id_t id) {
    for (const auto &job : job_list) {
        if (id <= 0 || job->job_id == id) return job.get();
    }
    return nullptr;
}

job_t *parser_t::job_get_from_pid(pid_t pid) const {
    pid_t pgid = getpgid(pid);

    if (pgid == -1) {
        return nullptr;
    }

    for (const auto &job : jobs()) {
        if (job->pgid == pgid) {
            for (const process_ptr_t &p : job->processes) {
                if (p->pid == pid) {
                    return job.get();
                }
            }
        }
    }
    return nullptr;
}

profile_item_t *parser_t::create_profile_item() {
    profile_item_t *result = nullptr;
    if (g_profiling_active) {
        profile_items.push_back(make_unique<profile_item_t>());
        result = profile_items.back().get();
    }
    return result;
}

int parser_t::eval(wcstring cmd, const io_chain_t &io, enum block_type_t block_type) {
    // Parse the source into a tree, if we can.
    parse_error_list_t error_list;
    parsed_source_ref_t ps = parse_source(cmd, parse_flag_none, &error_list);
    if (!ps) {
        // Get a backtrace. This includes the message.
        wcstring backtrace_and_desc;
        this->get_backtrace(cmd, error_list, backtrace_and_desc);

        // Print it.
        std::fwprintf(stderr, L"%ls\n", backtrace_and_desc.c_str());
        return 1;
    }
    return this->eval(ps, io, block_type);
}

int parser_t::eval(parsed_source_ref_t ps, const io_chain_t &io, enum block_type_t block_type) {
    assert(block_type == TOP || block_type == SUBST);
    if (!ps->tree.empty()) {
        // Execute the first node.
        tnode_t<grammar::job_list> start{&ps->tree, &ps->tree.front()};
        return this->eval_node(ps, start, io, block_type, nullptr /* parent */);
    }
    return 0;
}

template <typename T>
int parser_t::eval_node(parsed_source_ref_t ps, tnode_t<T> node, const io_chain_t &io,
                        block_type_t block_type, std::shared_ptr<job_t> parent_job) {
    static_assert(
        std::is_same<T, grammar::statement>::value || std::is_same<T, grammar::job_list>::value,
        "Unexpected node type");
    // Handle cancellation requests. If our block stack is currently empty, then we already did
    // successfully cancel (or there was nothing to cancel); clear the flag. If our block stack is
    // not empty, we are still in the process of cancelling; refuse to evaluate anything.
    if (this->cancellation_requested) {
        if (!block_stack.empty()) {
            return 1;
        }
        this->cancellation_requested = false;
    }

    // Only certain blocks are allowed.
    if ((block_type != TOP) && (block_type != SUBST)) {
        debug(1, INVALID_SCOPE_ERR_MSG, parser_t::get_block_desc(block_type));
        bugreport();
        return 1;
    }

    job_reap(*this, false);  // not sure why we reap jobs here

    // Start it up
    block_t *scope_block = this->push_block(block_t::scope_block(block_type));

    // Create and set a new execution context.
    using exc_ctx_ref_t = std::unique_ptr<parse_execution_context_t>;
    scoped_push<exc_ctx_ref_t> exc(&execution_context,
                                   make_unique<parse_execution_context_t>(ps, this, parent_job));
    int result = execution_context->eval_node(node, scope_block, io);
    exc.restore();
    this->pop_block(scope_block);

    job_reap(*this, false);  // reap again
    return result;
}

// Explicit instantiations. TODO: use overloads instead?
template int parser_t::eval_node(parsed_source_ref_t, tnode_t<grammar::statement>,
                                 const io_chain_t &, enum block_type_t,
                                 std::shared_ptr<job_t> parent_job);
template int parser_t::eval_node(parsed_source_ref_t, tnode_t<grammar::job_list>,
                                 const io_chain_t &, enum block_type_t,
                                 std::shared_ptr<job_t> parent_job);

void parser_t::get_backtrace(const wcstring &src, const parse_error_list_t &errors,
                             wcstring &output) const {
    if (!errors.empty()) {
        const parse_error_t &err = errors.at(0);

        // Determine if we want to try to print a caret to point at the source error. The
        // err.source_start <= src.size() check is due to the nasty way that slices work, which is
        // by rewriting the source.
        size_t which_line = 0;
        bool skip_caret = true;
        if (err.source_start != SOURCE_LOCATION_UNKNOWN && err.source_start <= src.size()) {
            // Determine which line we're on.
            which_line = 1 + std::count(src.begin(), src.begin() + err.source_start, L'\n');

            // Don't include the caret if we're interactive, this is the first line of text, and our
            // source is at its beginning, because then it's obvious.
            skip_caret = (is_interactive() && which_line == 1 && err.source_start == 0);
        }

        wcstring prefix;
        const wchar_t *filename = this->current_filename();
        if (filename) {
            if (which_line > 0) {
                prefix = format_string(_(L"%ls (line %lu): "),
                                       user_presentable_path(filename).c_str(), which_line);
            } else {
                prefix = format_string(_(L"%ls: "), user_presentable_path(filename).c_str());
            }
        } else {
            prefix = L"fish: ";
        }

        const wcstring description =
            err.describe_with_prefix(src, prefix, is_interactive(), skip_caret);
        if (!description.empty()) {
            output.append(description);
            output.push_back(L'\n');
        }
        output.append(this->stack_trace());
    }
}

block_t::block_t(block_type_t t) : block_type(t) {}

block_t::~block_t() = default;

wcstring block_t::description() const {
    wcstring result;
    switch (this->type()) {
        case WHILE: {
            result.append(L"while");
            break;
        }
        case FOR: {
            result.append(L"for");
            break;
        }
        case IF: {
            result.append(L"if");
            break;
        }
        case FUNCTION_CALL: {
            result.append(L"function_call");
            break;
        }
        case FUNCTION_CALL_NO_SHADOW: {
            result.append(L"function_call_no_shadow");
            break;
        }
        case SWITCH: {
            result.append(L"switch");
            break;
        }
        case SUBST: {
            result.append(L"substitution");
            break;
        }
        case TOP: {
            result.append(L"top");
            break;
        }
        case BEGIN: {
            result.append(L"begin");
            break;
        }
        case SOURCE: {
            result.append(L"source");
            break;
        }
        case EVENT: {
            result.append(L"event");
            break;
        }
        case BREAKPOINT: {
            result.append(L"breakpoint");
            break;
        }
    }

    if (this->src_lineno >= 0) {
        append_format(result, L" (line %d)", this->src_lineno);
    }
    if (this->src_filename != nullptr) {
        append_format(result, L" (file %ls)", this->src_filename);
    }
    return result;
}

// Various block constructors.

block_t block_t::if_block() { return block_t(IF); }

block_t block_t::event_block(event_t evt) {
    block_t b{EVENT};
    b.event = std::move(evt);
    return b;
}

block_t block_t::function_block(wcstring name, wcstring_list_t args, bool shadows) {
    block_t b{shadows ? FUNCTION_CALL : FUNCTION_CALL_NO_SHADOW};
    b.function_name = std::move(name);
    b.function_args = std::move(args);
    return b;
}

block_t block_t::source_block(const wchar_t *src) {
    block_t b{SOURCE};
    b.sourced_file = src;
    return b;
}

block_t block_t::for_block() { return block_t{FOR}; }
block_t block_t::while_block() { return block_t{WHILE}; }
block_t block_t::switch_block() { return block_t{SWITCH}; }
block_t block_t::scope_block(block_type_t type) {
    assert((type == BEGIN || type == TOP || type == SUBST) && "Invalid scope type");
    return block_t(type);
}
block_t block_t::breakpoint_block() { return block_t(BREAKPOINT); }
