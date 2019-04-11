// Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.)
//
// A note on error handling: fish has two kind of errors, fatal parse errors non-fatal runtime
// errors. A fatal error prevents execution of the entire file, while a non-fatal error skips that
// job.
//
// Non-fatal errors are printed as soon as they are encountered; otherwise you would have to wait
// for the execution to finish to see them.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <cwchar>
#include <wctype.h>

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "builtin.h"
#include "builtin_function.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "exec.h"
#include "expand.h"
#include "function.h"
#include "io.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parse_execution.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "tnode.h"
#include "tokenizer.h"
#include "util.h"
#include "wildcard.h"
#include "wutil.h"

namespace g = grammar;

/// These are the specific statement types that support redirections.
static constexpr bool type_is_redirectable_block(parse_token_type_t type) {
    return type == symbol_block_statement || type == symbol_if_statement ||
           type == symbol_switch_statement;
}

static bool specific_statement_type_is_redirectable_block(const parse_node_t &node) {
    return type_is_redirectable_block(node.type);
}

/// Get the name of a redirectable block, for profiling purposes.
static wcstring profiling_cmd_name_for_redirectable_block(const parse_node_t &node,
                                                          const parse_node_tree_t &tree,
                                                          const wcstring &src) {
    assert(specific_statement_type_is_redirectable_block(node));
    assert(node.has_source());

    // Get the source for the block, and cut it at the next statement terminator.
    const size_t src_start = node.source_start;

    auto term = tree.find_child<g::end_command>(node);
    assert(term.has_source() && term.source_range()->start >= src_start);
    size_t src_len = term.source_range()->start - src_start;

    wcstring result = wcstring(src, src_start, src_len);
    result.append(L"...");
    return result;
}

parse_execution_context_t::parse_execution_context_t(parsed_source_ref_t pstree, parser_t *p,
                                                     std::shared_ptr<job_t> parent)
    : pstree(std::move(pstree)), parser(p), parent_job(std::move(parent)) {}

// Utilities

wcstring parse_execution_context_t::get_source(const parse_node_t &node) const {
    return node.get_source(pstree->src);
}

tnode_t<g::plain_statement> parse_execution_context_t::infinite_recursive_statement_in_job_list(
    tnode_t<g::job_list> job_list, wcstring *out_func_name) const {
    // This is a bit fragile. It is a test to see if we are inside of function call, but not inside
    // a block in that function call. If, in the future, the rules for what block scopes are pushed
    // on function invocation changes, then this check will break.
    const block_t *current = parser->block_at_index(0), *parent = parser->block_at_index(1);
    bool is_within_function_call =
        (current && parent && current->type() == TOP && parent->type() == FUNCTION_CALL);
    if (!is_within_function_call) {
        return {};
    }

    // Check to see which function call is forbidden.
    if (parser->forbidden_function.empty()) {
        return {};
    }
    const wcstring &forbidden_function_name = parser->forbidden_function.back();

    // Get the first job in the job list.
    tnode_t<g::job> first_job = job_list.try_get_child<g::job_conjunction, 1>().child<0>();
    if (!first_job) {
        return {};
    }

    // Here's the statement node we find that's infinite recursive.
    tnode_t<grammar::plain_statement> infinite_recursive_statement;

    // Get the list of plain statements.
    // Ignore statements with decorations like 'builtin' or 'command', since those
    // are not infinite recursion. In particular that is what enables 'wrapper functions'.
    tnode_t<g::statement> statement = first_job.child<0>();
    tnode_t<g::job_continuation> continuation = first_job.child<1>();
    const null_environment_t nullenv{};
    while (statement) {
        tnode_t<g::plain_statement> plain_statement =
            statement.try_get_child<g::decorated_statement, 0>()
                .try_get_child<g::plain_statement, 0>();
        if (plain_statement) {
            maybe_t<wcstring> cmd = command_for_plain_statement(plain_statement, pstree->src);
            if (cmd && expand_one(*cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES, nullenv) &&
                cmd == forbidden_function_name) {
                // This is it.
                infinite_recursive_statement = plain_statement;
                if (out_func_name != NULL) {
                    *out_func_name = forbidden_function_name;
                }
                break;
            }
        }
        statement = continuation.next_in_list<g::statement>();
    }

    return infinite_recursive_statement;
}

process_type_t parse_execution_context_t::process_type_for_command(
    tnode_t<grammar::plain_statement> statement, const wcstring &cmd) const {
    enum process_type_t process_type = process_type_t::external;

    // Determine the process type, which depends on the statement decoration (command, builtin,
    // etc).
    enum parse_statement_decoration_t decoration = get_decoration(statement);

    switch (decoration) {
        case parse_statement_decoration_exec:
            process_type = process_type_t::exec;
            break;
        case parse_statement_decoration_command:
            process_type = process_type_t::external;
            break;
        case parse_statement_decoration_builtin:
            process_type = process_type_t::builtin;
            break;
        case parse_statement_decoration_eval:
            process_type = process_type_t::eval;
            break;
        case parse_statement_decoration_none:
            if (function_exists(cmd)) {
                process_type = process_type_t::function;
            } else if (builtin_exists(cmd)) {
                process_type = process_type_t::builtin;
            } else {
                process_type = process_type_t::external;
            }
            break;
    }

    return process_type;
}

bool parse_execution_context_t::should_cancel_execution(const block_t *block) const {
    return cancellation_reason(block) != execution_cancellation_none;
}

parse_execution_context_t::execution_cancellation_reason_t
parse_execution_context_t::cancellation_reason(const block_t *block) const {
    if (shell_is_exiting()) {
        return execution_cancellation_exit;
    }
    if (parser && parser->cancellation_requested) {
        return execution_cancellation_skip;
    }
    if (block && block->skip) {
        return execution_cancellation_skip;
    }
    if (block && block->loop_status != LOOP_NORMAL) {
        return execution_cancellation_loop_control;
    }
    return execution_cancellation_none;
}

/// Return whether the job contains a single statement, of block type, with no redirections.
bool parse_execution_context_t::job_is_simple_block(tnode_t<g::job> job_node) const {
    tnode_t<g::statement> statement = job_node.child<0>();

    // Must be no pipes.
    if (job_node.child<1>().try_get_child<g::tok_pipe, 0>()) {
        return false;
    }

    // Helper to check if an argument or redirection list has no redirections.
    auto is_empty = [](tnode_t<g::arguments_or_redirections_list> lst) -> bool {
        return !lst.next_in_list<g::redirection>();
    };

    // Check if we're a block statement with redirections. We do it this obnoxious way to preserve
    // type safety (in case we add more specific statement types).
    const parse_node_t &specific_statement = statement.get_child_node<0>();
    switch (specific_statement.type) {
        case symbol_block_statement:
            return is_empty(statement.require_get_child<g::block_statement, 0>().child<3>());
        case symbol_switch_statement:
            return is_empty(statement.require_get_child<g::switch_statement, 0>().child<5>());
        case symbol_if_statement:
            return is_empty(statement.require_get_child<g::if_statement, 0>().child<3>());
        case symbol_not_statement:
        case symbol_decorated_statement:
            // not block statements
            return false;
        default:
            assert(0 && "Unexpected child block type");
            return false;
    }
}

