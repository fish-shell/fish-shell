// Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.)
//
// A note on error handling: fish has two kind of errors, fatal parse errors non-fatal runtime
// errors. A fatal error prevents execution of the entire file, while a non-fatal error skips that
// job.
//
// Non-fatal errors are printed as soon as they are encountered; otherwise you would have to wait
// for the execution to finish to see them.
#include "config.h"  // IWYU pragma: keep

#include "parse_execution.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <wctype.h>

#include <algorithm>
#include <cwchar>
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
#include "flog.h"
#include "function.h"
#include "io.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "timer.h"
#include "tnode.h"
#include "tokenizer.h"
#include "trace.h"
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

/// Get a redirection from stderr to stdout (i.e. 2>&1).
static redirection_spec_t get_stderr_merge() {
    const wchar_t *stdout_fileno_str = L"1";
    return redirection_spec_t{STDERR_FILENO, redirection_mode_t::fd, stdout_fileno_str};
}

parse_execution_context_t::parse_execution_context_t(parsed_source_ref_t pstree, parser_t *p,
                                                     const operation_context_t &ctx,
                                                     job_lineage_t lineage)
    : pstree(std::move(pstree)), parser(p), ctx(ctx), lineage(std::move(lineage)) {}

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
        (current && parent && current->type() == block_type_t::top && parent->is_function_call());
    if (!is_within_function_call) {
        return {};
    }

    // Get the function name of the immediate block.
    const wcstring &forbidden_function_name = parent->function_name;

    // Get the first job in the job list.
    tnode_t<g::job> first_job = job_list.try_get_child<g::job_conjunction, 1>().child<0>();
    if (!first_job) {
        return {};
    }

    // Here's the statement node we find that's infinite recursive.
    tnode_t<grammar::plain_statement> infinite_recursive_statement;

    // Ignore the jobs variable assigment and "time" prefixes.
    tnode_t<g::statement> statement = first_job.child<2>();
    tnode_t<g::job_continuation> continuation = first_job.child<3>();
    const null_environment_t nullenv{};
    while (statement) {
        // Get the list of plain statements.
        // Ignore statements with decorations like 'builtin' or 'command', since those
        // are not infinite recursion. In particular that is what enables 'wrapper functions'.
        tnode_t<g::plain_statement> plain_statement =
            statement.try_get_child<g::decorated_statement, 0>()
                .try_get_child<g::plain_statement, 0>();
        if (plain_statement) {
            maybe_t<wcstring> cmd = command_for_plain_statement(plain_statement, pstree->src);
            if (cmd &&
                expand_one(*cmd, {expand_flag::skip_cmdsubst, expand_flag::skip_variables}, ctx) &&
                cmd == forbidden_function_name) {
                // This is it.
                infinite_recursive_statement = plain_statement;
                if (out_func_name != nullptr) {
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
        case parse_statement_decoration_none:
            if (function_exists(cmd, *parser)) {
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

maybe_t<end_execution_reason_t> parse_execution_context_t::check_end_execution() const {
    if (shell_is_exiting()) {
        return end_execution_reason_t::cancelled;
    }
    if (nullptr == parser) {
        return none();
    }
    if (parser->cancellation_signal) {
        return end_execution_reason_t::cancelled;
    }
    const auto &ld = parser->libdata();
    if (ld.returning) {
        return end_execution_reason_t::control_flow;
    }
    if (ld.loop_status != loop_status_t::normals) {
        return end_execution_reason_t::control_flow;
    }
    return none();
}

/// Return whether the job contains a single statement, of block type, with no redirections.
bool parse_execution_context_t::job_is_simple_block(tnode_t<g::job> job_node) const {
    tnode_t<g::statement> statement = job_node.child<2>();

    // Must be no pipes.
    if (job_node.child<3>().try_get_child<g::tok_pipe, 0>()) {
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

end_execution_reason_t parse_execution_context_t::run_if_statement(
    tnode_t<g::if_statement> statement, const block_t *associated_block) {
    end_execution_reason_t result = end_execution_reason_t::ok;

    // We have a sequence of if clauses, with a final else, resulting in a single job list that we
    // execute.
    tnode_t<g::job_list> job_list_to_execute;
    tnode_t<g::if_clause> if_clause = statement.child<0>();
    tnode_t<g::else_clause> else_clause = statement.child<1>();

    // We start with the 'if'.
    trace_if_enabled(*parser, L"if");

    for (;;) {
        if (auto ret = check_end_execution()) {
            result = *ret;
            break;
        }

        // An if condition has a job and a "tail" of andor jobs, e.g. "foo ; and bar; or baz".
        tnode_t<g::job_conjunction> condition_head = if_clause.child<1>();
        tnode_t<g::andor_job_list> condition_boolean_tail = if_clause.child<3>();

        // Check the condition and the tail. We treat end_execution_reason_t::error here as failure,
        // in accordance with historic behavior.
        end_execution_reason_t cond_ret = run_job_conjunction(condition_head, associated_block);
        if (cond_ret == end_execution_reason_t::ok) {
            cond_ret = run_job_list(condition_boolean_tail, associated_block);
        }
        const bool take_branch =
            (cond_ret == end_execution_reason_t::ok) && parser->get_last_status() == EXIT_SUCCESS;

        if (take_branch) {
            // Condition succeeded.
            job_list_to_execute = if_clause.child<4>();
            break;
        }
        auto else_cont = else_clause.try_get_child<g::else_continuation, 1>();
        if (!else_cont) {
            // 'if' condition failed, no else clause, return 0, we're done.
            parser->set_last_statuses(statuses_t::just(STATUS_CMD_OK));
            break;
        } else {
            // We have an 'else continuation' (either else-if or else).
            if (auto maybe_if_clause = else_cont.try_get_child<g::if_clause, 0>()) {
                // it's an 'else if', go to the next one.
                if_clause = maybe_if_clause;
                else_clause = else_cont.try_get_child<g::else_clause, 1>();
                assert(else_clause && "Expected to have an else clause");
                trace_if_enabled(*parser, L"else if");
            } else {
                // It's the final 'else', we're done.
                job_list_to_execute = else_cont.try_get_child<g::job_list, 1>();
                assert(job_list_to_execute && "Should have a job list");
                trace_if_enabled(*parser, L"else");
                break;
            }
        }
    }

    // Execute any job list we got.
    if (job_list_to_execute) {
        block_t *ib = parser->push_block(block_t::if_block());
        run_job_list(job_list_to_execute, ib);
        if (auto ret = check_end_execution()) {
            result = *ret;
        }
        parser->pop_block(ib);
    } else {
        // No job list means no successful conditions, so return 0 (issue #1443).
        parser->set_last_statuses(statuses_t::just(STATUS_CMD_OK));
    }

    trace_if_enabled(*parser, L"end if");

    // It's possible there's a last-minute cancellation (issue #1297).
    if (auto ret = check_end_execution()) {
        result = *ret;
    }

    // Otherwise, take the exit status of the job list. Reversal of issue #1061.
    return result;
}

end_execution_reason_t parse_execution_context_t::run_begin_statement(
    tnode_t<g::job_list> contents) {
    // Basic begin/end block. Push a scope block, run jobs, pop it
    trace_if_enabled(*parser, L"begin");
    block_t *sb = parser->push_block(block_t::scope_block(block_type_t::begin));
    end_execution_reason_t ret = run_job_list(contents, sb);
    parser->pop_block(sb);
    trace_if_enabled(*parser, L"end begin");
    return ret;
}

// Define a function.
end_execution_reason_t parse_execution_context_t::run_function_statement(
    tnode_t<grammar::block_statement> statement, tnode_t<grammar::function_header> header) {
    // Get arguments.
    wcstring_list_t arguments;
    argument_node_list_t arg_nodes = header.descendants<g::argument>();
    end_execution_reason_t result =
        this->expand_arguments_from_nodes(arg_nodes, &arguments, failglob);

    if (result != end_execution_reason_t::ok) {
        return result;
    }
    trace_if_enabled(*parser, L"function", arguments);
    io_streams_t streams(0);  // no limit on the amount of output from builtin_function()
    int err = builtin_function(*parser, streams, arguments, pstree, statement);
    parser->set_last_statuses(statuses_t::just(err));

    wcstring errtext = streams.err.contents();
    if (!errtext.empty()) {
        return this->report_error(err, header, L"%ls", errtext.c_str());
    }
    return result;
}

end_execution_reason_t parse_execution_context_t::run_block_statement(
    tnode_t<g::block_statement> statement, const block_t *associated_block) {
    tnode_t<g::block_header> bheader = statement.child<0>();
    tnode_t<g::job_list> contents = statement.child<1>();

    end_execution_reason_t ret = end_execution_reason_t::ok;
    if (auto header = bheader.try_get_child<g::for_header, 0>()) {
        ret = run_for_statement(header, contents);
    } else if (auto header = bheader.try_get_child<g::while_header, 0>()) {
        ret = run_while_statement(header, contents, associated_block);
    } else if (auto header = bheader.try_get_child<g::function_header, 0>()) {
        ret = run_function_statement(statement, header);
    } else if (auto header = bheader.try_get_child<g::begin_header, 0>()) {
        ret = run_begin_statement(contents);
    } else {
        FLOGF(error, L"Unexpected block header: %ls\n", bheader.node()->describe().c_str());
        PARSER_DIE();
    }
    return ret;
}

end_execution_reason_t parse_execution_context_t::run_for_statement(
    tnode_t<grammar::for_header> header, tnode_t<grammar::job_list> block_contents) {
    // Get the variable name: `for var_name in ...`. We expand the variable name. It better result
    // in just one.
    tnode_t<g::tok_string> var_name_node = header.child<1>();
    wcstring for_var_name = get_source(var_name_node);
    if (!expand_one(for_var_name, expand_flags_t{}, ctx)) {
        return report_error(STATUS_EXPAND_ERROR, var_name_node,
                            FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG, for_var_name.c_str());
    }

    // Get the contents to iterate over.
    wcstring_list_t arguments;
    end_execution_reason_t ret = this->expand_arguments_from_nodes(
        get_argument_nodes(header.child<3>()), &arguments, nullglob);
    if (ret != end_execution_reason_t::ok) {
        return ret;
    }

    auto var = parser->vars().get(for_var_name, ENV_DEFAULT);
    if (var && var->read_only()) {
        return report_error(STATUS_INVALID_ARGS, var_name_node,
                            L"You cannot use read-only variable '%ls' in a for loop",
                            for_var_name.c_str());
    }
    int retval;
    if (var) {
        retval = parser->set_var_and_fire(for_var_name, ENV_LOCAL | ENV_USER, var->as_list());
    } else {
        retval = parser->set_empty_var_and_fire(for_var_name, ENV_LOCAL | ENV_USER);
    }
    assert(retval == ENV_OK);

    if (!valid_var_name(for_var_name)) {
        return report_error(STATUS_INVALID_ARGS, var_name_node, BUILTIN_ERR_VARNAME, L"for",
                            for_var_name.c_str());
    }

    trace_if_enabled(*parser, L"for", arguments);
    block_t *fb = parser->push_block(block_t::for_block());

    // Now drive the for loop.
    for (const wcstring &val : arguments) {
        if (auto reason = check_end_execution()) {
            ret = *reason;
            break;
        }

        int retval = parser->set_var_and_fire(for_var_name, ENV_DEFAULT | ENV_USER, val);
        assert(retval == ENV_OK && "for loop variable should have been successfully set");
        (void)retval;

        auto &ld = parser->libdata();
        ld.loop_status = loop_status_t::normals;
        this->run_job_list(block_contents, fb);

        if (check_end_execution() == end_execution_reason_t::control_flow) {
            // Handle break or continue.
            bool do_break = (ld.loop_status == loop_status_t::breaks);
            ld.loop_status = loop_status_t::normals;
            if (do_break) {
                break;
            }
        }
    }

    parser->pop_block(fb);
    trace_if_enabled(*parser, L"end for");
    return ret;
}

end_execution_reason_t parse_execution_context_t::run_switch_statement(
    tnode_t<grammar::switch_statement> statement) {
    // Get the switch variable.
    tnode_t<grammar::argument> switch_value_n = statement.child<1>();
    const wcstring switch_value = get_source(switch_value_n);

    // Expand it. We need to offset any errors by the position of the string.
    completion_list_t switch_values_expanded;
    parse_error_list_t errors;
    auto expand_ret = expand_string(switch_value, &switch_values_expanded,
                                    expand_flag::no_descriptions, ctx, &errors);
    parse_error_offset_source_start(&errors, switch_value_n.source_range()->start);

    switch (expand_ret.result) {
        case expand_result_t::error:
            return report_errors(expand_ret.status, errors);

        case expand_result_t::cancel:
            return end_execution_reason_t::cancelled;

        case expand_result_t::wildcard_no_match:
            return report_error(STATUS_UNMATCHED_WILDCARD, switch_value_n, WILDCARD_ERR_MSG,
                                get_source(switch_value_n).c_str());

        case expand_result_t::ok:
            if (switch_values_expanded.size() > 1) {
                return report_error(STATUS_INVALID_ARGS, switch_value_n,
                                    _(L"switch: Expected at most one argument, got %lu\n"),
                                    switch_values_expanded.size());
            }
            break;
    }

    // If we expanded to nothing, match the empty string.
    assert(switch_values_expanded.size() <= 1 && "Should have at most one expansion");
    wcstring switch_value_expanded;
    if (!switch_values_expanded.empty()) {
        switch_value_expanded = std::move(switch_values_expanded.front().completion);
    }

    end_execution_reason_t result = end_execution_reason_t::ok;
    block_t *sb = parser->push_block(block_t::switch_block());

    // Expand case statements.
    tnode_t<g::case_item_list> case_item_list = statement.child<3>();
    tnode_t<g::case_item> matching_case_item{};
    while (auto case_item = case_item_list.next_in_list<g::case_item>()) {
        if (auto ret = check_end_execution()) {
            result = *ret;
            break;
        }

        // Expand arguments. A case item list may have a wildcard that fails to expand to
        // anything. We also report case errors, but don't stop execution; i.e. a case item that
        // contains an unexpandable process will report and then fail to match.
        auto arg_nodes = get_argument_nodes(case_item.child<1>());
        wcstring_list_t case_args;
        end_execution_reason_t case_result =
            this->expand_arguments_from_nodes(arg_nodes, &case_args, failglob);
        if (case_result == end_execution_reason_t::ok) {
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
        assert(result == end_execution_reason_t::ok && "Expected success");
        auto job_list = matching_case_item.child<3>();
        result = this->run_job_list(job_list, sb);
    }

    parser->pop_block(sb);
    return result;
}

end_execution_reason_t parse_execution_context_t::run_while_statement(
    tnode_t<grammar::while_header> header, tnode_t<grammar::job_list> contents,
    const block_t *associated_block) {
    end_execution_reason_t ret = end_execution_reason_t::ok;

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

    trace_if_enabled(*parser, L"while");

    // Run while the condition is true.
    for (;;) {
        // Save off the exit status if it came from the loop body. We'll restore it if the condition
        // is false.
        auto cond_saved_status =
            first_cond_check ? statuses_t::just(EXIT_SUCCESS) : parser->get_last_statuses();
        first_cond_check = false;

        // Check the condition.
        end_execution_reason_t cond_ret =
            this->run_job_conjunction(condition_head, associated_block);
        if (cond_ret == end_execution_reason_t::ok) {
            cond_ret = run_job_list(condition_boolean_tail, associated_block);
        }

        // If the loop condition failed to execute, then exit the loop without modifying the exit
        // status. If the loop condition executed with a failure status, restore the status and then
        // exit the loop.
        if (cond_ret != end_execution_reason_t::ok) {
            break;
        } else if (parser->get_last_status() != EXIT_SUCCESS) {
            parser->set_last_statuses(cond_saved_status);
            break;
        }

        // Check cancellation.
        if (auto reason = check_end_execution()) {
            ret = *reason;
            break;
        }

        // Push a while block and then check its cancellation reason.
        auto &ld = parser->libdata();
        ld.loop_status = loop_status_t::normals;

        block_t *wb = parser->push_block(block_t::while_block());
        this->run_job_list(contents, wb);
        auto cancel_reason = this->check_end_execution();
        parser->pop_block(wb);

        if (cancel_reason == end_execution_reason_t::control_flow) {
            // Handle break or continue.
            bool do_break = (ld.loop_status == loop_status_t::breaks);
            ld.loop_status = loop_status_t::normals;
            if (do_break) {
                break;
            } else {
                continue;
            }
        }

        // no_exec means that fish was invoked with -n or --no-execute. If set, we allow the loop to
        // not-execute once so its contents can be checked, and then break.
        if (no_exec()) {
            break;
        }
    }
    trace_if_enabled(*parser, L"end while");
    return ret;
}

// Reports an error. Always returns end_execution_reason_t::error.
end_execution_reason_t parse_execution_context_t::report_error(int status, const parse_node_t &node,
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

    return this->report_errors(status, error_list);
}

end_execution_reason_t parse_execution_context_t::report_errors(
    int status, const parse_error_list_t &error_list) const {
    if (!parser->cancellation_signal) {
        if (error_list.empty()) {
            FLOG(error, L"Error reported but no error text found.");
        }

        // Get a backtrace.
        wcstring backtrace_and_desc;
        parser->get_backtrace(pstree->src, error_list, backtrace_and_desc);

        // Print it.
        if (!should_suppress_stderr_for_tests()) {
            std::fwprintf(stderr, L"%ls", backtrace_and_desc.c_str());
        }

        // Mark status.
        parser->set_last_statuses(statuses_t::just(status));
    }
    return end_execution_reason_t::error;
}

/// Handle the case of command not found.
end_execution_reason_t parse_execution_context_t::handle_command_not_found(
    const wcstring &cmd_str, tnode_t<g::plain_statement> statement, int err_code) {
    // We couldn't find the specified command. This is a non-fatal error. We want to set the exit
    // status to 127, which is the standard number used by other shells like bash and zsh.

    const wchar_t *const cmd = cmd_str.c_str();
    if (err_code != ENOENT) {
        return this->report_error(STATUS_NOT_EXECUTABLE, statement,
                                  _(L"The file '%ls' is not executable by this user"), cmd);
    } else {
        // Handle unrecognized commands with standard command not found handler that can make better
        // error messages.
        wcstring_list_t event_args;
        {
            auto args = get_argument_nodes(statement.child<1>());
            end_execution_reason_t arg_result =
                this->expand_arguments_from_nodes(args, &event_args, failglob);

            if (arg_result != end_execution_reason_t::ok) {
                return arg_result;
            }

            event_args.insert(event_args.begin(), cmd_str);
        }

        event_fire_generic(*parser, L"fish_command_not_found", &event_args);

        // Here we want to report an error (so it shows a backtrace), but with no text.
        return this->report_error(STATUS_CMD_UNKNOWN, statement, L"");
    }
}

end_execution_reason_t parse_execution_context_t::expand_command(
    tnode_t<grammar::plain_statement> statement, wcstring *out_cmd,
    wcstring_list_t *out_args) const {
    // Here we're expanding a command, for example $HOME/bin/stuff or $randomthing. The first
    // completion becomes the command itself, everything after becomes arguments. Command
    // substitutions are not supported.
    parse_error_list_t errors;

    // Get the unexpanded command string. We expect to always get it here.
    wcstring unexp_cmd = *command_for_plain_statement(statement, pstree->src);
    size_t pos_of_command_token = statement.child<0>().source_range()->start;

    // Expand the string to produce completions, and report errors.
    expand_result_t expand_err =
        expand_to_command_and_args(unexp_cmd, ctx, out_cmd, out_args, &errors);
    if (expand_err == expand_result_t::error) {
        // Issue #5812 - the expansions were done on the command token,
        // excluding prefixes such as " " or "if ".
        // This means that the error positions are relative to the beginning
        // of the token; we need to make them relative to the original source.
        for (auto &error : errors) error.source_start += pos_of_command_token;
        return report_errors(STATUS_ILLEGAL_CMD, errors);
    } else if (expand_err == expand_result_t::wildcard_no_match) {
        return report_error(STATUS_UNMATCHED_WILDCARD, statement, WILDCARD_ERR_MSG,
                            get_source(statement).c_str());
    }
    assert(expand_err == expand_result_t::ok);

    // Complain if the resulting expansion was empty, or expanded to an empty string.
    // For no-exec it's okay, as we can't really perform the expansion.
    if (out_cmd->empty() && !no_exec()) {
        return this->report_error(STATUS_ILLEGAL_CMD, statement,
                                  _(L"The expanded command was empty."));
    }
    return end_execution_reason_t::ok;
}

/// Creates a 'normal' (non-block) process.
end_execution_reason_t parse_execution_context_t::populate_plain_process(
    job_t *job, process_t *proc, tnode_t<grammar::plain_statement> statement) {
    assert(job != nullptr);
    assert(proc != nullptr);

    // We may decide that a command should be an implicit cd.
    bool use_implicit_cd = false;

    // Get the command and any arguments due to expanding the command.
    wcstring cmd;
    wcstring_list_t args_from_cmd_expansion;
    auto ret = expand_command(statement, &cmd, &args_from_cmd_expansion);
    if (ret != end_execution_reason_t::ok) {
        return ret;
    }
    // For no-exec, having an empty command is okay. We can't do anything more with it tho.
    if (no_exec()) return end_execution_reason_t::ok;
    assert(!cmd.empty() && "expand_command should not produce an empty command");

    // Determine the process type.
    enum process_type_t process_type = process_type_for_command(statement, cmd);

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
            // No command. If we're --no-execute return okay - it might be a function.
            if (no_exec()) return end_execution_reason_t::ok;
            return this->handle_command_not_found(cmd, statement, no_cmd_err_code);
        }
    }

    // Produce the full argument list and the set of IO redirections.
    wcstring_list_t cmd_args;
    redirection_spec_list_t redirections;
    if (use_implicit_cd) {
        // Implicit cd is simple.
        cmd_args = {L"cd", cmd};
        path_to_external_command.clear();

        // If we have defined a wrapper around cd, use it, otherwise use the cd builtin.
        process_type =
            function_exists(L"cd", *parser) ? process_type_t::function : process_type_t::builtin;
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
        end_execution_reason_t arg_result =
            this->expand_arguments_from_nodes(arg_nodes, &cmd_args, glob_behavior);
        if (arg_result != end_execution_reason_t::ok) {
            return arg_result;
        }

        // The set of IO redirections that we construct for the process.
        auto reason = this->determine_redirections(statement.child<1>(), &redirections);
        if (reason != end_execution_reason_t::ok) {
            return reason;
        }

        // Determine the process type.
        process_type = process_type_for_command(statement, cmd);
    }

    // Populate the process.
    proc->type = process_type;
    proc->set_argv(cmd_args);
    proc->set_redirection_specs(std::move(redirections));
    proc->actual_cmd = std::move(path_to_external_command);
    return end_execution_reason_t::ok;
}

// Determine the list of arguments, expanding stuff. Reports any errors caused by expansion. If we
// have a wildcard that could not be expanded, report the error and continue.
end_execution_reason_t parse_execution_context_t::expand_arguments_from_nodes(
    const argument_node_list_t &argument_nodes, wcstring_list_t *out_arguments,
    globspec_t glob_behavior) {
    // Get all argument nodes underneath the statement. We guess we'll have that many arguments (but
    // may have more or fewer, if there are wildcards involved).
    out_arguments->reserve(out_arguments->size() + argument_nodes.size());
    completion_list_t arg_expanded;
    for (const auto &arg_node : argument_nodes) {
        // Expect all arguments to have source.
        assert(arg_node.has_source());
        const wcstring arg_str = arg_node.get_source(pstree->src);

        // Expand this string.
        parse_error_list_t errors;
        arg_expanded.clear();
        auto expand_ret =
            expand_string(arg_str, &arg_expanded, expand_flag::no_descriptions, ctx, &errors);
        parse_error_offset_source_start(&errors, arg_node.source_range()->start);
        switch (expand_ret.result) {
            case expand_result_t::error: {
                return this->report_errors(expand_ret.status, errors);
            }

            case expand_result_t::cancel: {
                return end_execution_reason_t::cancelled;
            }
            case expand_result_t::wildcard_no_match: {
                if (glob_behavior == failglob) {
                    // For no_exec, ignore the error - this might work at runtime.
                    if (no_exec()) return end_execution_reason_t::ok;
                    // Report the unmatched wildcard error and stop processing.
                    return report_error(STATUS_UNMATCHED_WILDCARD, arg_node, WILDCARD_ERR_MSG,
                                        get_source(arg_node).c_str());
                }
                break;
            }
            case expand_result_t::ok: {
                break;
            }
            default: {
                DIE("unexpected expand_string() return value");
            }
        }

        // Now copy over any expanded arguments. Use std::move() to avoid extra allocations; this
        // is called very frequently.
        out_arguments->reserve(out_arguments->size() + arg_expanded.size());
        for (completion_t &new_arg : arg_expanded) {
            out_arguments->push_back(std::move(new_arg.completion));
        }
    }

    // We may have received a cancellation during this expansion.
    if (auto ret = check_end_execution()) {
        return *ret;
    }

    return end_execution_reason_t::ok;
}

end_execution_reason_t parse_execution_context_t::determine_redirections(
    tnode_t<g::arguments_or_redirections_list> node, redirection_spec_list_t *out_redirections) {
    // Get all redirection nodes underneath the statement.
    while (auto redirect_node = node.next_in_list<g::redirection>()) {
        wcstring target;  // file path or target fd
        auto redirect = redirection_for_node(redirect_node, pstree->src, &target);

        if (!redirect || !redirect->is_valid()) {
            // TODO: figure out if this can ever happen. If so, improve this error message.
            return report_error(STATUS_INVALID_ARGS, redirect_node, _(L"Invalid redirection: %ls"),
                                redirect_node.get_source(pstree->src).c_str());
        }

        // PCA: I can't justify this skip_variables flag. It was like this when I got here.
        bool target_expanded =
            expand_one(target, no_exec() ? expand_flag::skip_variables : expand_flags_t{}, ctx);
        if (!target_expanded || target.empty()) {
            // TODO: Improve this error message.
            return report_error(STATUS_INVALID_ARGS, redirect_node,
                                _(L"Invalid redirection target: %ls"), target.c_str());
        }

        // Make a redirection spec from the redirect token.
        assert(redirect && redirect->is_valid() && "expected to have a valid redirection");

        redirection_spec_t spec{redirect->fd, redirect->mode, std::move(target)};

        // Validate this spec.
        if (spec.mode == redirection_mode_t::fd && !spec.is_close() && !spec.get_target_as_fd()) {
            const wchar_t *fmt =
                _(L"Requested redirection to '%ls', which is not a valid file descriptor");
            return report_error(STATUS_INVALID_ARGS, redirect_node, fmt, spec.target.c_str());
        }
        out_redirections->push_back(std::move(spec));

        if (redirect->stderr_merge) {
            // This was a redirect like &> which also modifies stderr.
            // Also redirect stderr to stdout.
            out_redirections->push_back(get_stderr_merge());
        }
    }
    return end_execution_reason_t::ok;
}

end_execution_reason_t parse_execution_context_t::populate_not_process(
    job_t *job, process_t *proc, tnode_t<g::not_statement> not_statement) {
    auto &flags = job->mut_flags();
    flags.negate = !flags.negate;
    auto optional_time = not_statement.require_get_child<g::optional_time, 2>();
    if (optional_time.tag() == parse_optional_time_time) {
        flags.has_time_prefix = true;
        if (!job->mut_flags().foreground) {
            return this->report_error(STATUS_INVALID_ARGS, not_statement, ERROR_TIME_BACKGROUND);
        }
    }
    return this->populate_job_process(
        job, proc, not_statement.require_get_child<g::statement, 3>(),
        not_statement.require_get_child<g::variable_assignments, 1>());
}

template <typename Type>
end_execution_reason_t parse_execution_context_t::populate_block_process(
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
    redirection_spec_list_t redirections;
    auto reason = this->determine_redirections(arguments, &redirections);
    if (reason == end_execution_reason_t::ok) {
        proc->type = process_type_t::block_node;
        proc->block_node_source = pstree;
        proc->internal_block_node = statement;
        proc->set_redirection_specs(std::move(redirections));
    }
    return reason;
}

end_execution_reason_t parse_execution_context_t::apply_variable_assignments(
    process_t *proc, tnode_t<grammar::variable_assignments> variable_assignments,
    const block_t **block) {
    variable_assignment_node_list_t assignment_list =
        get_variable_assignment_nodes(variable_assignments);
    if (assignment_list.empty()) return end_execution_reason_t::ok;
    *block = parser->push_block(block_t::variable_assignment_block());
    for (const auto &variable_assignment : assignment_list) {
        const wcstring &source = variable_assignment.get_source(pstree->src);
        auto equals_pos = variable_assignment_equals_pos(source);
        assert(equals_pos);
        const wcstring variable_name = source.substr(0, *equals_pos);
        const wcstring expression = source.substr(*equals_pos + 1);
        completion_list_t expression_expanded;
        parse_error_list_t errors;
        // TODO this is mostly copied from expand_arguments_from_nodes, maybe extract to function
        auto expand_ret = expand_string(expression, &expression_expanded,
                                        expand_flag::no_descriptions, ctx, &errors);
        parse_error_offset_source_start(
            &errors, variable_assignment.source_range()->start + *equals_pos + 1);
        switch (expand_ret.result) {
            case expand_result_t::error:
                return this->report_errors(expand_ret.status, errors);

            case expand_result_t::cancel:
                return end_execution_reason_t::cancelled;

            case expand_result_t::wildcard_no_match:  // nullglob (equivalent to set)
            case expand_result_t::ok:
                break;

            default: {
                DIE("unexpected expand_string() return value");
            }
        }
        wcstring_list_t vals;
        for (auto &completion : expression_expanded) {
            vals.emplace_back(std::move(completion.completion));
        }
        if (proc) proc->variable_assignments.push_back({variable_name, vals});
        parser->set_var_and_fire(variable_name, ENV_LOCAL | ENV_EXPORT, std::move(vals));
    }
    return end_execution_reason_t::ok;
}

end_execution_reason_t parse_execution_context_t::populate_job_process(
    job_t *job, process_t *proc, tnode_t<grammar::statement> statement,
    tnode_t<grammar::variable_assignments> variable_assignments) {
    // Get the "specific statement" which is boolean / block / if / switch / decorated.
    const parse_node_t &specific_statement = statement.get_child_node<0>();

    const block_t *block = nullptr;
    end_execution_reason_t result =
        this->apply_variable_assignments(proc, variable_assignments, &block);
    cleanup_t scope([&]() {
        if (block) parser->pop_block(block);
    });
    if (result != end_execution_reason_t::ok) return result;

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
            FLOGF(error, L"'%ls' not handled by new parser yet.",
                  specific_statement.describe().c_str());
            PARSER_DIE();
            break;
        }
    }

    return result;
}

end_execution_reason_t parse_execution_context_t::populate_job_from_job_node(
    job_t *j, tnode_t<grammar::job> job_node, const block_t *associated_block) {
    UNUSED(associated_block);

    // Tell the job what its command is.
    j->set_command(get_source(job_node));

    // We are going to construct process_t structures for every statement in the job. Get the first
    // statement.
    tnode_t<g::optional_time> optional_time = job_node.child<0>();
    tnode_t<g::variable_assignments> variable_assignments = job_node.child<1>();
    tnode_t<g::statement> statement = job_node.child<2>();

    // Create processes. Each one may fail.
    process_list_t processes;
    processes.emplace_back(new process_t());
    if (optional_time.tag() == parse_optional_time_time) {
        j->mut_flags().has_time_prefix = true;
        if (job_node_is_background(job_node)) {
            return this->report_error(STATUS_INVALID_ARGS, job_node, ERROR_TIME_BACKGROUND);
        }
    }
    end_execution_reason_t result =
        this->populate_job_process(j, processes.back().get(), statement, variable_assignments);

    // Construct process_ts for job continuations (pipelines), by walking the list until we hit the
    // terminal (empty) job continuation.
    tnode_t<g::job_continuation> job_cont = job_node.child<3>();
    assert(job_cont);
    while (auto pipe = job_cont.try_get_child<g::tok_pipe, 0>()) {
        if (result != end_execution_reason_t::ok) {
            break;
        }
        auto variable_assignments = job_cont.require_get_child<g::variable_assignments, 2>();
        auto statement = job_cont.require_get_child<g::statement, 3>();

        // Handle the pipe, whose fd may not be the obvious stdout.
        auto parsed_pipe = pipe_or_redir_t::from_string(get_source(pipe));
        assert(parsed_pipe.has_value() && parsed_pipe->is_pipe && "Failed to parse valid pipe");
        if (!parsed_pipe->is_valid()) {
            result = report_error(STATUS_INVALID_ARGS, pipe, ILLEGAL_FD_ERR_MSG,
                                  get_source(pipe).c_str());
            break;
        }
        processes.back()->pipe_write_fd = parsed_pipe->fd;
        if (parsed_pipe->stderr_merge) {
            // This was a pipe like &| which redirects both stdout and stderr.
            // Also redirect stderr to stdout.
            auto specs = processes.back()->redirection_specs();
            specs.push_back(get_stderr_merge());
            processes.back()->set_redirection_specs(std::move(specs));
        }

        // Store the new process (and maybe with an error).
        processes.emplace_back(new process_t());
        result =
            this->populate_job_process(j, processes.back().get(), statement, variable_assignments);

        // Get the next continuation.
        job_cont = job_cont.require_get_child<g::job_continuation, 4>();
        assert(job_cont);
    }

    // Inform our processes of who is first and last
    processes.front()->is_first_in_job = true;
    processes.back()->is_last_in_job = true;

    // Return what happened.
    if (result == end_execution_reason_t::ok) {
        // Link up the processes.
        assert(!processes.empty());  //!OCLINT(multiple unary operator)
        j->processes = std::move(processes);
    }
    return result;
}

static bool remove_job(parser_t &parser, job_t *job) {
    for (auto j = parser.jobs().begin(); j != parser.jobs().end(); ++j) {
        if (j->get() == job) {
            parser.jobs().erase(j);
            return true;
        }
    }
    return false;
}

end_execution_reason_t parse_execution_context_t::run_1_job(tnode_t<g::job> job_node,
                                                            const block_t *associated_block) {
    if (auto ret = check_end_execution()) {
        return *ret;
    }

    // We definitely do not want to execute anything if we're told we're --no-execute!
    if (no_exec()) return end_execution_reason_t::ok;

    // Get terminal modes.
    struct termios tmodes = {};
    if (parser->is_interactive() && tcgetattr(STDIN_FILENO, &tmodes)) {
        // Need real error handling here.
        wperror(L"tcgetattr");
        parser->set_last_statuses(statuses_t::just(STATUS_CMD_ERROR));
        return end_execution_reason_t::error;
    }

    // Increment the eval_level for the duration of this command.
    scoped_push<int> saved_eval_level(&parser->eval_level, parser->eval_level + 1);

    // Save the node index.
    scoped_push<tnode_t<grammar::job>> saved_node(&executing_job_node, job_node);

    // Profiling support.
    long long start_time = 0, parse_time = 0, exec_time = 0;
    profile_item_t *profile_item = this->parser->create_profile_item();
    if (profile_item != nullptr) {
        start_time = get_time();
    }

    // When we encounter a block construct (e.g. while loop) in the general case, we create a "block
    // process" containing its node. This allows us to handle block-level redirections.
    // However, if there are no redirections, then we can just jump into the block directly, which
    // is significantly faster.
    if (job_is_simple_block(job_node)) {
        tnode_t<g::optional_time> optional_time = job_node.child<0>();
        // If no-exec has been given, there is nothing to time.
        cleanup_t timer = push_timer(optional_time.tag() == parse_optional_time_time && !no_exec());
        tnode_t<g::variable_assignments> variable_assignments = job_node.child<1>();
        const block_t *block = nullptr;
        end_execution_reason_t result =
            this->apply_variable_assignments(nullptr, variable_assignments, &block);
        cleanup_t scope([&]() {
            if (block) parser->pop_block(block);
        });

        tnode_t<g::statement> statement = job_node.child<2>();
        const parse_node_t &specific_statement = statement.get_child_node<0>();
        assert(specific_statement_type_is_redirectable_block(specific_statement));
        if (result == end_execution_reason_t::ok) {
            switch (specific_statement.type) {
                case symbol_block_statement: {
                    result =
                        this->run_block_statement({&tree(), &specific_statement}, associated_block);
                    break;
                }
                case symbol_if_statement: {
                    result =
                        this->run_if_statement({&tree(), &specific_statement}, associated_block);
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
        }

        if (profile_item != nullptr) {
            // Block-types profile a little weird. They have no 'parse' time, and their command is
            // just the block type.
            exec_time = get_time();
            profile_item->level = parser->eval_level;
            profile_item->parse = 0;
            profile_item->exec = static_cast<int>(exec_time - start_time);
            profile_item->cmd = profiling_cmd_name_for_redirectable_block(
                specific_statement, this->tree(), this->pstree->src);
            profile_item->skipped = false;
        }

        return result;
    }

    const auto &ld = parser->libdata();

    auto job_control_mode = get_job_control_mode();
    bool wants_job_control =
        (job_control_mode == job_control_t::all) ||
        ((job_control_mode == job_control_t::interactive) && parser->is_interactive()) ||
        lineage.root_has_job_control;

    job_t::properties_t props{};
    props.wants_terminal = wants_job_control && !ld.is_event;
    props.skip_notification =
        ld.is_subshell || ld.is_block || ld.is_event || !parser->is_interactive();
    props.from_event_handler = ld.is_event;
    props.job_control = wants_job_control;

    shared_ptr<job_t> job = std::make_shared<job_t>(acquire_job_id(), props, this->lineage);
    job->tmodes = tmodes;

    job->mut_flags().foreground = !job_node_is_background(job_node);

    // We are about to populate a job. One possible argument to the job is a command substitution
    // which may be interested in the job that's populating it, via '--on-job-exit caller'. Record
    // the job ID here.
    scoped_push<internal_job_id_t> caller_id(&parser->libdata().caller_id, job->internal_job_id);

    // Populate the job. This may fail for reasons like command_not_found. If this fails, an error
    // will have been printed.
    end_execution_reason_t pop_result =
        this->populate_job_from_job_node(job.get(), job_node, associated_block);
    caller_id.restore();

    // Store time it took to 'parse' the command.
    if (profile_item != nullptr) {
        parse_time = get_time();
    }

    // Clean up the job on failure or cancellation.
    if (pop_result == end_execution_reason_t::ok) {
        // Set the pgroup assignment mode, now that the job is populated.
        job->pgroup_provenance = get_pgroup_provenance(job, lineage);

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
        if (!exec_job(*this->parser, job, lineage)) {
            remove_job(*this->parser, job.get());
        }

        // Update universal variables on external conmmands.
        // TODO: justify this, why not on every command?
        if (job_contained_external_command) {
            parser->vars().universal_barrier();
        }
    }

    if (profile_item != nullptr) {
        exec_time = get_time();
        profile_item->level = parser->eval_level;
        profile_item->parse = static_cast<int>(parse_time - start_time);
        profile_item->exec = static_cast<int>(exec_time - parse_time);
        profile_item->cmd = job ? job->command() : wcstring();
        profile_item->skipped = (pop_result != end_execution_reason_t::ok);
    }

    job_reap(*parser, false);  // clean up jobs
    return pop_result;
}

end_execution_reason_t parse_execution_context_t::run_job_conjunction(
    tnode_t<grammar::job_conjunction> job_expr, const block_t *associated_block) {
    end_execution_reason_t result = end_execution_reason_t::ok;
    tnode_t<g::job_conjunction> cursor = job_expr;
    // continuation is the parent of the cursor
    tnode_t<g::job_conjunction_continuation> continuation;
    while (cursor) {
        if (auto reason = check_end_execution()) {
            result = *reason;
            break;
        }
        bool skip = false;
        if (continuation) {
            // Check the conjunction type.
            parse_job_decoration_t conj = bool_statement_type(continuation);
            assert((conj == parse_job_decoration_and || conj == parse_job_decoration_or) &&
                   "Unexpected conjunction");
            skip = should_skip(conj);
        }
        if (!skip) {
            result = run_1_job(cursor.child<0>(), associated_block);
        }
        continuation = cursor.child<1>();
        cursor = continuation.try_get_child<g::job_conjunction, 2>();
    }
    return result;
}

bool parse_execution_context_t::should_skip(parse_job_decoration_t type) const {
    switch (type) {
        case parse_job_decoration_and:
            // AND. Skip if the last job failed.
            return parser->get_last_status() != 0;
        case parse_job_decoration_or:
            // OR. Skip if the last job succeeded.
            return parser->get_last_status() == 0;
        default:
            return false;
    }
}

template <typename Type>
end_execution_reason_t parse_execution_context_t::run_job_list(tnode_t<Type> job_list,
                                                               const block_t *associated_block) {
    // We handle both job_list and andor_job_list uniformly.
    static_assert(Type::token == symbol_job_list || Type::token == symbol_andor_job_list,
                  "Not a job list");

    end_execution_reason_t result = end_execution_reason_t::ok;
    while (auto job_conj = job_list.template next_in_list<g::job_conjunction>()) {
        if (auto reason = check_end_execution()) {
            result = *reason;
            break;
        }

        // Maybe skip the job if it has a leading and/or.
        // Skipping is treated as success.
        if (should_skip(get_decorator(job_conj))) {
            result = end_execution_reason_t::ok;
        } else {
            result = this->run_job_conjunction(job_conj, associated_block);
        }
    }

    // Returns the result of the last job executed or skipped.
    return result;
}

end_execution_reason_t parse_execution_context_t::eval_node(tnode_t<g::statement> statement,
                                                            const block_t *associated_block) {
    assert(statement && "Empty node in eval_node");
    assert(statement.matches_node_tree(tree()) && "statement has unexpected tree");
    enum end_execution_reason_t status = end_execution_reason_t::ok;
    if (auto block = statement.try_get_child<g::block_statement, 0>()) {
        status = this->run_block_statement(block, associated_block);
    } else if (auto ifstat = statement.try_get_child<g::if_statement, 0>()) {
        status = this->run_if_statement(ifstat, associated_block);
    } else if (auto switchstat = statement.try_get_child<g::switch_statement, 0>()) {
        status = this->run_switch_statement(switchstat);
    } else {
        FLOGF(error, L"Unexpected node %ls found in %s", statement.node()->describe().c_str(),
              __FUNCTION__);
        abort();
    }
    return status;
}

end_execution_reason_t parse_execution_context_t::eval_node(tnode_t<g::job_list> job_list,
                                                            const block_t *associated_block) {
    // Apply this block IO for the duration of this function.
    assert(job_list && "Empty node in eval_node");
    assert(job_list.matches_node_tree(tree()) && "job_list has unexpected tree");
    assert(associated_block && "Null block");

    // Check for infinite recursion: a function which immediately calls itself..
    wcstring func_name;
    auto infinite_recursive_node =
        this->infinite_recursive_statement_in_job_list(job_list, &func_name);
    if (infinite_recursive_node) {
        // We have an infinite recursion.
        return this->report_error(STATUS_CMD_ERROR, infinite_recursive_node,
                                  INFINITE_FUNC_RECURSION_ERR_MSG, func_name.c_str());
    }

    // Check for stack overflow. The TOP check ensures we only do this for function calls.
    if (associated_block->type() == block_type_t::top && parser->function_stack_is_overflowing()) {
        return this->report_error(STATUS_CMD_ERROR, job_list, CALL_STACK_LIMIT_EXCEEDED_ERR_MSG);
    }
    return this->run_job_list(job_list, associated_block);
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
