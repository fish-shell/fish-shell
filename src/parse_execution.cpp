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
#include <wchar.h>
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
#include "parse_tree.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "tokenizer.h"
#include "util.h"
#include "wildcard.h"
#include "wutil.h"

namespace g = grammar;

/// These are the specific statement types that support redirections.
static bool specific_statement_type_is_redirectable_block(const parse_node_t &node) {
    return node.type == symbol_block_statement || node.type == symbol_if_statement ||
           node.type == symbol_switch_statement;
}

/// Get the name of a redirectable block, for profiling purposes.
static wcstring profiling_cmd_name_for_redirectable_block(const parse_node_t &node,
                                                          const parse_node_tree_t &tree,
                                                          const wcstring &src) {
    assert(specific_statement_type_is_redirectable_block(node));
    assert(node.has_source());

    // Get the source for the block, and cut it at the next statement terminator.
    const size_t src_start = node.source_start;
    size_t src_len = node.source_length;

    const parse_node_tree_t::parse_node_list_t statement_terminator_nodes =
        tree.find_nodes(node, parse_token_type_end, 1);
    if (!statement_terminator_nodes.empty()) {
        const parse_node_t *term = statement_terminator_nodes.at(0);
        assert(term->source_start >= src_start);
        src_len = term->source_start - src_start;
    }

    wcstring result = wcstring(src, src_start, src_len);
    result.append(L"...");
    return result;
}

parse_execution_context_t::parse_execution_context_t(parsed_source_ref_t pstree, parser_t *p,
                                                     int initial_eval_level)
    : pstree(std::move(pstree)), parser(p), eval_level(initial_eval_level) {}

// Utilities

wcstring parse_execution_context_t::get_source(const parse_node_t &node) const {
    return node.get_source(pstree->src);
}

const parse_node_t *parse_execution_context_t::get_child(const parse_node_t &parent,
                                                         node_offset_t which,
                                                         parse_token_type_t expected_type) const {
    return this->tree().get_child(parent, which, expected_type);
}

node_offset_t parse_execution_context_t::get_offset(const parse_node_t &node) const {
    // Get the offset of a node via pointer arithmetic, very hackish.
    const parse_node_t *addr = &node;
    const parse_node_t *base = &this->tree().at(0);
    assert(addr >= base);
    node_offset_t offset = static_cast<node_offset_t>(addr - base);
    assert(offset < this->tree().size());
    assert(&tree().at(offset) == &node);
    return offset;
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
    auto first_job = job_list.next_in_list<g::job>();
    if (!first_job) {
        return {};
    }

    // Here's the statement node we find that's infinite recursive.
    tnode_t<grammar::plain_statement> infinite_recursive_statement;

    // Get the list of statements.
    const parse_node_tree_t::parse_node_list_t statements =
        tree().specific_statements_for_job(*first_job);

    // Find all the decorated statements. We are interested in statements with no decoration (i.e.
    // not command, not builtin) whose command expands to the forbidden function.
    for (size_t i = 0; i < statements.size(); i++) {
        // We only care about decorated statements, not while statements, etc.
        const parse_node_t &statement = *statements.at(i);
        if (statement.type != symbol_decorated_statement) {
            continue;
        }
        tnode_t<grammar::decorated_statement> dec_statement(&tree(), &statement);

        auto plain_statement = tree().find_child<grammar::plain_statement>(dec_statement);
        if (get_decoration(plain_statement) != parse_statement_decoration_none) {
            // This statement has a decoration like 'builtin' or 'command', and therefore is not
            // infinite recursion. In particular this is what enables 'wrapper functions'.
            continue;
        }

        // Ok, this is an undecorated plain statement. Get and expand its command.
        maybe_t<wcstring> cmd = command_for_plain_statement(plain_statement, pstree->src);
        if (cmd && expand_one(*cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES, NULL) &&
            cmd == forbidden_function_name) {
            // This is it.
            infinite_recursive_statement = plain_statement;
            if (out_func_name != NULL) {
                *out_func_name = forbidden_function_name;
            }
            break;
        }
    }
    return infinite_recursive_statement;
}