parse_execution_result_t parse_execution_context_t::run_if_statement(
    tnode_t<g::if_statement> statement, const block_t *associated_block) {
    parse_execution_result_t result = parse_execution_success;

    // We have a sequence of if clauses, with a final else, resulting in a single job list that we
    // execute.
    tnode_t<g::job_list> job_list_to_execute;
    tnode_t<g::if_clause> if_clause = statement.child<0>();
    tnode_t<g::else_clause> else_clause = statement.child<1>();
    for (;;) {
        if (should_cancel_execution(associated_block)) {
            result = parse_execution_cancelled;
            break;
        }

        // An if condition has a job and a "tail" of andor jobs, e.g. "foo ; and bar; or baz".
        tnode_t<g::job_conjunction> condition_head = if_clause.child<1>();
        tnode_t<g::andor_job_list> condition_boolean_tail = if_clause.child<3>();

        // Check the condition and the tail. We treat parse_execution_errored here as failure, in
        // accordance with historic behavior.
        parse_execution_result_t cond_ret = run_job_conjunction(condition_head, associated_block);
        if (cond_ret == parse_execution_success) {
            cond_ret = run_job_list(condition_boolean_tail, associated_block);
        }
        const bool take_branch =
            (cond_ret == parse_execution_success) && proc_get_last_status() == EXIT_SUCCESS;

        if (take_branch) {
            // Condition succeeded.
            job_list_to_execute = if_clause.child<4>();
            break;
        }
        auto else_cont = else_clause.try_get_child<g::else_continuation, 1>();
        if (!else_cont) {
            // 'if' condition failed, no else clause, return 0, we're done.
            proc_set_last_statuses(statuses_t::just(STATUS_CMD_OK));
            break;
        } else {
            // We have an 'else continuation' (either else-if or else).
            if (auto maybe_if_clause = else_cont.try_get_child<g::if_clause, 0>()) {
                // it's an 'else if', go to the next one.
                if_clause = maybe_if_clause;
                else_clause = else_cont.try_get_child<g::else_clause, 1>();
                assert(else_clause && "Expected to have an else clause");
            } else {
                // It's the final 'else', we're done.
                job_list_to_execute = else_cont.try_get_child<g::job_list, 1>();
                assert(job_list_to_execute && "Should have a job list");
                break;
            }
        }
    }

    // Execute any job list we got.
    if (job_list_to_execute) {
        if_block_t *ib = parser->push_block<if_block_t>();
        run_job_list(job_list_to_execute, ib);
        if (should_cancel_execution(ib)) {
            result = parse_execution_cancelled;
        }
        parser->pop_block(ib);
    } else {
        // No job list means no sucessful conditions, so return 0 (issue #1443).
        proc_set_last_statuses(statuses_t::just(STATUS_CMD_OK));
    }

    // It's possible there's a last-minute cancellation (issue #1297).
    if (should_cancel_execution(associated_block)) {
        result = parse_execution_cancelled;
    }

    // Otherwise, take the exit status of the job list. Reversal of issue #1061.
    return result;
}

parse_execution_result_t parse_execution_context_t::run_begin_statement(
    tnode_t<g::job_list> contents) {
    // Basic begin/end block. Push a scope block, run jobs, pop it
    scope_block_t *sb = parser->push_block<scope_block_t>(BEGIN);
    parse_execution_result_t ret = run_job_list(contents, sb);
    parser->pop_block(sb);

    return ret;
}

// Define a function.
parse_execution_result_t parse_execution_context_t::run_function_statement(
    tnode_t<g::function_header> header, tnode_t<g::job_list> body) {
    // Get arguments.
    wcstring_list_t arguments;
    argument_node_list_t arg_nodes = header.descendants<g::argument>();
    parse_execution_result_t result =
        this->expand_arguments_from_nodes(arg_nodes, &arguments, failglob);

    if (result != parse_execution_success) {
        return result;
    }
    io_streams_t streams(0);  // no limit on the amount of output from builtin_function()
    int err = builtin_function(*parser, streams, arguments, pstree, body);
    proc_set_last_statuses(statuses_t::just(err));

    if (!streams.err.empty()) {
        this->report_error(header, L"%ls", streams.err.contents().c_str());
        result = parse_execution_errored;
    }

    return result;
}

parse_execution_result_t parse_execution_context_t::run_block_statement(
    tnode_t<g::block_statement> statement, const block_t *associated_block) {
    tnode_t<g::block_header> bheader = statement.child<0>();
    tnode_t<g::job_list> contents = statement.child<1>();

    parse_execution_result_t ret = parse_execution_success;
    if (auto header = bheader.try_get_child<g::for_header, 0>()) {
        ret = run_for_statement(header, contents);
    } else if (auto header = bheader.try_get_child<g::while_header, 0>()) {
        ret = run_while_statement(header, contents, associated_block);
    } else if (auto header = bheader.try_get_child<g::function_header, 0>()) {
        ret = run_function_statement(header, contents);
    } else if (auto header = bheader.try_get_child<g::begin_header, 0>()) {
        ret = run_begin_statement(contents);
    } else {
        debug(0, L"Unexpected block header: %ls\n", bheader.node()->describe().c_str());
        PARSER_DIE();
    }
    return ret;
}

/// Return true if the current execution context is within a function block, else false.
bool parse_execution_context_t::is_function_context() const {
    const block_t *current = parser->block_at_index(0);
    const block_t *parent = parser->block_at_index(1);
    bool is_within_function_call =
        (current && parent && current->type() == TOP && parent->type() == FUNCTION_CALL);
    return is_within_function_call;
}

