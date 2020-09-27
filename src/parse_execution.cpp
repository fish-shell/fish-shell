// Provides the "linkage" between an ast and actual execution structures (job_t, etc.)
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

#include "ast.h"
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
#include "job_group.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "timer.h"
#include "tokenizer.h"
#include "trace.h"
#include "util.h"
#include "wildcard.h"
#include "wutil.h"

/// These are the specific statement types that support redirections.
static constexpr bool type_is_redirectable_block(ast::type_t type) {
    using t = ast::type_t;
    return type == t::block_statement || type == t::if_statement || type == t::switch_statement;
}

static bool specific_statement_type_is_redirectable_block(const ast::node_t &node) {
    return type_is_redirectable_block(node.type);
}

/// Get the name of a redirectable block, for profiling purposes.
static wcstring profiling_cmd_name_for_redirectable_block(const ast::node_t &node,
                                                          const parsed_source_t &pstree) {
    using namespace ast;
    assert(specific_statement_type_is_redirectable_block(node));

    auto source_range = node.try_source_range();
    assert(source_range.has_value() && "No source range for block");

    size_t src_end = 0;
    switch (node.type) {
        case type_t::block_statement: {
            const node_t *block_header = node.as<block_statement_t>()->header.get();
            switch (block_header->type) {
                case type_t::for_header:
                    src_end = block_header->as<for_header_t>()->semi_nl.source_range().start;
                    break;

                case type_t::while_header:
                    src_end = block_header->as<while_header_t>()->condition.source_range().end();
                    break;

                case type_t::function_header:
                    src_end = block_header->as<function_header_t>()->semi_nl.source_range().start;
                    break;

                case type_t::begin_header:
                    src_end = block_header->as<begin_header_t>()->kw_begin.source_range().end();
                    break;

                default:
                    DIE("Unexpected block header type");
            }
        } break;

        case type_t::if_statement:
            src_end = node.as<if_statement_t>()->if_clause.condition.job.source_range().end();
            break;

        case type_t::switch_statement:
            src_end = node.as<switch_statement_t>()->semi_nl.source_range().start;
            break;

        default:
            DIE("Not a redirectable block type");
            break;
    }

    assert(src_end >= source_range->start && "Invalid source end");

    // Get the source for the block, and cut it at the next statement terminator.
    wcstring result = pstree.src.substr(source_range->start, src_end - source_range->start);
    result.append(L"...");
    return result;
}

/// Get a redirection from stderr to stdout (i.e. 2>&1).
static redirection_spec_t get_stderr_merge() {
    const wchar_t *stdout_fileno_str = L"1";
    return redirection_spec_t{STDERR_FILENO, redirection_mode_t::fd, stdout_fileno_str};
}

parse_execution_context_t::parse_execution_context_t(parsed_source_ref_t pstree,
                                                     const operation_context_t &ctx,
                                                     cancellation_group_ref_t cancel_group,
                                                     io_chain_t block_io)
    : pstree(std::move(pstree)),
      parser(ctx.parser.get()),
      ctx(ctx),
      cancel_group(std::move(cancel_group)),
      block_io(std::move(block_io)) {}

// Utilities

wcstring parse_execution_context_t::get_source(const ast::node_t &node) const {
    return node.source(pstree->src);
}

const ast::decorated_statement_t *
parse_execution_context_t::infinite_recursive_statement_in_job_list(const ast::job_list_t &jobs,
                                                                    wcstring *out_func_name) const {
    // This is a bit fragile. It is a test to see if we are inside of function call, but not inside
    // a block in that function call. If, in the future, the rules for what block scopes are pushed
    // on function invocation changes, then this check will break.
    const block_t *current = parser->block_at_index(0), *parent = parser->block_at_index(1);
    bool is_within_function_call =
        (current && parent && current->type() == block_type_t::top && parent->is_function_call());
    if (!is_within_function_call) {
        return nullptr;
    }

    // Get the function name of the immediate block.
    const wcstring &forbidden_function_name = parent->function_name;

    // Get the first job in the job list.
    const ast::job_conjunction_t *jc = jobs.at(0);
    if (!jc) return nullptr;
    const ast::job_t *job = &jc->job;

    // Helper to return if a statement is infinitely recursive in this function.
    auto statement_recurses =
        [&](const ast::statement_t &stat) -> const ast::decorated_statement_t * {
        // Ignore non-decorated statements like `if`, etc.
        const ast::decorated_statement_t *dc =
            stat.contents.contents->try_as<ast::decorated_statement_t>();
        if (!dc) return nullptr;

        // Ignore statements with decorations like 'builtin' or 'command', since those
        // are not infinite recursion. In particular that is what enables 'wrapper functions'.
        if (dc->decoration() != statement_decoration_t::none) return nullptr;

        // Check the command.
        wcstring cmd = dc->command.source(pstree->src);
        bool forbidden =
            !cmd.empty() &&
            expand_one(cmd, {expand_flag::skip_cmdsubst, expand_flag::skip_variables}, ctx) &&
            cmd == forbidden_function_name;
        return forbidden ? dc : nullptr;
    };

    const ast::decorated_statement_t *infinite_recursive_statement = nullptr;

    // Check main statement.
    infinite_recursive_statement = statement_recurses(jc->job.statement);

    // Check piped remainder.
    if (!infinite_recursive_statement) {
        for (const ast::job_continuation_t &c : job->continuation) {
            if (const auto *s = statement_recurses(c.statement)) {
                infinite_recursive_statement = s;
                break;
            }
        }
    }

    if (infinite_recursive_statement && out_func_name) {
        *out_func_name = forbidden_function_name;
    }
    // may be null
    return infinite_recursive_statement;
}