enum process_type_t parse_execution_context_t::process_type_for_command(
    tnode_t<grammar::plain_statement> statement, const wcstring &cmd) const {
    enum process_type_t process_type = EXTERNAL;

    // Determine the process type, which depends on the statement decoration (command, builtin,
    // etc).
    enum parse_statement_decoration_t decoration = get_decoration(statement);

    if (decoration == parse_statement_decoration_exec) {
        // Always exec.
        process_type = INTERNAL_EXEC;
    } else if (decoration == parse_statement_decoration_command) {
        // Always a command.
        process_type = EXTERNAL;
    } else if (decoration == parse_statement_decoration_builtin) {
        // What happens if this builtin is not valid?
        process_type = INTERNAL_BUILTIN;
    } else if (function_exists(cmd)) {
        process_type = INTERNAL_FUNCTION;
    } else if (builtin_exists(cmd)) {
        process_type = INTERNAL_BUILTIN;
    } else {
        process_type = EXTERNAL;
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
    // Must have one statement.
    tnode_t<g::statement> statement = job_node.child<0>();
    const parse_node_t &specific_statement = *get_child(statement, 0);
    if (!specific_statement_type_is_redirectable_block(specific_statement)) {
        // Not an appropriate block type.
        return false;
    }

    // Must be no pipes.
    if (job_node.child<1>().try_get_child<g::tok_pipe, 0>()) {
        return false;
    }

    // Check for arguments and redirections. All of the above types have an arguments / redirections
    // list. It must be empty.
    const parse_node_t &args_and_redirections =
        tree().find_child(specific_statement, symbol_arguments_or_redirections_list);
    if (args_and_redirections.child_count > 0) {
        // Non-empty, we have an argument or redirection.
        return false;
    }

    // Ok, we are a simple block!
    return true;
}

parse_execution_result_t parse_execution_context_t::run_if_statement(
    tnode_t<g::if_statement> statement) {
    // Push an if block.
    if_block_t *ib = parser->push_block<if_block_t>();
    ib->node_offset = this->get_offset(*statement);

    parse_execution_result_t result = parse_execution_success;

    // We have a sequence of if clauses, with a final else, resulting in a single job list that we
    // execute.
    tnode_t<g::job_list> job_list_to_execute;
    tnode_t<g::if_clause> if_clause = statement.child<0>();
    tnode_t<g::else_clause> else_clause = statement.child<1>();
    for (;;) {
        if (should_cancel_execution(ib)) {
            result = parse_execution_cancelled;
            break;
        }

        // An if condition has a job and a "tail" of andor jobs, e.g. "foo ; and bar; or baz".
        tnode_t<g::job> condition_head = if_clause.child<1>();
        tnode_t<g::andor_job_list> condition_boolean_tail = if_clause.child<3>();

        // Check the condition and the tail. We treat parse_execution_errored here as failure, in
        // accordance with historic behavior.
        parse_execution_result_t cond_ret = run_1_job(condition_head, ib);
        if (cond_ret == parse_execution_success) {
            cond_ret = run_job_list(condition_boolean_tail, ib);
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
            proc_set_last_status(STATUS_CMD_OK);
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
    if (job_list_to_execute != NULL) {
        run_job_list(*job_list_to_execute, ib);
    } else {
        // No job list means no sucessful conditions, so return 0 (issue #1443).
        proc_set_last_status(STATUS_CMD_OK);
    }

    // It's possible there's a last-minute cancellation (issue #1297).
    if (should_cancel_execution(ib)) {
        result = parse_execution_cancelled;
    }

    // Done
    parser->pop_block(ib);

    // Otherwise, take the exit status of the job list. Reversal of issue #1061.
    return result;
}

parse_execution_result_t parse_execution_context_t::run_begin_statement(
    tnode_t<g::begin_header> header, tnode_t<g::job_list> contents) {
    // Basic begin/end block. Push a scope block, run jobs, pop it
    scope_block_t *sb = parser->push_block<scope_block_t>(BEGIN);
    parse_execution_result_t ret = run_job_list(contents, sb);
    parser->pop_block(sb);

    return ret;
}

// Define a function.
parse_execution_result_t parse_execution_context_t::run_function_statement(
    tnode_t<g::function_header> header, tnode_t<g::end_command> block_end_command) {
    // Get arguments.
    wcstring_list_t arguments;
    argument_node_list_t arg_nodes = header.descendants<g::argument>();
    parse_execution_result_t result =
        this->expand_arguments_from_nodes(arg_nodes, &arguments, failglob);

    if (result != parse_execution_success) {
        return result;
    }

    // The function definition extends from the end of the header to the function end. It's not
    // just the range of the contents because that loses comments - see issue #1710.
    assert(block_end_command.has_source());
    auto header_range = header.source_range();
    size_t contents_start = header_range->start + header_range->length;
    size_t contents_end = block_end_command.source_range()
                              ->start;  // 1 past the last character in the function definition
    assert(contents_end >= contents_start);

    // Swallow whitespace at both ends.
    while (contents_start < contents_end && iswspace(pstree->src.at(contents_start))) {
        contents_start++;
    }
    while (contents_start < contents_end && iswspace(pstree->src.at(contents_end - 1))) {
        contents_end--;
    }

    assert(contents_end >= contents_start);
    const wcstring contents_str =
        wcstring(pstree->src, contents_start, contents_end - contents_start);
    int definition_line_offset = this->line_offset_of_character_at_offset(contents_start);
    io_streams_t streams(0);  // no limit on the amount of output from builtin_function()
    int err = builtin_function(*parser, streams, arguments, contents_str, definition_line_offset);
    proc_set_last_status(err);

    if (!streams.err.empty()) {
        this->report_error(header, L"%ls", streams.err.buffer().c_str());
        result = parse_execution_errored;
    }

    return result;
}

parse_execution_result_t parse_execution_context_t::run_block_statement(
    tnode_t<g::block_statement> statement) {
    tnode_t<g::block_header> bheader = statement.child<0>();
    tnode_t<g::job_list> contents = statement.child<1>();

    parse_execution_result_t ret = parse_execution_success;
    if (auto header = bheader.try_get_child<g::for_header, 0>()) {
        ret = run_for_statement(header, contents);
    } else if (auto header = bheader.try_get_child<g::while_header, 0>()) {
        ret = run_while_statement(header, contents);
    } else if (auto header = bheader.try_get_child<g::function_header, 0>()) {
        tnode_t<g::end_command> func_end = statement.child<2>();
        ret = run_function_statement(header, func_end);
    } else if (auto header = bheader.try_get_child<g::begin_header, 0>()) {
        ret = run_begin_statement(header, contents);
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
    if (!expand_one(for_var_name, 0, NULL)) {
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

    auto var = env_get(for_var_name, ENV_LOCAL);
    if (!var && !is_function_context()) var = env_get(for_var_name, ENV_DEFAULT);
    if (!var || var->read_only()) {
        int retval = env_set_empty(for_var_name, ENV_LOCAL | ENV_USER);
        if (retval != ENV_OK) {
            report_error(var_name_node, L"You cannot use read-only variable '%ls' in a for loop",
                         for_var_name.c_str());
            return parse_execution_errored;
        }
    }

    for_block_t *fb = parser->push_block<for_block_t>();

    // Now drive the for loop.
    for (const wcstring &val : arguments) {
        if (should_cancel_execution(fb)) {
            ret = parse_execution_cancelled;
            break;
        }

        int retval = env_set_one(for_var_name, ENV_DEFAULT | ENV_USER, val);
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
    int expand_ret =
        expand_string(switch_value, &switch_values_expanded, EXPAND_NO_DESCRIPTIONS, &errors);
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
    tnode_t<grammar::while_header> header, tnode_t<grammar::job_list> contents) {
    // Push a while block.
    while_block_t *wb = parser->push_block<while_block_t>();
    wb->node_offset = this->get_offset(header);

    parse_execution_result_t ret = parse_execution_success;

    // The conditions of the while loop.
    tnode_t<g::job> condition_head = header.child<1>();
    tnode_t<g::andor_job_list> condition_boolean_tail = header.child<3>();

    // Run while the condition is true.
    for (;;) {
        // Check the condition.
        parse_execution_result_t cond_ret = this->run_1_job(condition_head, wb);
        if (cond_ret == parse_execution_success) {
            cond_ret = run_job_list(condition_boolean_tail, wb);
        }

        // We only continue on successful execution and EXIT_SUCCESS.
        if (cond_ret != parse_execution_success || proc_get_last_status() != EXIT_SUCCESS) {
            break;
        }

        // Check cancellation.
        if (this->should_cancel_execution(wb)) {
            ret = parse_execution_cancelled;
            break;
        }

        // The block ought to go inside the loop (see issue #1212).
        this->run_job_list(contents, wb);

        if (this->cancellation_reason(wb) == execution_cancellation_loop_control) {
            // Handle break or continue.
            if (wb->loop_status == LOOP_CONTINUE) {
                // Reset the loop state.
                wb->loop_status = LOOP_NORMAL;
                continue;
            } else if (wb->loop_status == LOOP_BREAK) {
                break;
            }
        }

        // no_exec means that fish was invoked with -n or --no-execute. If set, we allow the loop to
        // not-execute once so its contents can be checked, and then break.
        if (no_exec) {
            break;
        }
    }

    // Done
    parser->pop_block(wb);
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
            fwprintf(stderr, L"%ls", backtrace_and_desc.c_str());
        }
    }
    return parse_execution_errored;
}

/// Reports an unmatched wildcard error and returns parse_execution_errored.
parse_execution_result_t parse_execution_context_t::report_unmatched_wildcard_error(
    const parse_node_t &unmatched_wildcard) {
    proc_set_last_status(STATUS_UNMATCHED_WILDCARD);
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
    const wchar_t *const equals_ptr = wcschr(cmd, L'=');
    if (equals_ptr != NULL) {
        // Try to figure out if this is a pure variable assignment (foo=bar), or if this appears to
        // be running a command (foo=bar ruby...).
        const wcstring name_str = wcstring(cmd, equals_ptr - cmd);  // variable name, up to the =
        const wcstring val_str = wcstring(equals_ptr + 1);          // variable value, past the =

        auto args = statement.descendants<g::argument>(1);
        if (!args.empty()) {
            const wcstring argument = get_source(args.at(0));

            wcstring ellipsis_str = wcstring(1, ellipsis_char);
            if (ellipsis_str == L"$") ellipsis_str = L"...";

            // Looks like a command.
            this->report_error(statement, ERROR_BAD_EQUALS_IN_COMMAND5, argument.c_str(),
                               name_str.c_str(), val_str.c_str(), argument.c_str(),
                               ellipsis_str.c_str());
        } else {
            wcstring assigned_val = reconstruct_orig_str(val_str);
            this->report_error(statement, ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, name_str.c_str(),
                               assigned_val.c_str());
        }
    } else if (wcschr(cmd, L'$') || wcschr(cmd, VARIABLE_EXPAND_SINGLE) ||
               wcschr(cmd, VARIABLE_EXPAND)) {
        const wchar_t *msg =
            _(L"Variables may not be used as commands. In fish, "
              L"please define a function or use 'eval %ls'.");
        wcstring eval_cmd = reconstruct_orig_str(cmd_str);
        this->report_error(statement, msg, eval_cmd.c_str());
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
    proc_set_last_status(err_code == ENOENT ? STATUS_CMD_UNKNOWN : STATUS_NOT_EXECUTABLE);

    return parse_execution_errored;
}

/// Creates a 'normal' (non-block) process.
parse_execution_result_t parse_execution_context_t::populate_plain_process(
    job_t *job, process_t *proc, tnode_t<grammar::plain_statement> statement) {
    assert(job != NULL);
    assert(proc != NULL);

    // We may decide that a command should be an implicit cd.
    bool use_implicit_cd = false;

    // Get the command. We expect to always get it here.
    wcstring cmd = *command_for_plain_statement(statement, pstree->src);

    // Expand it as a command. Return an error on failure.
    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES, NULL);
    if (!expanded) {
        report_error(statement, ILLEGAL_CMD_ERR_MSG, cmd.c_str());
        proc_set_last_status(STATUS_ILLEGAL_CMD);
        return parse_execution_errored;
    }

    // Determine the process type.
    enum process_type_t process_type = process_type_for_command(statement, cmd);

    // Check for stack overflow.
    if (process_type == INTERNAL_FUNCTION &&
        parser->forbidden_function.size() > FISH_MAX_STACK_DEPTH) {
        this->report_error(statement, CALL_STACK_LIMIT_EXCEEDED_ERR_MSG);
        return parse_execution_errored;
    }

    wcstring path_to_external_command;
    if (process_type == EXTERNAL || process_type == INTERNAL_EXEC) {
        // Determine the actual command. This may be an implicit cd.
        bool has_command = path_get_path(cmd, &path_to_external_command);

        // If there was no command, then we care about the value of errno after checking for it, to
        // distinguish between e.g. no file vs permissions problem.
        const int no_cmd_err_code = errno;

        // If the specified command does not exist, and is undecorated, try using an implicit cd.
        if (!has_command && get_decoration(statement) == parse_statement_decoration_none) {
            // Implicit cd requires an empty argument and redirection list.
            tnode_t<g::arguments_or_redirections_list> args = statement.child<1>();
            if (!args.try_get_child<g::argument_or_redirection, 0>()) {
                // Ok, no arguments or redirections; check to see if the first argument is a
                // directory.
                wcstring implicit_cd_path;
                use_implicit_cd = path_can_be_implicit_cd(cmd, &implicit_cd_path);
            }
        }

        if (!has_command && !use_implicit_cd) {
            // No command.
            return this->handle_command_not_found(cmd, statement, no_cmd_err_code);
        }
    }

    // The argument list and set of IO redirections that we will construct for the process.
    io_chain_t process_io_chain;
    wcstring_list_t argument_list;
    if (use_implicit_cd) {
        /* Implicit cd is simple */
        argument_list.push_back(L"cd");
        argument_list.push_back(cmd);
        path_to_external_command.clear();

        // If we have defined a wrapper around cd, use it, otherwise use the cd builtin.
        process_type = function_exists(L"cd") ? INTERNAL_FUNCTION : INTERNAL_BUILTIN;
    } else {
        const globspec_t glob_behavior = (cmd == L"set" || cmd == L"count") ? nullglob : failglob;
        // Form the list of arguments. The command is the first argument.
        argument_node_list_t arg_nodes = statement.descendants<g::argument>();
        parse_execution_result_t arg_result =
            this->expand_arguments_from_nodes(arg_nodes, &argument_list, glob_behavior);
        if (arg_result != parse_execution_success) {
            return arg_result;
        }
        argument_list.insert(argument_list.begin(), cmd);

        // The set of IO redirections that we construct for the process.
        if (!this->determine_io_chain(statement.child<1>(), &process_io_chain)) {
            return parse_execution_errored;
        }

        // Determine the process type.
        process_type = process_type_for_command(statement, cmd);
    }

    // Populate the process.
    proc->type = process_type;
    proc->set_argv(argument_list);
    proc->set_io_chain(process_io_chain);
    proc->actual_cmd = path_to_external_command;
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
        int expand_ret = expand_string(arg_str, &arg_expanded, EXPAND_NO_DESCRIPTIONS, &errors);
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
    auto redirect_nodes = node.descendants<g::redirection>();
    for (tnode_t<g::redirection> redirect_node : redirect_nodes) {
        int source_fd = -1;  // source fd
        wcstring target;     // file path or target fd
        enum token_type redirect_type =
            redirection_type(redirect_node, pstree->src, &source_fd, &target);

        // PCA: I can't justify this EXPAND_SKIP_VARIABLES flag. It was like this when I got here.
        bool target_expanded = expand_one(target, no_exec ? EXPAND_SKIP_VARIABLES : 0, NULL);
        if (!target_expanded || target.empty()) {
            // TODO: Improve this error message.
            errored =
                report_error(redirect_node, _(L"Invalid redirection target: %ls"), target.c_str());
        }

        // Generate the actual IO redirection.
        shared_ptr<io_data_t> new_io;
        assert(redirect_type != TOK_NONE);
        switch (redirect_type) {
            case TOK_REDIRECT_FD: {
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
            case TOK_REDIRECT_OUT:
            case TOK_REDIRECT_APPEND:
            case TOK_REDIRECT_IN:
            case TOK_REDIRECT_NOCLOB: {
                int oflags = oflags_for_redirection_type(redirect_type);
                io_file_t *new_io_file = new io_file_t(source_fd, target, oflags);
                new_io.reset(new_io_file);
                break;
            }
            default: {
                // Should be unreachable.
                debug(0, "Unexpected redirection type %ld.", (long)redirect_type);
                PARSER_DIE();
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

parse_execution_result_t parse_execution_context_t::populate_boolean_process(
    job_t *job, process_t *proc, tnode_t<g::boolean_statement> bool_statement) {
    // Handle a boolean statement.
    bool skip_job = false;
    switch (bool_statement_type(bool_statement)) {
        case parse_bool_and: {
            // AND. Skip if the last job failed.
            skip_job = (proc_get_last_status() != 0);
            break;
        }
        case parse_bool_or: {
            // OR. Skip if the last job succeeded.
            skip_job = (proc_get_last_status() == 0);
            break;
        }
        case parse_bool_not: {
            // NOT. Negate it.
            job->set_flag(JOB_NEGATE, !job->get_flag(JOB_NEGATE));
            break;
        }
    }

    if (skip_job) {
        return parse_execution_skipped;
    }
    return this->populate_job_process(job, proc,
                                      bool_statement.require_get_child<g::statement, 1>());
}

template <typename Type>
parse_execution_result_t parse_execution_context_t::populate_block_process(job_t *job,
                                                                           process_t *proc,
                                                                           tnode_t<Type> node) {
    // We handle block statements by creating INTERNAL_BLOCK_NODE, that will bounce back to us when
    // it's time to execute them.
    UNUSED(job);
    static_assert(Type::token == symbol_block_statement || Type::token == symbol_if_statement ||
                      Type::token == symbol_switch_statement,
                  "Invalid block process");

    // The set of IO redirections that we construct for the process.
    // TODO: fix this ugly find_child.
    auto arguments = node.template find_child<g::arguments_or_redirections_list>();
    io_chain_t process_io_chain;
    bool errored = !this->determine_io_chain(arguments, &process_io_chain);
    if (errored) return parse_execution_errored;

    proc->type = INTERNAL_BLOCK_NODE;
    proc->internal_block_node = this->get_offset(node);
    proc->set_io_chain(process_io_chain);
    return parse_execution_success;
}

parse_execution_result_t parse_execution_context_t::populate_job_process(
    job_t *job, process_t *proc, tnode_t<grammar::statement> statement) {
    // Get the "specific statement" which is boolean / block / if / switch / decorated.
    const parse_node_t &specific_statement = *get_child(statement, 0);

    parse_execution_result_t result = parse_execution_success;

    switch (specific_statement.type) {
        case symbol_boolean_statement: {
            result = this->populate_boolean_process(job, proc, {&tree(), &specific_statement});
            break;
        }
        case symbol_block_statement:
            result = this->populate_block_process(
                job, proc, tnode_t<g::block_statement>(&tree(), &specific_statement));
            break;
        case symbol_if_statement:
            result = this->populate_block_process(
                job, proc, tnode_t<g::if_statement>(&tree(), &specific_statement));
            break;
        case symbol_switch_statement:
            result = this->populate_block_process(
                job, proc, tnode_t<g::switch_statement>(&tree(), &specific_statement));
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
        tnode_t<g::statement> statement = job_cont.require_get_child<g::statement, 1>();

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
        job_cont = job_cont.require_get_child<g::job_continuation, 2>();
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
    scoped_push<int> saved_eval_level(&eval_level, eval_level + 1);

    // Save the node index.
    scoped_push<node_offset_t> saved_node_offset(&executing_node_idx, this->get_offset(job_node));

    // Profiling support.
    long long start_time = 0, parse_time = 0, exec_time = 0;
    profile_item_t *profile_item = this->parser->create_profile_item();
    if (profile_item != NULL) {
        start_time = get_time();
    }

    // When we encounter a block construct (e.g. while loop) in the general case, we create a "block
    // process" that has a pointer to its source. This allows us to handle block-level redirections.
    // However, if there are no redirections, then we can just jump into the block directly, which
    // is significantly faster.
    if (job_is_simple_block(job_node)) {
        parse_execution_result_t result = parse_execution_success;

        tnode_t<g::statement> statement = job_node.child<0>();
        const parse_node_t &specific_statement = *get_child(statement, 0);
        assert(specific_statement_type_is_redirectable_block(specific_statement));
        switch (specific_statement.type) {
            case symbol_block_statement: {
                result = this->run_block_statement({&tree(), &specific_statement});
                break;
            }
            case symbol_if_statement: {
                result = this->run_if_statement({&tree(), &specific_statement});
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
            profile_item->level = eval_level;
            profile_item->parse = 0;
            profile_item->exec = (int)(exec_time - start_time);
            profile_item->cmd = profiling_cmd_name_for_redirectable_block(
                specific_statement, this->tree(), this->pstree->src);
            profile_item->skipped = false;
        }

        return result;
    }

    shared_ptr<job_t> job = std::make_shared<job_t>(acquire_job_id(), block_io);
    job->tmodes = tmodes;
    job->set_flag(JOB_CONTROL,
                  (job_control_mode == JOB_CONTROL_ALL) ||
                      ((job_control_mode == JOB_CONTROL_INTERACTIVE) && shell_is_interactive()));

    job->set_flag(JOB_FOREGROUND, !tree().job_should_be_backgrounded(job_node));

    job->set_flag(JOB_TERMINAL, job->get_flag(JOB_CONTROL) && !is_event);

    job->set_flag(JOB_SKIP_NOTIFICATION,
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
            if (proc->type == EXTERNAL) {
                job_contained_external_command = true;
                break;
            }
        }

        // Actually execute the job.
        exec_job(*this->parser, job.get());

        // Only external commands require a new fishd barrier.
        if (job_contained_external_command) {
            set_proc_had_barrier(false);
        }
    }

    if (profile_item != NULL) {
        exec_time = get_time();
        profile_item->level = eval_level;
        profile_item->parse = (int)(parse_time - start_time);
        profile_item->exec = (int)(exec_time - parse_time);
        profile_item->cmd = job ? job->command() : wcstring();
        profile_item->skipped = !populated_job;
    }

    job_reap(0);  // clean up jobs
    return parse_execution_success;
}

parse_execution_result_t parse_execution_context_t::run_job_list(const parse_node_t &job_list_node,
                                                                 const block_t *associated_block) {
    assert(job_list_node.type == symbol_job_list || job_list_node.type == symbol_andor_job_list);

    parse_execution_result_t result = parse_execution_success;
    const parse_node_t *job_list = &job_list_node;
    while (job_list != NULL && !should_cancel_execution(associated_block)) {
        assert(job_list->type == symbol_job_list || job_list_node.type == symbol_andor_job_list);

        // Try pulling out a job.
        const parse_node_t *job = tree().next_node_in_node_list(*job_list, symbol_job, &job_list);

        if (job != NULL) {
            result = this->run_1_job({&tree(), job}, associated_block);
        }
    }

    // Returns the last job executed.
    return result;
}

parse_execution_result_t parse_execution_context_t::eval_node_at_offset(
    node_offset_t offset, const block_t *associated_block, const io_chain_t &io) {
    // Don't ever expect to have an empty tree if this is called.
    assert(!tree().empty());  //!OCLINT(multiple unary operator)
    assert(offset < tree().size());

    // Apply this block IO for the duration of this function.
    scoped_push<io_chain_t> block_io_push(&block_io, io);

    const parse_node_t &node = tree().at(offset);

    // Currently, we only expect to execute the top level job list, or a block node. Assert that.
    assert(node.type == symbol_job_list || specific_statement_type_is_redirectable_block(node));

    enum parse_execution_result_t status = parse_execution_success;
    switch (node.type) {
        case symbol_job_list: {
            // We should only get a job list if it's the very first node. This is because this is
            // the entry point for both top-level execution (the first node) and INTERNAL_BLOCK_NODE
            // execution (which does block statements, but never job lists).
            assert(offset == 0);
            tnode_t<g::job_list> job_list{&tree(), &node};
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
                status = this->run_job_list(node, associated_block);
            }
            break;
        }
        case symbol_block_statement: {
            status = this->run_block_statement({&tree(), &node});
            break;
        }
        case symbol_if_statement: {
            status = this->run_if_statement({&tree(), &node});
            break;
        }
        case symbol_switch_statement: {
            status = this->run_switch_statement({&tree(), &node});
            break;
        }
        default: {
            // In principle, we could support other node types. However we never expect to be passed
            // them - see above.
            debug(0, "Unexpected node %ls found in %s", node.describe().c_str(), __FUNCTION__);
            PARSER_DIE();
            break;
        }
    }

    return status;
}

int parse_execution_context_t::line_offset_of_node_at_offset(node_offset_t requested_index) {
    // If we're not executing anything, return -1.
    if (requested_index == NODE_OFFSET_INVALID) {
        return -1;
    }

    // If for some reason we're executing a node without source, return -1.
    const parse_node_t &node = tree().at(requested_index);
    if (!node.has_source()) {
        return -1;
    }

    size_t char_offset = tree().at(requested_index).source_start;
    return this->line_offset_of_character_at_offset(char_offset);
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
        for (i = cached_lineno_offset; str[i] != L'\0' && i < offset; i++) {
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
    int line_offset = this->line_offset_of_node_at_offset(this->executing_node_idx);
    if (line_offset >= 0) {
        // The offset is 0 based; the number is 1 based.
        line_number = line_offset + 1;
    }
    return line_number;
}

int parse_execution_context_t::get_current_source_offset() const {
    int result = -1;
    if (executing_node_idx != NODE_OFFSET_INVALID) {
        const parse_node_t &node = tree().at(executing_node_idx);
        if (node.has_source()) {
            result = static_cast<int>(node.source_start);
        }
    }
    return result;
}