parse_execution_result_t parse_execution_context_t::run_for_statement(
    tnode_t<grammar::for_header> header, tnode_t<grammar::job_list> block_contents) {
    // Get the variable name: `for var_name in ...`. We expand the variable name. It better result
    // in just one.
    tnode_t<g::tok_string> var_name_node = header.child<1>();
    wcstring for_var_name = get_source(var_name_node);
    if (!expand_one(for_var_name, 0, parser->vars())) {
        report_error(var_name_node, FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG, for_var_name.c_str());
        return parse_execution_errored;
    }

    // Get the contents to iterate over.
    wcstring_list_t arguments;
    parse_execution_result_t ret = this->expand_arguments_from_nodes(
        get_argument_nodes(header.child<3>()), &arguments, nullglob);
    if (ret != parse_execution_success) {
        return ret;
    }

    auto &vars = parser->vars();
    auto var = vars.get(for_var_name, ENV_LOCAL);
    if (!var && !is_function_context()) var = vars.get(for_var_name, ENV_DEFAULT);
    if (!var || var->read_only()) {
        int retval = parser->vars().set_empty(for_var_name, ENV_LOCAL | ENV_USER);
        if (retval != ENV_OK) {
            report_error(var_name_node, L"You cannot use read-only variable '%ls' in a for loop",
                         for_var_name.c_str());
            return parse_execution_errored;
        }
    }

    if (!valid_var_name(for_var_name)) {
        report_error(var_name_node, BUILTIN_ERR_VARNAME, L"for", for_var_name.c_str());
        return parse_execution_errored;
    }

    for_block_t *fb = parser->push_block<for_block_t>();

    // Now drive the for loop.
    for (const wcstring &val : arguments) {
        if (should_cancel_execution(fb)) {
            ret = parse_execution_cancelled;
            break;
        }

        int retval = parser->vars().set_one(for_var_name, ENV_DEFAULT | ENV_USER, val);
        assert(retval == ENV_OK && "for loop variable should have been successfully set");
        (void)retval;

        fb->loop_status = LOOP_NORMAL;
        this->run_job_list(block_contents, fb);

        if (this->cancellation_reason(fb) == execution_cancellation_loop_control) {
            // Handle break or continue.
            if (fb->loop_status == LOOP_CONTINUE) {
                // Reset the loop state.
                fb->loop_status = LOOP_NORMAL;
                continue;
            } else if (fb->loop_status == LOOP_BREAK) {
                break;
            }
        }
    }

    parser->pop_block(fb);
    return ret;
}

parse_execution_result_t parse_execution_context_t::run_switch_statement(
    tnode_t<grammar::switch_statement> statement) {
    parse_execution_result_t result = parse_execution_success;

    // Get the switch variable.
    tnode_t<grammar::argument> switch_value_n = statement.child<1>();
    const wcstring switch_value = get_source(switch_value_n);

    // Expand it. We need to offset any errors by the position of the string.
    std::vector<completion_t> switch_values_expanded;
    parse_error_list_t errors;
    int expand_ret = expand_string(switch_value, &switch_values_expanded, EXPAND_NO_DESCRIPTIONS,
                                   parser->vars(), &errors);
    parse_error_offset_source_start(&errors, switch_value_n.source_range()->start);

    switch (expand_ret) {
        case EXPAND_ERROR: {
            result = report_errors(errors);
            break;
        }
        case EXPAND_WILDCARD_NO_MATCH: {
            result = report_unmatched_wildcard_error(switch_value_n);
            break;
        }
        case EXPAND_WILDCARD_MATCH:
        case EXPAND_OK: {
            break;
        }
        default: {
            DIE("unexpected expand_string() return value");
            break;
        }
    }

    if (result == parse_execution_success && switch_values_expanded.size() != 1) {
        result =
            report_error(switch_value_n, _(L"switch: Expected exactly one argument, got %lu\n"),
                         switch_values_expanded.size());
    }

    if (result != parse_execution_success) {
        return result;
    }

    const wcstring &switch_value_expanded = switch_values_expanded.at(0).completion;

    switch_block_t *sb = parser->push_block<switch_block_t>();

    // Expand case statements.
    tnode_t<g::case_item_list> case_item_list = statement.child<3>();
    tnode_t<g::case_item> matching_case_item{};
    while (auto case_item = case_item_list.next_in_list<g::case_item>()) {
        if (should_cancel_execution(sb)) {
            result = parse_execution_cancelled;
            break;
        }

        // Expand arguments. A case item list may have a wildcard that fails to expand to
        // anything. We also report case errors, but don't stop execution; i.e. a case item that
        // contains an unexpandable process will report and then fail to match.
        auto arg_nodes = get_argument_nodes(case_item.child<1>());
        wcstring_list_t case_args;
        parse_execution_result_t case_result =
            this->expand_arguments_from_nodes(arg_nodes, &case_args, failglob);
        if (case_result == parse_execution_success) {
            for (const wcstring &arg : case_args) {
                // Unescape wildcards so they can be expanded again.
                wcstring unescaped_arg = parse_util_unescape_wildcards(arg);
                bool match = wildcard_match(switch_value_expanded, unescaped_arg);

                // If this matched, we're done.
                if (match) {
                    matching_case_item = case_item;
                    break;
                }
            }
        }
        if (matching_case_item) break;
    }

    if (matching_case_item) {
        // Success, evaluate the job list.
        assert(result == parse_execution_success && "Expected success");
        auto job_list = matching_case_item.child<3>();
        result = this->run_job_list(job_list, sb);
    }

    parser->pop_block(sb);
    return result;
}

parse_execution_result_t parse_execution_context_t::run_while_statement(
    tnode_t<grammar::while_header> header, tnode_t<grammar::job_list> contents,
    const block_t *associated_block) {
    parse_execution_result_t ret = parse_execution_success;

    // "The exit status of the while loop shall be the exit status of the last compound-list-2
    // executed, or zero if none was executed."
    // Here are more detailed requirements:
    // - If we execute the loop body zero times, or the loop body is empty, the status is success.
    // - An empty loop body is treated as true, both in the loop condition and after loop exit.
    // - The exit status of the last command is visible in the loop condition. (i.e. do not set the
    // exit status to true BEFORE executing the loop condition).
    // We achieve this by restoring the status if the loop condition fails, plus a special
    // affordance for the first condition.
    bool first_cond_check = true;

    // The conditions of the while loop.
    tnode_t<g::job_conjunction> condition_head = header.child<1>();
    tnode_t<g::andor_job_list> condition_boolean_tail = header.child<3>();

    // Run while the condition is true.
    for (;;) {
        // Save off the exit status if it came from the loop body. We'll restore it if the condition
        // is false.
        auto cond_saved_status =
            first_cond_check ? statuses_t::just(EXIT_SUCCESS) : proc_get_last_statuses();
        first_cond_check = false;

        // Check the condition.
        parse_execution_result_t cond_ret =
            this->run_job_conjunction(condition_head, associated_block);
        if (cond_ret == parse_execution_success) {
            cond_ret = run_job_list(condition_boolean_tail, associated_block);
        }

        // If the loop condition failed to execute, then exit the loop without modifying the exit
        // status. If the loop condition executed with a failure status, restore the status and then
        // exit the loop.
        if (cond_ret != parse_execution_success) {
            break;
        } else if (proc_get_last_status() != EXIT_SUCCESS) {
            proc_set_last_statuses(cond_saved_status);
            break;
        }

        // Check cancellation.
        if (this->should_cancel_execution(associated_block)) {
            ret = parse_execution_cancelled;
            break;
        }

        // Push a while block and then check its cancellation reason.
        while_block_t *wb = parser->push_block<while_block_t>();
        this->run_job_list(contents, wb);
        auto loop_status = wb->loop_status;
        auto cancel_reason = this->cancellation_reason(wb);
        parser->pop_block(wb);

        if (cancel_reason == execution_cancellation_loop_control) {
            // Handle break or continue.
            if (loop_status == LOOP_CONTINUE) {
                continue;
            } else if (loop_status == LOOP_BREAK) {
                break;
            }
        }

        // no_exec means that fish was invoked with -n or --no-execute. If set, we allow the loop to
        // not-execute once so its contents can be checked, and then break.
        if (no_exec) {
            break;
        }
    }
    return ret;
}