process_type_t parse_execution_context_t::process_type_for_command(
    const ast::decorated_statement_t &statement, const wcstring &cmd) const {
    enum process_type_t process_type = process_type_t::external;

    // Determine the process type, which depends on the statement decoration (command, builtin,
    // etc).
    switch (statement.decoration()) {
        case statement_decoration_t::exec:
            process_type = process_type_t::exec;
            break;
        case statement_decoration_t::command:
            process_type = process_type_t::external;
            break;
        case statement_decoration_t::builtin:
            process_type = process_type_t::builtin;
            break;
        case statement_decoration_t::none:
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
    if (ctx.check_cancel() || check_cancel_from_fish_signal()) {
        return end_execution_reason_t::cancelled;
    }
    const auto &ld = parser->libdata();
    if (ld.exit_current_script) {
        return end_execution_reason_t::cancelled;
    }
    if (ld.returning) {
        return end_execution_reason_t::control_flow;
    }
    if (ld.loop_status != loop_status_t::normals) {
        return end_execution_reason_t::control_flow;
    }
    return none();
}

/// Return whether the job contains a single statement, of block type, with no redirections.
bool parse_execution_context_t::job_is_simple_block(const ast::job_t &job) const {
    using namespace ast;
    // Must be no pipes.
    if (!job.continuation.empty()) {
        return false;
    }

    // Helper to check if an argument_or_redirection_list_t has no redirections.
    auto no_redirs = [](const argument_or_redirection_list_t &list) -> bool {
        for (const argument_or_redirection_t &val : list) {
            if (val.is_redirection()) return false;
        }
        return true;
    };

    // Check if we're a block statement with redirections. We do it this obnoxious way to preserve
    // type safety (in case we add more specific statement types).
    const node_t &ss = *job.statement.contents.contents;
    switch (ss.type) {
        case type_t::block_statement:
            return no_redirs(ss.as<block_statement_t>()->args_or_redirs);
        case type_t::switch_statement:
            return no_redirs(ss.as<switch_statement_t>()->args_or_redirs);
        case type_t::if_statement:
            return no_redirs(ss.as<if_statement_t>()->args_or_redirs);
        case type_t::not_statement:
        case type_t::decorated_statement:
            // not block statements
            return false;
        default:
            assert(0 && "Unexpected child block type");
            return false;
    }
}

end_execution_reason_t parse_execution_context_t::run_if_statement(
    const ast::if_statement_t &statement, const block_t *associated_block) {
    using namespace ast;
    using job_list_t = ast::job_list_t;
    end_execution_reason_t result = end_execution_reason_t::ok;

    // We have a sequence of if clauses, with a final else, resulting in a single job list that we
    // execute.
    const job_list_t *job_list_to_execute = nullptr;
    const if_clause_t *if_clause = &statement.if_clause;

    // Index of the *next* elseif_clause to test.
    const elseif_clause_list_t &elseif_clauses = statement.elseif_clauses;
    size_t next_elseif_idx = 0;

    // We start with the 'if'.
    trace_if_enabled(*parser, L"if");

    for (;;) {
        if (auto ret = check_end_execution()) {
            result = *ret;
            break;
        }

        // An if condition has a job and a "tail" of andor jobs, e.g. "foo ; and bar; or baz".
        // Check the condition and the tail. We treat end_execution_reason_t::error here as failure,
        // in accordance with historic behavior.
        end_execution_reason_t cond_ret =
            run_job_conjunction(if_clause->condition, associated_block);
        if (cond_ret == end_execution_reason_t::ok) {
            cond_ret = run_job_list(if_clause->andor_tail, associated_block);
        }
        const bool take_branch =
            (cond_ret == end_execution_reason_t::ok) && parser->get_last_status() == EXIT_SUCCESS;

        if (take_branch) {
            // Condition succeeded.
            job_list_to_execute = &if_clause->body;
            break;
        }

        // See if we have an elseif.
        const auto *elseif_clause = elseif_clauses.at(next_elseif_idx++);
        if (elseif_clause) {
            trace_if_enabled(*parser, L"else if");
            if_clause = &elseif_clause->if_clause;
        } else {
            break;
        }
    }

    if (!job_list_to_execute) {
        // our ifs and elseifs failed.
        // Check our else body.
        if (statement.else_clause) {
            trace_if_enabled(*parser, L"else");
            job_list_to_execute = &statement.else_clause->body;
        }
    }

    if (!job_list_to_execute) {
        // 'if' condition failed, no else clause, return 0, we're done.
        // No job list means no successful conditions, so return 0 (issue #1443).
        parser->set_last_statuses(statuses_t::just(STATUS_CMD_OK));
    } else {
        // Execute the job list we got.
        block_t *ib = parser->push_block(block_t::if_block());
        run_job_list(*job_list_to_execute, ib);
        if (auto ret = check_end_execution()) {
            result = *ret;
        }
        parser->pop_block(ib);
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
    const ast::job_list_t &contents) {
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
    const ast::block_statement_t &statement, const ast::function_header_t &header) {
    using namespace ast;
    // Get arguments.
    wcstring_list_t arguments;
    ast_args_list_t arg_nodes = get_argument_nodes(header.args);
    arg_nodes.insert(arg_nodes.begin(), &header.first_arg);
    end_execution_reason_t result =
        this->expand_arguments_from_nodes(arg_nodes, &arguments, failglob);

    if (result != end_execution_reason_t::ok) {
        return result;
    }
    trace_if_enabled(*parser, L"function", arguments);
    // no limit on the amount of output from builtin_function()
    buffered_output_stream_t outs(0);
    buffered_output_stream_t errs(0);
    io_streams_t streams(outs, errs);
    int err_code = 0;
    maybe_t<int> err = builtin_function(*parser, streams, arguments, pstree, statement);
    if (err) {
        err_code = err.value();
        parser->libdata().status_count++;
        parser->set_last_statuses(statuses_t::just(err_code));
    }

    wcstring errtext = errs.contents();
    if (!errtext.empty()) {
        return this->report_error(err_code, header, L"%ls", errtext.c_str());
    }
    return result;
}

end_execution_reason_t parse_execution_context_t::run_block_statement(
    const ast::block_statement_t &statement, const block_t *associated_block) {
    const ast::node_t &bh = *statement.header.contents;
    const ast::job_list_t &contents = statement.jobs;
    end_execution_reason_t ret = end_execution_reason_t::ok;
    if (const auto *fh = bh.try_as<ast::for_header_t>()) {
        ret = run_for_statement(*fh, contents);
    } else if (const auto *wh = bh.try_as<ast::while_header_t>()) {
        ret = run_while_statement(*wh, contents, associated_block);
    } else if (const auto *fh = bh.try_as<ast::function_header_t>()) {
        ret = run_function_statement(statement, *fh);
    } else if (bh.try_as<ast::begin_header_t>()) {
        ret = run_begin_statement(contents);
    } else {
        FLOGF(error, L"Unexpected block header: %ls\n", bh.describe().c_str());
        PARSER_DIE();
    }
    return ret;
}

end_execution_reason_t parse_execution_context_t::run_for_statement(
    const ast::for_header_t &header, const ast::job_list_t &block_contents) {
    // Get the variable name: `for var_name in ...`. We expand the variable name. It better result
    // in just one.
    wcstring for_var_name = header.var_name.source(get_source());
    if (!expand_one(for_var_name, expand_flags_t{}, ctx)) {
        return report_error(STATUS_EXPAND_ERROR, header.var_name,
                            FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG, for_var_name.c_str());
    }

    // Get the contents to iterate over.
    wcstring_list_t arguments;
    ast_args_list_t arg_nodes = get_argument_nodes(header.args);
    end_execution_reason_t ret = this->expand_arguments_from_nodes(arg_nodes, &arguments, nullglob);
    if (ret != end_execution_reason_t::ok) {
        return ret;
    }

    auto var = parser->vars().get(for_var_name, ENV_DEFAULT);
    if (var && var->read_only()) {
        return report_error(STATUS_INVALID_ARGS, header.var_name,
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
        return report_error(STATUS_INVALID_ARGS, header.var_name, BUILTIN_ERR_VARNAME, L"for",
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
    const ast::switch_statement_t &statement) {
    // Get the switch variable.
    const wcstring switch_value = get_source(statement.argument);

    // Expand it. We need to offset any errors by the position of the string.
    completion_list_t switch_values_expanded;
    parse_error_list_t errors;
    auto expand_ret =
        expand_string(switch_value, &switch_values_expanded, expand_flags_t{}, ctx, &errors);
    parse_error_offset_source_start(&errors, statement.argument.range.start);

    switch (expand_ret.result) {
        case expand_result_t::error:
            return report_errors(expand_ret.status, errors);

        case expand_result_t::cancel:
            return end_execution_reason_t::cancelled;

        case expand_result_t::wildcard_no_match:
            return report_error(STATUS_UNMATCHED_WILDCARD, statement.argument, WILDCARD_ERR_MSG,
                                get_source(statement.argument).c_str());

        case expand_result_t::ok:
            if (switch_values_expanded.size() > 1) {
                return report_error(STATUS_INVALID_ARGS, statement.argument,
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
    if (trace_enabled(*parser)) trace_argv(*parser, L"switch", {switch_value_expanded});
    block_t *sb = parser->push_block(block_t::switch_block());

    // Expand case statements.
    const ast::case_item_t *matching_case_item = nullptr;
    for (const ast::case_item_t &case_item : statement.cases) {
        if (auto ret = check_end_execution()) {
            result = *ret;
            break;
        }

        // Expand arguments. A case item list may have a wildcard that fails to expand to
        // anything. We also report case errors, but don't stop execution; i.e. a case item that
        // contains an unexpandable process will report and then fail to match.
        ast_args_list_t arg_nodes = get_argument_nodes(case_item.arguments);
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
                    matching_case_item = &case_item;
                    break;
                }
            }
        }
        if (matching_case_item) break;
    }

    if (matching_case_item) {
        // Success, evaluate the job list.
        assert(result == end_execution_reason_t::ok && "Expected success");
        result = this->run_job_list(matching_case_item->body, sb);
    }

    parser->pop_block(sb);
    trace_if_enabled(*parser, L"end switch");
    return result;
}

end_execution_reason_t parse_execution_context_t::run_while_statement(
    const ast::while_header_t &header, const ast::job_list_t &contents,
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
            this->run_job_conjunction(header.condition, associated_block);
        if (cond_ret == end_execution_reason_t::ok) {
            cond_ret = run_job_list(header.andor_tail, associated_block);
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
end_execution_reason_t parse_execution_context_t::report_error(int status, const ast::node_t &node,
                                                               const wchar_t *fmt, ...) const {
    auto r = node.source_range();

    // Create an error.
    parse_error_list_t error_list = parse_error_list_t(1);
    parse_error_t *error = &error_list.at(0);
    error->source_start = r.start;
    error->source_length = r.length;
    error->code = parse_error_syntax;  // hackish

    va_list va;
    va_start(va, fmt);
    error->text = vformat_string(fmt, va);
    va_end(va);

    return this->report_errors(status, error_list);
}

end_execution_reason_t parse_execution_context_t::report_errors(
    int status, const parse_error_list_t &error_list) const {
    if (!ctx.check_cancel()) {
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

// static
parse_execution_context_t::ast_args_list_t parse_execution_context_t::get_argument_nodes(
    const ast::argument_list_t &args) {
    ast_args_list_t result;
    for (const ast::argument_t &arg : args) result.push_back(&arg);
    return result;
}

// static
parse_execution_context_t::ast_args_list_t parse_execution_context_t::get_argument_nodes(
    const ast::argument_or_redirection_list_t &args) {
    ast_args_list_t result;
    for (const ast::argument_or_redirection_t &v : args) {
        if (v.is_argument()) result.push_back(&v.argument());
    }
    return result;
}

/// Handle the case of command not found.
end_execution_reason_t parse_execution_context_t::handle_command_not_found(
    const wcstring &cmd_str, const ast::decorated_statement_t &statement, int err_code) {
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
            ast_args_list_t args = get_argument_nodes(statement.args_or_redirs);
            end_execution_reason_t arg_result =
                this->expand_arguments_from_nodes(args, &event_args, failglob);

            if (arg_result != end_execution_reason_t::ok) {
                return arg_result;
            }

            event_args.insert(event_args.begin(), cmd_str);
        }

        wcstring buffer;
        wcstring error;

        // Redirect to stderr
        auto io = io_chain_t{};
        io.append_from_specs({redirection_spec_t{STDOUT_FILENO, redirection_mode_t::fd, L"2"}}, L"");

        if (function_exists(L"fish_command_not_found", *parser)) {
            buffer = L"fish_command_not_found";
            for (const wcstring &arg : event_args) {
                buffer.push_back(L' ');
                buffer.append(escape_string(arg, ESCAPE_ALL));
            }
            auto prev_statuses = parser->get_last_statuses();

            event_t event(event_type_t::generic);
            event.desc.str_param1 = L"fish_command_not_found";
            block_t *b = parser->push_block(block_t::event_block(event));
            parser->eval(buffer, io);
            parser->pop_block(b);
            parser->set_last_statuses(std::move(prev_statuses));
        } else {
            // If we have no handler, just print it as a normal error.
            error = _(L"Unknown command:");
            if (!event_args.empty()) {
                error.push_back(L' ');
                error.append(escape_string(event_args[0], ESCAPE_ALL));
            }
        }

        // Here we want to report an error (so it shows a backtrace).
        // If the handler printed text, that's already shown, so error will be empty.
        return this->report_error(STATUS_CMD_UNKNOWN, statement, error.c_str());
    }
}

end_execution_reason_t parse_execution_context_t::expand_command(
    const ast::decorated_statement_t &statement, wcstring *out_cmd,
    wcstring_list_t *out_args) const {
    // Here we're expanding a command, for example $HOME/bin/stuff or $randomthing. The first
    // completion becomes the command itself, everything after becomes arguments. Command
    // substitutions are not supported.
    parse_error_list_t errors;

    // Get the unexpanded command string. We expect to always get it here.
    wcstring unexp_cmd = get_source(statement.command);
    size_t pos_of_command_token = statement.command.range.start;

    // Expand the string to produce completions, and report errors.
    expand_result_t expand_err =
        expand_to_command_and_args(unexp_cmd, ctx, out_cmd, out_args, &errors);
    if (expand_err == expand_result_t::error) {
        // Issue #5812 - the expansions were done on the command token,
        // excluding prefixes such as " " or "if ".
        // This means that the error positions are relative to the beginning
        // of the token; we need to make them relative to the original source.
        parse_error_offset_source_start(&errors, pos_of_command_token);
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
    job_t *job, process_t *proc, const ast::decorated_statement_t &statement) {
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
        if (!has_command && statement.decoration() == statement_decoration_t::none) {
            // Implicit cd requires an empty argument and redirection list.
            if (statement.args_or_redirs.empty()) {
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

        ast_args_list_t arg_nodes = get_argument_nodes(statement.args_or_redirs);
        end_execution_reason_t arg_result =
            this->expand_arguments_from_nodes(arg_nodes, &cmd_args, glob_behavior);
        if (arg_result != end_execution_reason_t::ok) {
            return arg_result;
        }

        // The set of IO redirections that we construct for the process.
        auto reason = this->determine_redirections(statement.args_or_redirs, &redirections);
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
    const ast_args_list_t &argument_nodes, wcstring_list_t *out_arguments,
    globspec_t glob_behavior) {
    // Get all argument nodes underneath the statement. We guess we'll have that many arguments (but
    // may have more or fewer, if there are wildcards involved).
    out_arguments->reserve(out_arguments->size() + argument_nodes.size());
    completion_list_t arg_expanded;
    for (const ast::argument_t *arg_node : argument_nodes) {
        // Expect all arguments to have source.
        assert(arg_node->has_source());
        const wcstring arg_str = get_source(*arg_node);

        // Expand this string.
        parse_error_list_t errors;
        arg_expanded.clear();
        auto expand_ret = expand_string(arg_str, &arg_expanded, expand_flags_t{}, ctx, &errors);
        parse_error_offset_source_start(&errors, arg_node->range.start);
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
                    return report_error(STATUS_UNMATCHED_WILDCARD, *arg_node, WILDCARD_ERR_MSG,
                                        get_source(*arg_node).c_str());
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
    const ast::argument_or_redirection_list_t &list, redirection_spec_list_t *out_redirections) {
    // Get all redirection nodes underneath the statement.
    for (const ast::argument_or_redirection_t &arg_or_redir : list) {
        if (!arg_or_redir.is_redirection()) continue;
        const ast::redirection_t &redir_node = arg_or_redir.redirection();

        maybe_t<pipe_or_redir_t> oper = pipe_or_redir_t::from_string(get_source(redir_node.oper));
        if (!oper || !oper->is_valid()) {
            // TODO: figure out if this can ever happen. If so, improve this error message.
            return report_error(STATUS_INVALID_ARGS, redir_node, _(L"Invalid redirection: %ls"),
                                get_source(redir_node).c_str());
        }

        // PCA: I can't justify this skip_variables flag. It was like this when I got here.
        wcstring target = get_source(redir_node.target);
        bool target_expanded =
            expand_one(target, no_exec() ? expand_flag::skip_variables : expand_flags_t{}, ctx);
        if (!target_expanded || target.empty()) {
            // TODO: Improve this error message.
            return report_error(STATUS_INVALID_ARGS, redir_node,
                                _(L"Invalid redirection target: %ls"), target.c_str());
        }

        // Make a redirection spec from the redirect token.
        assert(oper && oper->is_valid() && "expected to have a valid redirection");
        redirection_spec_t spec{oper->fd, oper->mode, std::move(target)};

        // Validate this spec.
        if (spec.mode == redirection_mode_t::fd && !spec.is_close() && !spec.get_target_as_fd()) {
            const wchar_t *fmt =
                _(L"Requested redirection to '%ls', which is not a valid file descriptor");
            return report_error(STATUS_INVALID_ARGS, redir_node, fmt, spec.target.c_str());
        }
        out_redirections->push_back(std::move(spec));

        if (oper->stderr_merge) {
            // This was a redirect like &> which also modifies stderr.
            // Also redirect stderr to stdout.
            out_redirections->push_back(get_stderr_merge());
        }
    }
    return end_execution_reason_t::ok;
}

end_execution_reason_t parse_execution_context_t::populate_not_process(
    job_t *job, process_t *proc, const ast::not_statement_t &not_statement) {
    auto &flags = job->mut_flags();
    flags.negate = !flags.negate;
    return this->populate_job_process(job, proc, not_statement.contents, not_statement.variables);
}

template <typename Type>
end_execution_reason_t parse_execution_context_t::populate_block_process(
    job_t *job, process_t *proc, const ast::statement_t &statement,
    const Type &specific_statement) {
    using namespace ast;
    // We handle block statements by creating process_type_t::block_node, that will bounce back to
    // us when it's time to execute them.
    UNUSED(job);
    static_assert(Type::AstType == type_t::block_statement ||
                      Type::AstType == type_t::if_statement ||
                      Type::AstType == type_t::switch_statement,
                  "Invalid block process");

    // Get the argument or redirections list.
    // TODO: args_or_redirs should be available without resolving the statement type.
    const argument_or_redirection_list_t *args_or_redirs = nullptr;

    // Upcast to permit dropping the 'template' keyword.
    const node_t &ss = specific_statement;
    switch (Type::AstType) {
        case type_t::block_statement:
            args_or_redirs = &ss.as<block_statement_t>()->args_or_redirs;
            break;
        case type_t::if_statement:
            args_or_redirs = &ss.as<if_statement_t>()->args_or_redirs;
            break;
        case type_t::switch_statement:
            args_or_redirs = &ss.as<switch_statement_t>()->args_or_redirs;
            break;
        default:
            DIE("Unexpected block node type");
    }
    assert(args_or_redirs && "Should have args_or_redirs");

    redirection_spec_list_t redirections;
    auto reason = this->determine_redirections(*args_or_redirs, &redirections);
    if (reason == end_execution_reason_t::ok) {
        proc->type = process_type_t::block_node;
        proc->block_node_source = pstree;
        proc->internal_block_node = &statement;
        proc->set_redirection_specs(std::move(redirections));
    }
    return reason;
}

end_execution_reason_t parse_execution_context_t::apply_variable_assignments(
    process_t *proc, const ast::variable_assignment_list_t &variable_assignment_list,
    const block_t **block) {
    if (variable_assignment_list.empty()) return end_execution_reason_t::ok;
    *block = parser->push_block(block_t::variable_assignment_block());
    for (const ast::variable_assignment_t &variable_assignment : variable_assignment_list) {
        const wcstring &source = get_source(variable_assignment);
        auto equals_pos = variable_assignment_equals_pos(source);
        assert(equals_pos);
        const wcstring variable_name = source.substr(0, *equals_pos);
        const wcstring expression = source.substr(*equals_pos + 1);
        completion_list_t expression_expanded;
        parse_error_list_t errors;
        // TODO this is mostly copied from expand_arguments_from_nodes, maybe extract to function
        auto expand_ret =
            expand_string(expression, &expression_expanded, expand_flags_t{}, ctx, &errors);
        parse_error_offset_source_start(&errors, variable_assignment.range.start + *equals_pos + 1);
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
    job_t *job, process_t *proc, const ast::statement_t &statement,
    const ast::variable_assignment_list_t &variable_assignments) {
    using namespace ast;
    // Get the "specific statement" which is boolean / block / if / switch / decorated.
    const node_t &specific_statement = *statement.contents.contents;

    const block_t *block = nullptr;
    end_execution_reason_t result =
        this->apply_variable_assignments(proc, variable_assignments, &block);
    cleanup_t scope([&]() {
        if (block) parser->pop_block(block);
    });
    if (result != end_execution_reason_t::ok) return result;

    switch (specific_statement.type) {
        case type_t::not_statement: {
            result =
                this->populate_not_process(job, proc, *specific_statement.as<not_statement_t>());
            break;
        }
        case type_t::block_statement:
            result = this->populate_block_process(job, proc, statement,
                                                  *specific_statement.as<block_statement_t>());
            break;
        case type_t::if_statement:
            result = this->populate_block_process(job, proc, statement,
                                                  *specific_statement.as<if_statement_t>());
            break;
        case type_t::switch_statement:
            result = this->populate_block_process(job, proc, statement,
                                                  *specific_statement.as<switch_statement_t>());
            break;
        case type_t::decorated_statement: {
            result = this->populate_plain_process(job, proc,
                                                  *specific_statement.as<decorated_statement_t>());
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
    job_t *j, const ast::job_t &job_node, const block_t *associated_block) {
    UNUSED(associated_block);

    // We are going to construct process_t structures for every statement in the job.
    // Create processes. Each one may fail.
    process_list_t processes;
    processes.emplace_back(new process_t());
    end_execution_reason_t result = this->populate_job_process(
        j, processes.back().get(), job_node.statement, job_node.variables);

    // Construct process_ts for job continuations (pipelines).
    for (const ast::job_continuation_t &jc : job_node.continuation) {
        if (result != end_execution_reason_t::ok) {
            break;
        }
        // Handle the pipe, whose fd may not be the obvious stdout.
        auto parsed_pipe = pipe_or_redir_t::from_string(get_source(jc.pipe));
        assert(parsed_pipe.has_value() && parsed_pipe->is_pipe && "Failed to parse valid pipe");
        if (!parsed_pipe->is_valid()) {
            result = report_error(STATUS_INVALID_ARGS, jc.pipe, ILLEGAL_FD_ERR_MSG,
                                  get_source(jc.pipe).c_str());
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
        result = this->populate_job_process(j, processes.back().get(), jc.statement, jc.variables);
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

/// Decide if a job node should be 'time'd.
/// For historical reasons the 'not' and 'time' prefix are "inside out". That is, it's
/// 'not time cmd'. Note that a time appearing anywhere in the pipeline affects the whole job.
/// `sleep 1 | not time true` will time the whole job!
static bool job_node_wants_timing(const ast::job_t &job_node) {
    // Does our job have the job-level time prefix?
    if (job_node.time) return true;

    // Helper to return true if a node is 'not time ...' or 'not not time...' or...
    auto is_timed_not_statement = [](const ast::statement_t &stat) {
        const auto *ns = stat.contents->try_as<ast::not_statement_t>();
        while (ns) {
            if (ns->time) return true;
            ns = ns->contents.try_as<ast::not_statement_t>();
        }
        return false;
    };

    // Do we have a 'not time ...' anywhere in our pipeline?
    if (is_timed_not_statement(job_node.statement)) return true;
    for (const ast::job_continuation_t &jc : job_node.continuation) {
        if (is_timed_not_statement(jc.statement)) return true;
    }
    return false;
}

end_execution_reason_t parse_execution_context_t::run_1_job(const ast::job_t &job_node,
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
    scoped_push<const ast::job_t *> saved_node(&executing_job_node, &job_node);

    // Profiling support.
    profile_item_t *profile_item = this->parser->create_profile_item();
    const auto start_time = profile_item ? profile_item_t::now() : 0;

    // When we encounter a block construct (e.g. while loop) in the general case, we create a "block
    // process" containing its node. This allows us to handle block-level redirections.
    // However, if there are no redirections, then we can just jump into the block directly, which
    // is significantly faster.
    if (job_is_simple_block(job_node)) {
        bool do_time = job_node.time.has_value();
        // If no-exec has been given, there is nothing to time.
        cleanup_t timer = push_timer(do_time && !no_exec());
        const block_t *block = nullptr;
        end_execution_reason_t result =
            this->apply_variable_assignments(nullptr, job_node.variables, &block);
        cleanup_t scope([&]() {
            if (block) parser->pop_block(block);
        });

        const ast::node_t *specific_statement = job_node.statement.contents.get();
        assert(specific_statement_type_is_redirectable_block(*specific_statement));
        if (result == end_execution_reason_t::ok) {
            switch (specific_statement->type) {
                case ast::type_t::block_statement: {
                    result = this->run_block_statement(
                        *specific_statement->as<ast::block_statement_t>(), associated_block);
                    break;
                }
                case ast::type_t::if_statement: {
                    result = this->run_if_statement(*specific_statement->as<ast::if_statement_t>(),
                                                    associated_block);
                    break;
                }
                case ast::type_t::switch_statement: {
                    result = this->run_switch_statement(
                        *specific_statement->as<ast::switch_statement_t>());
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
            profile_item->duration = profile_item_t::now() - start_time;
            profile_item->level = parser->eval_level;
            profile_item->cmd =
                profiling_cmd_name_for_redirectable_block(*specific_statement, *this->pstree);
            profile_item->skipped = false;
        }

        return result;
    }

    const auto &ld = parser->libdata();

    auto job_control_mode = get_job_control_mode();
    bool wants_job_control =
        (job_control_mode == job_control_t::all) ||
        ((job_control_mode == job_control_t::interactive) && parser->is_interactive()) ||
        (ctx.job_group && ctx.job_group->wants_job_control());

    job_t::properties_t props{};
    props.initial_background = job_node.bg.has_value();
    props.skip_notification =
        ld.is_subshell || ld.is_block || ld.is_event || !parser->is_interactive();
    props.from_event_handler = ld.is_event;
    props.job_control = wants_job_control;
    props.wants_timing = job_node_wants_timing(job_node);

    // It's an error to have 'time' in a background job.
    if (props.wants_timing && props.initial_background) {
        return this->report_error(STATUS_INVALID_ARGS, job_node, ERROR_TIME_BACKGROUND);
    }

    shared_ptr<job_t> job = std::make_shared<job_t>(props, get_source(job_node));

    // We are about to populate a job. One possible argument to the job is a command substitution
    // which may be interested in the job that's populating it, via '--on-job-exit caller'. Record
    // the job ID here.
    scoped_push<internal_job_id_t> caller_id(&parser->libdata().caller_id, job->internal_job_id);

    // Populate the job. This may fail for reasons like command_not_found. If this fails, an error
    // will have been printed.
    end_execution_reason_t pop_result =
        this->populate_job_from_job_node(job.get(), job_node, associated_block);
    caller_id.restore();

    // Clean up the job on failure or cancellation.
    if (pop_result == end_execution_reason_t::ok) {
        // Resolve the job's group and mark if this job is the first to get it.
        job->group = job_group_t::resolve_group_for_job(*job, cancel_group, ctx.job_group);
        assert(job->group && "Should not have a null group");
        job->mut_flags().is_group_root = (job->group != ctx.job_group);

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
        if (!exec_job(*this->parser, job, block_io)) {
            remove_job(*this->parser, job.get());
        }

        // Update universal variables on external conmmands.
        // TODO: justify this, why not on every command?
        if (job_contained_external_command) {
            parser->vars().universal_barrier();
        }
    }

    if (profile_item != nullptr) {
        profile_item->duration = profile_item_t::now() - start_time;
        profile_item->level = parser->eval_level;
        profile_item->cmd = job ? job->command() : wcstring();
        profile_item->skipped = (pop_result != end_execution_reason_t::ok);
    }

    job_reap(*parser, false);  // clean up jobs
    return pop_result;
}

end_execution_reason_t parse_execution_context_t::run_job_conjunction(
    const ast::job_conjunction_t &job_expr, const block_t *associated_block) {
    if (auto reason = check_end_execution()) {
        return *reason;
    }
    end_execution_reason_t result = run_1_job(job_expr.job, associated_block);

    for (const ast::job_conjunction_continuation_t &jc : job_expr.continuations) {
        if (result != end_execution_reason_t::ok) {
            return result;
        }
        if (auto reason = check_end_execution()) {
            return *reason;
        }
        // Check the conjunction type.
        bool skip = false;
        switch (jc.conjunction.type) {
            case parse_token_type_t::andand:
                // AND. Skip if the last job failed.
                skip = parser->get_last_status() != 0;
                break;
            case parse_token_type_t::oror:
                // OR. Skip if the last job succeeded.
                skip = parser->get_last_status() == 0;
                break;
            default:
                DIE("Unexpected job conjunction type");
        }
        if (!skip) {
            result = run_1_job(jc.job, associated_block);
        }
    }
    return result;
}

end_execution_reason_t parse_execution_context_t::test_and_run_1_job_conjunction(
    const ast::job_conjunction_t &jc, const block_t *associated_block) {
    // Test this job conjunction if it has an 'and' or 'or' decorator.
    // If it passes, then run it.
    if (auto reason = check_end_execution()) {
        return *reason;
    }
    // Maybe skip the job if it has a leading and/or.
    bool skip = false;
    if (jc.decorator.has_value()) {
        switch (jc.decorator->kw) {
            case parse_keyword_t::kw_and:
                // AND. Skip if the last job failed.
                skip = parser->get_last_status() != 0;
                break;
            case parse_keyword_t::kw_or:
                // OR. Skip if the last job succeeded.
                skip = parser->get_last_status() == 0;
                break;
            default:
                DIE("Unexpected keyword");
        }
    }
    // Skipping is treated as success.
    if (skip) {
        return end_execution_reason_t::ok;
    } else {
        return this->run_job_conjunction(jc, associated_block);
    }
}

end_execution_reason_t parse_execution_context_t::run_job_list(const ast::job_list_t &job_list_node,
                                                               const block_t *associated_block) {
    auto result = end_execution_reason_t::ok;
    for (const ast::job_conjunction_t &jc : job_list_node) {
        result = test_and_run_1_job_conjunction(jc, associated_block);
    }
    // Returns the result of the last job executed or skipped.
    return result;
}

end_execution_reason_t parse_execution_context_t::run_job_list(
    const ast::andor_job_list_t &job_list_node, const block_t *associated_block) {
    auto result = end_execution_reason_t::ok;
    for (const ast::andor_job_t &aoj : job_list_node) {
        result = test_and_run_1_job_conjunction(aoj.job, associated_block);
    }
    // Returns the result of the last job executed or skipped.
    return result;
}

end_execution_reason_t parse_execution_context_t::eval_node(const ast::statement_t &statement,
                                                            const block_t *associated_block) {
    // Note we only expect block-style statements here. No not statements.
    enum end_execution_reason_t status = end_execution_reason_t::ok;
    const ast::node_t *contents = statement.contents.get();
    if (const auto *block = contents->try_as<ast::block_statement_t>()) {
        status = this->run_block_statement(*block, associated_block);
    } else if (const auto *ifstat = contents->try_as<ast::if_statement_t>()) {
        status = this->run_if_statement(*ifstat, associated_block);
    } else if (const auto *switchstat = contents->try_as<ast::switch_statement_t>()) {
        status = this->run_switch_statement(*switchstat);
    } else {
        FLOGF(error, L"Unexpected node %ls found in %s", statement.describe().c_str(),
              __FUNCTION__);
        abort();
    }
    return status;
}

end_execution_reason_t parse_execution_context_t::eval_node(const ast::job_list_t &job_list,
                                                            const block_t *associated_block) {
    assert(associated_block && "Null block");

    // Check for infinite recursion: a function which immediately calls itself..
    wcstring func_name;
    if (const auto *infinite_recursive_node =
            this->infinite_recursive_statement_in_job_list(job_list, &func_name)) {
        // We have an infinite recursion.
        return this->report_error(STATUS_CMD_ERROR, *infinite_recursive_node,
                                  INFINITE_FUNC_RECURSION_ERR_MSG, func_name.c_str());
    }

    // Check for stack overflow. The TOP check ensures we only do this for function calls.
    if (associated_block->type() == block_type_t::top && parser->function_stack_is_overflowing()) {
        return this->report_error(STATUS_CMD_ERROR, job_list, CALL_STACK_LIMIT_EXCEEDED_ERR_MSG);
    }
    return this->run_job_list(job_list, associated_block);
}

int parse_execution_context_t::line_offset_of_node(const ast::job_t *node) {
    // If we're not executing anything, return -1.
    if (!node) {
        return -1;
    }

    // If for some reason we're executing a node without source, return -1.
    auto range = node->try_source_range();
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
        if (auto range = executing_job_node->try_source_range()) {
            result = static_cast<int>(range->start);
        }
    }
    return result;
}