// Reports an error. Always returns parse_execution_errored, so you can assign the result to an
// 'errored' variable.
parse_execution_result_t parse_execution_context_t::report_error(const parse_node_t &node,
                                                                 const wchar_t *fmt, ...) const {
    // Create an error.
    parse_error_list_t error_list = parse_error_list_t(1);
    parse_error_t *error = &error_list.at(0);
    error->source_start = node.source_start;
    error->source_length = node.source_length;
    error->code = parse_error_syntax;  // hackish

    va_list va;
    va_start(va, fmt);
    error->text = vformat_string(fmt, va);
    va_end(va);

    this->report_errors(error_list);
    return parse_execution_errored;
}

parse_execution_result_t parse_execution_context_t::report_errors(
    const parse_error_list_t &error_list) const {
    if (!parser->cancellation_requested) {
        if (error_list.empty()) {
            debug(0, "Error reported but no error text found.");
        }

        // Get a backtrace.
        wcstring backtrace_and_desc;
        parser->get_backtrace(pstree->src, error_list, backtrace_and_desc);

        // Print it.
        if (!should_suppress_stderr_for_tests()) {
            std::fwprintf(stderr, L"%ls", backtrace_and_desc.c_str());
        }
    }
    return parse_execution_errored;
}

/// Reports an unmatched wildcard error and returns parse_execution_errored.
parse_execution_result_t parse_execution_context_t::report_unmatched_wildcard_error(
    const parse_node_t &unmatched_wildcard) const {
    proc_set_last_statuses(statuses_t::just(STATUS_UNMATCHED_WILDCARD));
    report_error(unmatched_wildcard, WILDCARD_ERR_MSG, get_source(unmatched_wildcard).c_str());
    return parse_execution_errored;
}

// Given a command string that might contain fish special tokens return a string without those
// tokens.
//
// TODO(krader1961): Figure out what VARIABLE_EXPAND means in this context. After looking at the
// code and doing various tests I couldn't figure out why that token would be present when this
// code is run. I was therefore unable to determine how to substitute its presence in the error
// message.
static wcstring reconstruct_orig_str(wcstring tokenized_str) {
    wcstring orig_str = tokenized_str;

    if (tokenized_str.find(VARIABLE_EXPAND_SINGLE) != std::string::npos) {
        // Variable was quoted to force expansion of multiple elements into a single element.
        //
        // The following isn't entirely correct. For example, $abc"$def" will become "$abc$def".
        // However, anyone writing the former is asking for trouble so I don't feel bad about not
        // accurately reconstructing what they typed.
        wcstring new_str = wcstring(tokenized_str);
        std::replace(new_str.begin(), new_str.end(), (wchar_t)VARIABLE_EXPAND_SINGLE, L'$');
        orig_str = L"\"" + new_str + L"\"";
    }

    return orig_str;
}

/// Handle the case of command not found.
parse_execution_result_t parse_execution_context_t::handle_command_not_found(
    const wcstring &cmd_str, tnode_t<g::plain_statement> statement, int err_code) {
    // We couldn't find the specified command. This is a non-fatal error. We want to set the exit
    // status to 127, which is the standard number used by other shells like bash and zsh.

    const wchar_t *const cmd = cmd_str.c_str();
    const wchar_t *const equals_ptr = std::wcschr(cmd, L'=');
    if (equals_ptr != NULL) {
        // Try to figure out if this is a pure variable assignment (foo=bar), or if this appears to
        // be running a command (foo=bar ruby...).
        const wcstring name_str = wcstring(cmd, equals_ptr - cmd);  // variable name, up to the =
        const wcstring val_str = wcstring(equals_ptr + 1);          // variable value, past the =

        auto args = statement.descendants<g::argument>(1);
        if (!args.empty()) {
            const wcstring argument = get_source(args.at(0));

            // Looks like a command.
            this->report_error(statement, ERROR_BAD_EQUALS_IN_COMMAND5, argument.c_str(),
                               name_str.c_str(), val_str.c_str(), argument.c_str(),
                               ellipsis_str);
        } else {
            wcstring assigned_val = reconstruct_orig_str(val_str);
            this->report_error(statement, ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, name_str.c_str(),
                               assigned_val.c_str());
        }
    } else if (err_code != ENOENT) {
        this->report_error(statement, _(L"The file '%ls' is not executable by this user"), cmd);
    } else {
        // Handle unrecognized commands with standard command not found handler that can make better
        // error messages.
        wcstring_list_t event_args;
        {
            auto args = get_argument_nodes(statement.child<1>());
            parse_execution_result_t arg_result =
                this->expand_arguments_from_nodes(args, &event_args, failglob);

            if (arg_result != parse_execution_success) {
                return arg_result;
            }

            event_args.insert(event_args.begin(), cmd_str);
        }

        event_fire_generic(L"fish_command_not_found", &event_args);

        // Here we want to report an error (so it shows a backtrace), but with no text.
        this->report_error(statement, L"");
    }

    // Set the last proc status appropriately.
    int status = err_code == ENOENT ? STATUS_CMD_UNKNOWN : STATUS_NOT_EXECUTABLE;
    proc_set_last_statuses(statuses_t::just(status));

    return parse_execution_errored;
}

parse_execution_result_t parse_execution_context_t::expand_command(
    tnode_t<grammar::plain_statement> statement, wcstring *out_cmd,
    wcstring_list_t *out_args) const {
    // Here we're expanding a command, for example $HOME/bin/stuff or $randomthing. The first
    // completion becomes the command itself, everything after becomes arguments. Command
    // substitutions are not supported.
    parse_error_list_t errors;

    // Get the unexpanded command string. We expect to always get it here.
    wcstring unexp_cmd = *command_for_plain_statement(statement, pstree->src);

    // Expand the string to produce completions, and report errors.
    expand_error_t expand_err =
        expand_to_command_and_args(unexp_cmd, parser->vars(), out_cmd, out_args, &errors);
    if (expand_err == EXPAND_ERROR) {
        proc_set_last_statuses(statuses_t::just(STATUS_ILLEGAL_CMD));
        return report_errors(errors);
    } else if (expand_err == EXPAND_WILDCARD_NO_MATCH) {
        return report_unmatched_wildcard_error(statement);
    }
    assert(expand_err == EXPAND_OK || expand_err == EXPAND_WILDCARD_MATCH);

    // Complain if the resulting expansion was empty, or expanded to an empty string.
    if (out_cmd->empty()) {
        return this->report_error(statement, _(L"The expanded command was empty."));
    }
    return parse_execution_success;
}

/// Creates a 'normal' (non-block) process.
parse_execution_result_t parse_execution_context_t::populate_plain_process(
    job_t *job, process_t *proc, tnode_t<grammar::plain_statement> statement) {
    assert(job != NULL);
    assert(proc != NULL);

    // We may decide that a command should be an implicit cd.
    bool use_implicit_cd = false;

    // Get the command and any arguments due to expanding the command.
    wcstring cmd;
    wcstring_list_t args_from_cmd_expansion;
    auto ret = expand_command(statement, &cmd, &args_from_cmd_expansion);
    if (ret != parse_execution_success) {
        return ret;
    }
    assert(!cmd.empty() && "expand_command should not produce an empty command");

    // Determine the process type.
    enum process_type_t process_type = process_type_for_command(statement, cmd);

    // Check for stack overflow.
    if (process_type == process_type_t::function &&
        parser->forbidden_function.size() > FISH_MAX_STACK_DEPTH) {
        this->report_error(statement, CALL_STACK_LIMIT_EXCEEDED_ERR_MSG);
        return parse_execution_errored;
    }

    // Protect against exec with background processes running
    static uint32_t last_exec_run_counter =  -1;
    if (process_type == process_type_t::exec && shell_is_interactive()) {
        bool have_bg = false;
        for (const auto &bg : jobs()) {
            // The assumption here is that if it is a foreground job,
            // it's related to us.
            // This stops us from asking if we're doing `exec` inside a function.
            if (!bg->is_completed() && !bg->is_foreground()) {
                have_bg = true;
                break;
            }
        }

        if (have_bg) {
            /* debug(1, "Background jobs remain! run_counter: %u, last_exec_run_count: %u", reader_run_count(), last_exec_run_counter); */
            if (isatty(STDIN_FILENO) && reader_run_count() - 1 != last_exec_run_counter) {
                reader_bg_job_warning();
                last_exec_run_counter = reader_run_count();
                return parse_execution_errored;
            }
            else {
                hup_background_jobs();
            }
        }
    }

    wcstring path_to_external_command;
    if (process_type == process_type_t::external || process_type == process_type_t::exec) {
        // Determine the actual command. This may be an implicit cd.
        bool has_command = path_get_path(cmd, &path_to_external_command, parser->vars());

        // If there was no command, then we care about the value of errno after checking for it, to
        // distinguish between e.g. no file vs permissions problem.
        const int no_cmd_err_code = errno;

        // If the specified command does not exist, and is undecorated, try using an implicit cd.
        if (!has_command && get_decoration(statement) == parse_statement_decoration_none) {
            // Implicit cd requires an empty argument and redirection list.
            tnode_t<g::arguments_or_redirections_list> args = statement.child<1>();
            if (args_from_cmd_expansion.empty() && !args.try_get_child<g::argument, 0>() &&
                !args.try_get_child<g::redirection, 0>()) {
                // Ok, no arguments or redirections; check to see if the command is a directory.
                use_implicit_cd =
                    path_as_implicit_cd(cmd, parser->vars().get_pwd_slash(), parser->vars())
                        .has_value();
            }
        }

        if (!has_command && !use_implicit_cd) {
            // No command.
            return this->handle_command_not_found(cmd, statement, no_cmd_err_code);
        }
    }

    // Produce the full argument list and the set of IO redirections.
    wcstring_list_t cmd_args;
    io_chain_t process_io_chain;
    if (use_implicit_cd) {
        // Implicit cd is simple.
        cmd_args = {L"cd", cmd};
        path_to_external_command.clear();

        // If we have defined a wrapper around cd, use it, otherwise use the cd builtin.
        process_type = function_exists(L"cd") ? process_type_t::function : process_type_t::builtin;
    } else {
        // Not implicit cd.
        const globspec_t glob_behavior = (cmd == L"set" || cmd == L"count") ? nullglob : failglob;
        // Form the list of arguments. The command is the first argument, followed by any arguments
        // from expanding the command, followed by the argument nodes themselves. E.g. if the
        // command is '$gco foo' and $gco is git checkout.
        cmd_args.push_back(cmd);
        cmd_args.insert(cmd_args.end(), args_from_cmd_expansion.begin(),
                        args_from_cmd_expansion.end());
        argument_node_list_t arg_nodes = statement.descendants<g::argument>();
        parse_execution_result_t arg_result =
            this->expand_arguments_from_nodes(arg_nodes, &cmd_args, glob_behavior);
        if (arg_result != parse_execution_success) {
            return arg_result;
        }

        // The set of IO redirections that we construct for the process.
        if (!this->determine_io_chain(statement.child<1>(), &process_io_chain)) {
            return parse_execution_errored;
        }

        // Determine the process type.
        process_type = process_type_for_command(statement, cmd);
    }

    // Populate the process.
    proc->type = process_type;
    proc->set_argv(cmd_args);
    proc->set_io_chain(process_io_chain);
    proc->actual_cmd = std::move(path_to_external_command);
    return parse_execution_success;
}

// Determine the list of arguments, expanding stuff. Reports any errors caused by expansion. If we
// have a wildcard that could not be expanded, report the error and continue.
parse_execution_result_t parse_execution_context_t::expand_arguments_from_nodes(
    const argument_node_list_t &argument_nodes, wcstring_list_t *out_arguments,
    globspec_t glob_behavior) {
    // Get all argument nodes underneath the statement. We guess we'll have that many arguments (but
    // may have more or fewer, if there are wildcards involved).
    out_arguments->reserve(out_arguments->size() + argument_nodes.size());
    std::vector<completion_t> arg_expanded;
    for (const auto arg_node : argument_nodes) {
        // Expect all arguments to have source.
        assert(arg_node.has_source());
        const wcstring arg_str = arg_node.get_source(pstree->src);

        // Expand this string.
        parse_error_list_t errors;
        arg_expanded.clear();
        int expand_ret =
            expand_string(arg_str, &arg_expanded, EXPAND_NO_DESCRIPTIONS, parser->vars(), &errors);
        parse_error_offset_source_start(&errors, arg_node.source_range()->start);
        switch (expand_ret) {
            case EXPAND_ERROR: {
                this->report_errors(errors);
                return parse_execution_errored;
            }
            case EXPAND_WILDCARD_NO_MATCH: {
                if (glob_behavior == failglob) {
                    // Report the unmatched wildcard error and stop processing.
                    report_unmatched_wildcard_error(arg_node);
                    return parse_execution_errored;
                }
                break;
            }
            case EXPAND_WILDCARD_MATCH:
            case EXPAND_OK: {
                break;
            }
            default: {
                DIE("unexpected expand_string() return value");
                break;
            }
        }

        // Now copy over any expanded arguments. Use std::move() to avoid extra allocations; this
        // is called very frequently.
        out_arguments->reserve(out_arguments->size() + arg_expanded.size());
        for (completion_t &new_arg : arg_expanded) {
            out_arguments->push_back(std::move(new_arg.completion));
        }
    }

    return parse_execution_success;
}

bool parse_execution_context_t::determine_io_chain(tnode_t<g::arguments_or_redirections_list> node,
                                                   io_chain_t *out_chain) {
    io_chain_t result;
    bool errored = false;

    // Get all redirection nodes underneath the statement.
    while (auto redirect_node = node.next_in_list<g::redirection>()) {
        int source_fd = -1;  // source fd
        wcstring target;     // file path or target fd
        auto redirect_type = redirection_type(redirect_node, pstree->src, &source_fd, &target);

        // PCA: I can't justify this EXPAND_SKIP_VARIABLES flag. It was like this when I got here.
        bool target_expanded =
            expand_one(target, no_exec ? EXPAND_SKIP_VARIABLES : 0, parser->vars());
        if (!target_expanded || target.empty()) {
            // TODO: Improve this error message.
            errored =
                report_error(redirect_node, _(L"Invalid redirection target: %ls"), target.c_str());
        }

        // Generate the actual IO redirection.
        shared_ptr<io_data_t> new_io;
        assert(redirect_type && "expected to have a valid redirection");
        switch (*redirect_type) {
            case redirection_type_t::fd: {
                if (target == L"-") {
                    new_io.reset(new io_close_t(source_fd));
                } else {
                    int old_fd = fish_wcstoi(target.c_str());
                    if (errno || old_fd < 0) {
                        const wchar_t *fmt =
                            _(L"Requested redirection to '%ls', "
                              L"which is not a valid file descriptor");
                        errored = report_error(redirect_node, fmt, target.c_str());
                    } else {
                        new_io.reset(new io_fd_t(source_fd, old_fd, true));
                    }
                }
                break;
            }
            default: {
                int oflags = oflags_for_redirection_type(*redirect_type);
                io_file_t *new_io_file = new io_file_t(source_fd, target, oflags);
                new_io.reset(new_io_file);
                break;
            }
        }

        // Append the new_io if we got one.
        if (new_io.get() != NULL) {
            result.push_back(new_io);
        }
    }

    if (out_chain && !errored) {
        *out_chain = std::move(result);
    }
    return !errored;
}

parse_execution_result_t parse_execution_context_t::populate_not_process(
    job_t *job, process_t *proc, tnode_t<g::not_statement> not_statement) {
    job->set_flag(job_flag_t::NEGATE, !job->get_flag(job_flag_t::NEGATE));
    return this->populate_job_process(job, proc,
                                      not_statement.require_get_child<g::statement, 1>());
}

template <typename Type>
parse_execution_result_t parse_execution_context_t::populate_block_process(
    job_t *job, process_t *proc, tnode_t<g::statement> statement,
    tnode_t<Type> specific_statement) {
    // We handle block statements by creating process_type_t::block_node, that will bounce back to
    // us when it's time to execute them.
    UNUSED(job);
    static_assert(Type::token == symbol_block_statement || Type::token == symbol_if_statement ||
                      Type::token == symbol_switch_statement,
                  "Invalid block process");
    assert(statement && "statement missing");
    assert(specific_statement && "specific_statement missing");

    // The set of IO redirections that we construct for the process.
    // TODO: fix this ugly find_child.
    auto arguments = specific_statement.template find_child<g::arguments_or_redirections_list>();
    io_chain_t process_io_chain;
    bool errored = !this->determine_io_chain(arguments, &process_io_chain);
    if (errored) return parse_execution_errored;

    proc->type = process_type_t::block_node;
    proc->block_node_source = pstree;
    proc->internal_block_node = statement;
    proc->set_io_chain(process_io_chain);
    return parse_execution_success;
}

parse_execution_result_t parse_execution_context_t::populate_job_process(
    job_t *job, process_t *proc, tnode_t<grammar::statement> statement) {
    // Get the "specific statement" which is boolean / block / if / switch / decorated.
    const parse_node_t &specific_statement = statement.get_child_node<0>();

    parse_execution_result_t result = parse_execution_success;

    switch (specific_statement.type) {
        case symbol_not_statement: {
            result = this->populate_not_process(job, proc, {&tree(), &specific_statement});
            break;
        }
        case symbol_block_statement:
            result = this->populate_block_process(
                job, proc, statement, tnode_t<g::block_statement>(&tree(), &specific_statement));
            break;
        case symbol_if_statement:
            result = this->populate_block_process(
                job, proc, statement, tnode_t<g::if_statement>(&tree(), &specific_statement));
            break;
        case symbol_switch_statement:
            result = this->populate_block_process(
                job, proc, statement, tnode_t<g::switch_statement>(&tree(), &specific_statement));
            break;
        case symbol_decorated_statement: {
            // Get the plain statement. It will pull out the decoration itself.
            tnode_t<g::decorated_statement> dec_stat{&tree(), &specific_statement};
            auto plain_statement = dec_stat.find_child<g::plain_statement>();
            result = this->populate_plain_process(job, proc, plain_statement);
            break;
        }
        default: {
            debug(0, L"'%ls' not handled by new parser yet.",
                  specific_statement.describe().c_str());
            PARSER_DIE();
            break;
        }
    }

    return result;
}

parse_execution_result_t parse_execution_context_t::populate_job_from_job_node(
    job_t *j, tnode_t<grammar::job> job_node, const block_t *associated_block) {
    UNUSED(associated_block);

    // Tell the job what its command is.
    j->set_command(get_source(job_node));

    // We are going to construct process_t structures for every statement in the job. Get the first
    // statement.
    tnode_t<g::statement> statement = job_node.child<0>();
    assert(statement);

    parse_execution_result_t result = parse_execution_success;

    // Create processes. Each one may fail.
    process_list_t processes;
    processes.emplace_back(new process_t());
    result = this->populate_job_process(j, processes.back().get(), statement);

    // Construct process_ts for job continuations (pipelines), by walking the list until we hit the
    // terminal (empty) job continuation.
    tnode_t<g::job_continuation> job_cont = job_node.child<1>();
    assert(job_cont);
    while (auto pipe = job_cont.try_get_child<g::tok_pipe, 0>()) {
        if (result != parse_execution_success) {
            break;
        }
        tnode_t<g::statement> statement = job_cont.require_get_child<g::statement, 2>();

        // Handle the pipe, whose fd may not be the obvious stdout.
        int pipe_write_fd = fd_redirected_by_pipe(get_source(pipe));
        if (pipe_write_fd == -1) {
            result = report_error(pipe, ILLEGAL_FD_ERR_MSG, get_source(pipe).c_str());
            break;
        }
        processes.back()->pipe_write_fd = pipe_write_fd;

        // Store the new process (and maybe with an error).
        processes.emplace_back(new process_t());
        result = this->populate_job_process(j, processes.back().get(), statement);

        // Get the next continuation.
        job_cont = job_cont.require_get_child<g::job_continuation, 3>();
        assert(job_cont);
    }

    // Inform our processes of who is first and last
    processes.front()->is_first_in_job = true;
    processes.back()->is_last_in_job = true;

    // Return what happened.
    if (result == parse_execution_success) {
        // Link up the processes.
        assert(!processes.empty());  //!OCLINT(multiple unary operator)
        j->processes = std::move(processes);
    }
    return result;
}

static bool remove_job(job_t *job) {
    for (auto j = jobs().begin(); j != jobs().end(); ++j) {
        if (j->get() == job) {
            jobs().erase(j);
            return true;
        }
    }
    return false;
}

parse_execution_result_t parse_execution_context_t::run_1_job(tnode_t<g::job> job_node,
                                                              const block_t *associated_block) {
    if (should_cancel_execution(associated_block)) {
        return parse_execution_cancelled;
    }

    // Get terminal modes.
    struct termios tmodes = {};
    if (shell_is_interactive() && tcgetattr(STDIN_FILENO, &tmodes)) {
        // Need real error handling here.
        wperror(L"tcgetattr");
        return parse_execution_errored;
    }

    // Increment the eval_level for the duration of this command.
    scoped_push<int> saved_eval_level(&parser->eval_level, parser->eval_level + 1);

    // Save the node index.
    scoped_push<tnode_t<grammar::job>> saved_node(&executing_job_node, job_node);

    // Profiling support.
    long long start_time = 0, parse_time = 0, exec_time = 0;
    profile_item_t *profile_item = this->parser->create_profile_item();
    if (profile_item != NULL) {
        start_time = get_time();
    }

    // When we encounter a block construct (e.g. while loop) in the general case, we create a "block
    // process" containing its node. This allows us to handle block-level redirections.
    // However, if there are no redirections, then we can just jump into the block directly, which
    // is significantly faster.
    if (job_is_simple_block(job_node)) {
        parse_execution_result_t result = parse_execution_success;

        tnode_t<g::statement> statement = job_node.child<0>();
        const parse_node_t &specific_statement = statement.get_child_node<0>();
        assert(specific_statement_type_is_redirectable_block(specific_statement));
        switch (specific_statement.type) {
            case symbol_block_statement: {
                result =
                    this->run_block_statement({&tree(), &specific_statement}, associated_block);
                break;
            }
            case symbol_if_statement: {
                result = this->run_if_statement({&tree(), &specific_statement}, associated_block);
                break;
            }
            case symbol_switch_statement: {
                result = this->run_switch_statement({&tree(), &specific_statement});
                break;
            }
            default: {
                // Other types should be impossible due to the
                // specific_statement_type_is_redirectable_block check.
                PARSER_DIE();
                break;
            }
        }

        if (profile_item != NULL) {
            // Block-types profile a little weird. They have no 'parse' time, and their command is
            // just the block type.
            exec_time = get_time();
            profile_item->level = parser->eval_level;
            profile_item->parse = 0;
            profile_item->exec = (int)(exec_time - start_time);
            profile_item->cmd = profiling_cmd_name_for_redirectable_block(
                specific_statement, this->tree(), this->pstree->src);
            profile_item->skipped = false;
        }

        return result;
    }

    shared_ptr<job_t> job = std::make_shared<job_t>(acquire_job_id(), block_io, parent_job);
    job->tmodes = tmodes;
    job->set_flag(job_flag_t::JOB_CONTROL,
                  (job_control_mode == job_control_t::all) ||
                      ((job_control_mode == job_control_t::interactive) && shell_is_interactive()));

    job->set_flag(job_flag_t::FOREGROUND, !job_node_is_background(job_node));

    job->set_flag(job_flag_t::TERMINAL, job->get_flag(job_flag_t::JOB_CONTROL) && !is_event);

    job->set_flag(job_flag_t::SKIP_NOTIFICATION,
                  is_subshell || is_block || is_event || !shell_is_interactive());

    // Tell the current block what its job is. This has to happen before we populate it (#1394).
    parser->current_block()->job = job;

    // Populate the job. This may fail for reasons like command_not_found. If this fails, an error
    // will have been printed.
    parse_execution_result_t pop_result =
        this->populate_job_from_job_node(job.get(), job_node, associated_block);

    // Clean up the job on failure or cancellation.
    bool populated_job = (pop_result == parse_execution_success);
    if (!populated_job || this->should_cancel_execution(associated_block)) {
        assert(parser->current_block()->job == job);
        parser->current_block()->job = NULL;
        populated_job = false;
    }

    // Store time it took to 'parse' the command.
    if (profile_item != NULL) {
        parse_time = get_time();
    }

    if (populated_job) {
        // Success. Give the job to the parser - it will clean it up.
        parser->job_add(job);

        // Check to see if this contained any external commands.
        bool job_contained_external_command = false;
        for (const auto &proc : job->processes) {
            if (proc->type == process_type_t::external) {
                job_contained_external_command = true;
                break;
            }
        }

        // Actually execute the job.
        if (!exec_job(*this->parser, job)) {
            remove_job(job.get());
        }

        // Only external commands require a new fishd barrier.
        if (job_contained_external_command) {
            set_proc_had_barrier(false);
        }
    }

    if (profile_item != NULL) {
        exec_time = get_time();
        profile_item->level = parser->eval_level;
        profile_item->parse = (int)(parse_time - start_time);
        profile_item->exec = (int)(exec_time - parse_time);
        profile_item->cmd = job ? job->command() : wcstring();
        profile_item->skipped = !populated_job;
    }

    job_reap(false);  // clean up jobs
    return parse_execution_success;
}

parse_execution_result_t parse_execution_context_t::run_job_conjunction(
    tnode_t<grammar::job_conjunction> job_expr, const block_t *associated_block) {
    parse_execution_result_t result = parse_execution_success;
    tnode_t<g::job_conjunction> cursor = job_expr;
    // continuation is the parent of the cursor
    tnode_t<g::job_conjunction_continuation> continuation;
    while (cursor) {
        if (should_cancel_execution(associated_block)) break;
        bool skip = false;
        if (continuation) {
            // Check the conjunction type.
            parse_bool_statement_type_t conj = bool_statement_type(continuation);
            assert((conj == parse_bool_and || conj == parse_bool_or) && "Unexpected conjunction");
            skip = should_skip(conj);
        }
        if (! skip) {
            result = run_1_job(cursor.child<0>(), associated_block);
        }
        continuation = cursor.child<1>();
        cursor = continuation.try_get_child<g::job_conjunction, 2>();
    }
    return result;
}

bool parse_execution_context_t::should_skip(parse_bool_statement_type_t type) const {
    switch (type) {
        case parse_bool_and:
            // AND. Skip if the last job failed.
            return proc_get_last_status() != 0;
        case parse_bool_or:
            // OR. Skip if the last job succeeded.
            return proc_get_last_status() == 0;
        default:
            return false;
    }
}

template <typename Type>
parse_execution_result_t parse_execution_context_t::run_job_list(tnode_t<Type> job_list,
                                                                 const block_t *associated_block) {
    // We handle both job_list and andor_job_list uniformly.
    static_assert(Type::token == symbol_job_list || Type::token == symbol_andor_job_list,
                  "Not a job list");

    parse_execution_result_t result = parse_execution_success;
    while (auto job_conj = job_list.template next_in_list<g::job_conjunction>()) {
        if (should_cancel_execution(associated_block)) break;

        // Maybe skip the job if it has a leading and/or.
        // Skipping is treated as success.
        if (should_skip(get_decorator(job_conj))) {
            result = parse_execution_success;
        } else {
            result = this->run_job_conjunction(job_conj, associated_block);
        }
    }

    // Returns the result of the last job executed or skipped.
    return result;
}

parse_execution_result_t parse_execution_context_t::eval_node(tnode_t<g::statement> statement,
                                                              const block_t *associated_block,
                                                              const io_chain_t &io) {
    assert(statement && "Empty node in eval_node");
    assert(statement.matches_node_tree(tree()) && "statement has unexpected tree");
    // Apply this block IO for the duration of this function.
    scoped_push<io_chain_t> block_io_push(&block_io, io);
    enum parse_execution_result_t status = parse_execution_success;
    if (auto block = statement.try_get_child<g::block_statement, 0>()) {
        status = this->run_block_statement(block, associated_block);
    } else if (auto ifstat = statement.try_get_child<g::if_statement, 0>()) {
        status = this->run_if_statement(ifstat, associated_block);
    } else if (auto switchstat = statement.try_get_child<g::switch_statement, 0>()) {
        status = this->run_switch_statement(switchstat);
    } else {
        debug(0, "Unexpected node %ls found in %s", statement.node()->describe().c_str(),
              __FUNCTION__);
        abort();
    }
    return status;
}

parse_execution_result_t parse_execution_context_t::eval_node(tnode_t<g::job_list> job_list,
                                                              const block_t *associated_block,
                                                              const io_chain_t &io) {
    // Apply this block IO for the duration of this function.
    assert(job_list && "Empty node in eval_node");
    assert(job_list.matches_node_tree(tree()) && "job_list has unexpected tree");
    scoped_push<io_chain_t> block_io_push(&block_io, io);
    enum parse_execution_result_t status = parse_execution_success;
    wcstring func_name;
    auto infinite_recursive_node =
        this->infinite_recursive_statement_in_job_list(job_list, &func_name);
    if (infinite_recursive_node) {
        // We have an infinite recursion.
        this->report_error(infinite_recursive_node, INFINITE_FUNC_RECURSION_ERR_MSG,
                           func_name.c_str());
        status = parse_execution_errored;
    } else {
        // No infinite recursion.
        status = this->run_job_list(job_list, associated_block);
    }
    return status;
}

int parse_execution_context_t::line_offset_of_node(tnode_t<g::job> node) {
    // If we're not executing anything, return -1.
    if (!node) {
        return -1;
    }

    // If for some reason we're executing a node without source, return -1.
    auto range = node.source_range();
    if (!range) {
        return -1;
    }

    return this->line_offset_of_character_at_offset(range->start);
}

int parse_execution_context_t::line_offset_of_character_at_offset(size_t offset) {
    // Count the number of newlines, leveraging our cache.
    assert(offset <= pstree->src.size());

    // Easy hack to handle 0.
    if (offset == 0) {
        return 0;
    }

    // We want to return (one plus) the number of newlines at offsets less than the given offset.
    // cached_lineno_count is the number of newlines at indexes less than cached_lineno_offset.
    const wchar_t *str = pstree->src.c_str();
    if (offset > cached_lineno_offset) {
        size_t i;
        for (i = cached_lineno_offset; i < offset && str[i] != L'\0'; i++) {
            // Add one for every newline we find in the range [cached_lineno_offset, offset).
            if (str[i] == L'\n') {
                cached_lineno_count++;
            }
        }
        cached_lineno_offset =
            i;  // note: i, not offset, in case offset is beyond the length of the string
    } else if (offset < cached_lineno_offset) {
        // Subtract one for every newline we find in the range [offset, cached_lineno_offset).
        for (size_t i = offset; i < cached_lineno_offset; i++) {
            if (str[i] == L'\n') {
                cached_lineno_count--;
            }
        }
        cached_lineno_offset = offset;
    }
    return cached_lineno_count;
}

int parse_execution_context_t::get_current_line_number() {
    int line_number = -1;
    int line_offset = this->line_offset_of_node(this->executing_job_node);
    if (line_offset >= 0) {
        // The offset is 0 based; the number is 1 based.
        line_number = line_offset + 1;
    }
    return line_number;
}

int parse_execution_context_t::get_current_source_offset() const {
    int result = -1;
    if (executing_job_node) {
        if (auto range = executing_job_node.source_range()) {
            result = static_cast<int>(range->start);
        }
    }
    return result;
}
