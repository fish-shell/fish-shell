// Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.)
//
// A note on error handling: fish has two kind of errors, fatal parse errors non-fatal runtime
// errors. A fatal error prevents execution of the entire file, while a non-fatal error skips that
// job.
//
// Non-fatal errors are printed as soon as they are encountered; otherwise you would have to wait
// for the execution to finish to see them.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "event.h"
#include "exec.h"
#include "expand.h"
#include "function.h"
#include "io.h"
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

parse_execution_context_t::parse_execution_context_t(moved_ref<parse_node_tree_t> t,
                                                     const wcstring &s, parser_t *p,
                                                     int initial_eval_level)
    : tree(t),
      src(s),
      parser(p),
      eval_level(initial_eval_level),
      executing_node_idx(NODE_OFFSET_INVALID),
      cached_lineno_offset(0),
      cached_lineno_count(0) {}

// Utilities

wcstring parse_execution_context_t::get_source(const parse_node_t &node) const {
    return node.get_source(this->src);
}

const parse_node_t *parse_execution_context_t::get_child(const parse_node_t &parent,
                                                         node_offset_t which,
                                                         parse_token_type_t expected_type) const {
    return this->tree.get_child(parent, which, expected_type);
}

node_offset_t parse_execution_context_t::get_offset(const parse_node_t &node) const {
    // Get the offset of a node via pointer arithmetic, very hackish.
    const parse_node_t *addr = &node;
    const parse_node_t *base = &this->tree.at(0);
    assert(addr >= base);
    assert(addr - base < SOURCE_OFFSET_INVALID);
    node_offset_t offset = static_cast<node_offset_t>(addr - base);
    assert(offset < this->tree.size());
    assert(&tree.at(offset) == &node);
    return offset;
}

const parse_node_t *parse_execution_context_t::infinite_recursive_statement_in_job_list(
    const parse_node_t &job_list, wcstring *out_func_name) const {
    assert(job_list.type == symbol_job_list);
    // This is a bit fragile. It is a test to see if we are inside of function call, but not inside
    // a block in that function call. If, in the future, the rules for what block scopes are pushed
    // on function invocation changes, then this check will break.
    const block_t *current = parser->block_at_index(0), *parent = parser->block_at_index(1);
    bool is_within_function_call =
        (current && parent && current->type() == TOP && parent->type() == FUNCTION_CALL);
    if (!is_within_function_call) {
        return NULL;
    }

    // Check to see which function call is forbidden.
    if (parser->forbidden_function.empty()) {
        return NULL;
    }
    const wcstring &forbidden_function_name = parser->forbidden_function.back();

    // Get the first job in the job list.
    const parse_node_t *first_job = tree.next_node_in_node_list(job_list, symbol_job, NULL);
    if (first_job == NULL) {
        return NULL;
    }

    // Here's the statement node we find that's infinite recursive.
    const parse_node_t *infinite_recursive_statement = NULL;

    // Get the list of statements.
    const parse_node_tree_t::parse_node_list_t statements =
        tree.specific_statements_for_job(*first_job);

    // Find all the decorated statements. We are interested in statements with no decoration (i.e.
    // not command, not builtin) whose command expands to the forbidden function.
    for (size_t i = 0; i < statements.size(); i++) {
        // We only care about decorated statements, not while statements, etc.
        const parse_node_t &statement = *statements.at(i);
        if (statement.type != symbol_decorated_statement) {
            continue;
        }

        const parse_node_t &plain_statement = tree.find_child(statement, symbol_plain_statement);
        if (tree.decoration_for_plain_statement(plain_statement) !=
            parse_statement_decoration_none) {
            // This statement has a decoration like 'builtin' or 'command', and therefore is not
            // infinite recursion. In particular this is what enables 'wrapper functions'.
            continue;
        }

        // Ok, this is an undecorated plain statement. Get and expand its command.
        wcstring cmd;
        tree.command_for_plain_statement(plain_statement, src, &cmd);

        if (expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES, NULL) &&
            cmd == forbidden_function_name) {
            // This is it.
            infinite_recursive_statement = &statement;
            if (out_func_name != NULL) {
                *out_func_name = forbidden_function_name;
            }
            break;
        }
    }

    assert(infinite_recursive_statement == NULL ||
           infinite_recursive_statement->type == symbol_decorated_statement);
    return infinite_recursive_statement;
}

enum process_type_t parse_execution_context_t::process_type_for_command(
    const parse_node_t &plain_statement, const wcstring &cmd) const {
    assert(plain_statement.type == symbol_plain_statement);
    enum process_type_t process_type = EXTERNAL;

    // Determine the process type, which depends on the statement decoration (command, builtin,
    // etc).
    enum parse_statement_decoration_t decoration =
        tree.decoration_for_plain_statement(plain_statement);

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
    if (block && block->loop_status != LOOP_NORMAL) {
        // Nasty hack - break and continue set the 'skip' flag as well as the loop status flag.
        return execution_cancellation_loop_control;
    }
    if (block && block->skip) {
        return execution_cancellation_skip;
    }
    return execution_cancellation_none;
}

/// Return whether the job contains a single statement, of block type, with no redirections.
bool parse_execution_context_t::job_is_simple_block(const parse_node_t &job_node) const {
    assert(job_node.type == symbol_job);

    // Must have one statement.
    const parse_node_t &statement = *get_child(job_node, 0, symbol_statement);
    const parse_node_t &specific_statement = *get_child(statement, 0);
    if (!specific_statement_type_is_redirectable_block(specific_statement)) {
        // Not an appropriate block type.
        return false;
    }

    // Must be no pipes.
    const parse_node_t &continuation = *get_child(job_node, 1, symbol_job_continuation);
    if (continuation.child_count > 0) {
        // Multiple statements in this job, so there's pipes involved.
        return false;
    }

    // Check for arguments and redirections. All of the above types have an arguments / redirections
    // list. It must be empty.
    const parse_node_t &args_and_redirections =
        tree.find_child(specific_statement, symbol_arguments_or_redirections_list);
    if (args_and_redirections.child_count > 0) {
        // Non-empty, we have an argument or redirection.
        return false;
    }

    // Ok, we are a simple block!
    return true;
}

parse_execution_result_t parse_execution_context_t::run_if_statement(
    const parse_node_t &statement) {
    assert(statement.type == symbol_if_statement);

    // Push an if block.
    if_block_t *ib = new if_block_t();
    ib->node_offset = this->get_offset(statement);
    parser->push_block(ib);

    parse_execution_result_t result = parse_execution_success;

    // We have a sequence of if clauses, with a final else, resulting in a single job list that we
    // execute.
    const parse_node_t *job_list_to_execute = NULL;
    const parse_node_t *if_clause = get_child(statement, 0, symbol_if_clause);
    const parse_node_t *else_clause = get_child(statement, 1, symbol_else_clause);
    for (;;) {
        if (should_cancel_execution(ib)) {
            result = parse_execution_cancelled;
            break;
        }

        // An if condition has a job and a "tail" of andor jobs, e.g. "foo ; and bar; or baz".
        assert(if_clause != NULL && else_clause != NULL);
        const parse_node_t &condition_head = *get_child(*if_clause, 1, symbol_job);
        const parse_node_t &condition_boolean_tail =
            *get_child(*if_clause, 3, symbol_andor_job_list);

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
            job_list_to_execute = get_child(*if_clause, 4, symbol_job_list);
            break;
        } else if (else_clause->child_count == 0) {
            // 'if' condition failed, no else clause, return 0, we're done.
            job_list_to_execute = NULL;
            proc_set_last_status(STATUS_BUILTIN_OK);
            break;
        } else {
            // We have an 'else continuation' (either else-if or else).
            const parse_node_t &else_cont = *get_child(*else_clause, 1, symbol_else_continuation);
            const parse_node_t *maybe_if_clause = get_child(else_cont, 0);
            if (maybe_if_clause && maybe_if_clause->type == symbol_if_clause) {
                // it's an 'else if', go to the next one.
                if_clause = maybe_if_clause;
                else_clause = get_child(else_cont, 1, symbol_else_clause);
            } else {
                // It's the final 'else', we're done.
                job_list_to_execute = get_child(else_cont, 1, symbol_job_list);
                break;
            }
        }
    }

    // Execute any job list we got.
    if (job_list_to_execute != NULL) {
        run_job_list(*job_list_to_execute, ib);
    } else {
        // No job list means no sucessful conditions, so return 0 (issue #1443).
        proc_set_last_status(STATUS_BUILTIN_OK);
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
    const parse_node_t &header, const parse_node_t &contents) {
    assert(header.type == symbol_begin_header);
    assert(contents.type == symbol_job_list);

    // Basic begin/end block. Push a scope block.
    scope_block_t *sb = new scope_block_t(BEGIN);
    parser->push_block(sb);

    // Run the job list.
    parse_execution_result_t ret = run_job_list(contents, sb);

    // Pop the block.
    parser->pop_block(sb);

    return ret;
}

// Define a function.
parse_execution_result_t parse_execution_context_t::run_function_statement(
    const parse_node_t &header, const parse_node_t &block_end_command) {
    assert(header.type == symbol_function_header);
    assert(block_end_command.type == symbol_end_command);

    // Get arguments.
    wcstring_list_t argument_list;
    parse_execution_result_t result = this->determine_arguments(header, &argument_list, failglob);

    if (result == parse_execution_success) {
        // The function definition extends from the end of the header to the function end. It's not
        // just the range of the contents because that loses comments - see issue #1710.
        assert(block_end_command.has_source());
        size_t contents_start = header.source_start + header.source_length;
        size_t contents_end =
            block_end_command.source_start;  // 1 past the last character in the function definition
        assert(contents_end >= contents_start);

        // Swallow whitespace at both ends.
        while (contents_start < contents_end && iswspace(this->src.at(contents_start))) {
            contents_start++;
        }
        while (contents_start < contents_end && iswspace(this->src.at(contents_end - 1))) {
            contents_end--;
        }

        assert(contents_end >= contents_start);
        const wcstring contents_str =
            wcstring(this->src, contents_start, contents_end - contents_start);
        int definition_line_offset = this->line_offset_of_character_at_offset(contents_start);
        wcstring error_str;
        io_streams_t streams;
        int err = builtin_function(*parser, streams, argument_list, contents_str,
                                   definition_line_offset, &error_str);
        proc_set_last_status(err);

        if (!error_str.empty()) {
            this->report_error(header, L"%ls", error_str.c_str());
            result = parse_execution_errored;
        }
    }
    return result;
}

parse_execution_result_t parse_execution_context_t::run_block_statement(
    const parse_node_t &statement) {
    assert(statement.type == symbol_block_statement);

    const parse_node_t &block_header =
        *get_child(statement, 0, symbol_block_header);  // block header
    const parse_node_t &header =
        *get_child(block_header, 0);  // specific header type (e.g. for loop)
    const parse_node_t &contents = *get_child(statement, 1, symbol_job_list);  // block contents

    parse_execution_result_t ret = parse_execution_success;
    switch (header.type) {
        case symbol_for_header: {
            ret = run_for_statement(header, contents);
            break;
        }
        case symbol_while_header: {
            ret = run_while_statement(header, contents);
            break;
        }
        case symbol_function_header: {
            const parse_node_t &function_end = *get_child(
                statement, 2, symbol_end_command);  // the 'end' associated with the block
            ret = run_function_statement(header, function_end);
            break;
        }
        case symbol_begin_header: {
            ret = run_begin_statement(header, contents);
            break;
        }
        default: {
            fprintf(stderr, "Unexpected block header: %ls\n", header.describe().c_str());
            PARSER_DIE();
            break;
        }
    }

    return ret;
}

parse_execution_result_t parse_execution_context_t::run_for_statement(
    const parse_node_t &header, const parse_node_t &block_contents) {
    assert(header.type == symbol_for_header);
    assert(block_contents.type == symbol_job_list);

    // Get the variable name: `for var_name in ...`. We expand the variable name. It better result
    // in just one.
    const parse_node_t &var_name_node = *get_child(header, 1, parse_token_type_string);
    wcstring for_var_name = get_source(var_name_node);
    if (!expand_one(for_var_name, 0, NULL)) {
        report_error(var_name_node, FAILED_EXPANSION_VARIABLE_NAME_ERR_MSG, for_var_name.c_str());
        return parse_execution_errored;
    }

    // Get the contents to iterate over.
    wcstring_list_t argument_sequence;
    parse_execution_result_t ret = this->determine_arguments(header, &argument_sequence, nullglob);
    if (ret != parse_execution_success) {
        return ret;
    }

    for_block_t *fb = new for_block_t();
    parser->push_block(fb);

    // Now drive the for loop.
    const size_t arg_count = argument_sequence.size();
    for (size_t i = 0; i < arg_count; i++) {
        if (should_cancel_execution(fb)) {
            ret = parse_execution_cancelled;
            break;
        }

        const wcstring &val = argument_sequence.at(i);
        env_set(for_var_name, val.c_str(), ENV_LOCAL);
        fb->loop_status = LOOP_NORMAL;
        fb->skip = 0;

        this->run_job_list(block_contents, fb);

        if (this->cancellation_reason(fb) == execution_cancellation_loop_control) {
            // Handle break or continue.
            if (fb->loop_status == LOOP_CONTINUE) {
                // Reset the loop state.
                fb->loop_status = LOOP_NORMAL;
                fb->skip = false;
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
    const parse_node_t &statement) {
    assert(statement.type == symbol_switch_statement);

    parse_execution_result_t result = parse_execution_success;

    // Get the switch variable.
    const parse_node_t &switch_value_node = *get_child(statement, 1, symbol_argument);
    const wcstring switch_value = get_source(switch_value_node);

    // Expand it. We need to offset any errors by the position of the string.
    std::vector<completion_t> switch_values_expanded;
    parse_error_list_t errors;
    int expand_ret =
        expand_string(switch_value, &switch_values_expanded, EXPAND_NO_DESCRIPTIONS, &errors);
    parse_error_offset_source_start(&errors, switch_value_node.source_start);

    switch (expand_ret) {
        case EXPAND_ERROR: {
            result = report_errors(errors);
            break;
        }
        case EXPAND_WILDCARD_NO_MATCH: {
            result = report_unmatched_wildcard_error(switch_value_node);
            break;
        }
        case EXPAND_WILDCARD_MATCH:
        case EXPAND_OK: {
            break;
        }
    }

    if (result == parse_execution_success && switch_values_expanded.size() != 1) {
        result =
            report_error(switch_value_node, _(L"switch: Expected exactly one argument, got %lu\n"),
                         switch_values_expanded.size());
    }

    if (result == parse_execution_success) {
        const wcstring &switch_value_expanded = switch_values_expanded.at(0).completion;

        switch_block_t *sb = new switch_block_t();
        parser->push_block(sb);

        // Expand case statements.
        const parse_node_t *case_item_list = get_child(statement, 3, symbol_case_item_list);

        // Loop while we don't have a match but do have more of the list.
        const parse_node_t *matching_case_item = NULL;
        while (matching_case_item == NULL && case_item_list != NULL) {
            if (should_cancel_execution(sb)) {
                result = parse_execution_cancelled;
                break;
            }

            // Get the next item and the remainder of the list.
            const parse_node_t *case_item =
                tree.next_node_in_node_list(*case_item_list, symbol_case_item, &case_item_list);
            if (case_item == NULL) {
                // No more items.
                break;
            }

            // Pull out the argument list.
            const parse_node_t &arg_list = *get_child(*case_item, 1, symbol_argument_list);

            // Expand arguments. A case item list may have a wildcard that fails to expand to
            // anything. We also report case errors, but don't stop execution; i.e. a case item that
            // contains an unexpandable process will report and then fail to match.
            wcstring_list_t case_args;
            parse_execution_result_t case_result =
                this->determine_arguments(arg_list, &case_args, failglob);
            if (case_result == parse_execution_success) {
                for (size_t i = 0; i < case_args.size(); i++) {
                    const wcstring &arg = case_args.at(i);

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
        }

        if (result == parse_execution_success && matching_case_item != NULL) {
            // Success, evaluate the job list.
            const parse_node_t *job_list = get_child(*matching_case_item, 3, symbol_job_list);
            result = this->run_job_list(*job_list, sb);
        }

        parser->pop_block(sb);
    }

    return result;
}

parse_execution_result_t parse_execution_context_t::run_while_statement(
    const parse_node_t &header, const parse_node_t &block_contents) {
    assert(header.type == symbol_while_header);
    assert(block_contents.type == symbol_job_list);

    // Push a while block.
    while_block_t *wb = new while_block_t();
    wb->node_offset = this->get_offset(header);
    parser->push_block(wb);

    parse_execution_result_t ret = parse_execution_success;

    // The conditions of the while loop.
    const parse_node_t &condition_head = *get_child(header, 1, symbol_job);
    const parse_node_t &condition_boolean_tail = *get_child(header, 3, symbol_andor_job_list);

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
        this->run_job_list(block_contents, wb);

        if (this->cancellation_reason(wb) == execution_cancellation_loop_control) {
            // Handle break or continue.
            if (wb->loop_status == LOOP_CONTINUE) {
                // Reset the loop state.
                wb->loop_status = LOOP_NORMAL;
                wb->skip = false;
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

    /* Done */
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
            fprintf(stderr, "Bug: Error reported but no error text found.");
        }

        // Get a backtrace.
        wcstring backtrace_and_desc;
        parser->get_backtrace(src, error_list, &backtrace_and_desc);

        // Print it.
        fprintf(stderr, "%ls", backtrace_and_desc.c_str());
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

/// Handle the case of command not found.
parse_execution_result_t parse_execution_context_t::handle_command_not_found(
    const wcstring &cmd_str, const parse_node_t &statement_node, int err_code) {
    assert(statement_node.type == symbol_plain_statement);

    // We couldn't find the specified command. This is a non-fatal error. We want to set the exit
    // status to 127, which is the standard number used by other shells like bash and zsh.

    const wchar_t *const cmd = cmd_str.c_str();
    const wchar_t *const equals_ptr = wcschr(cmd, L'=');
    if (equals_ptr != NULL) {
        // Try to figure out if this is a pure variable assignment (foo=bar), or if this appears to
        // be running a command (foo=bar ruby...).
        const wcstring name_str = wcstring(cmd, equals_ptr - cmd);  // variable name, up to the =
        const wcstring val_str = wcstring(equals_ptr + 1);          // variable value, past the =

        const parse_node_tree_t::parse_node_list_t args =
            tree.find_nodes(statement_node, symbol_argument, 1);

        if (!args.empty()) {
            const wcstring argument = get_source(*args.at(0));

            wcstring ellipsis_str = wcstring(1, ellipsis_char);
            if (ellipsis_str == L"$") ellipsis_str = L"...";

            // Looks like a command.
            this->report_error(statement_node, ERROR_BAD_EQUALS_IN_COMMAND5, argument.c_str(),
                               name_str.c_str(), val_str.c_str(), argument.c_str(),
                               ellipsis_str.c_str());
        } else {
            this->report_error(statement_node, ERROR_BAD_COMMAND_ASSIGN_ERR_MSG, name_str.c_str(),
                               val_str.c_str());
        }
    } else if ((cmd[0] == L'$' || cmd[0] == VARIABLE_EXPAND || cmd[0] == VARIABLE_EXPAND_SINGLE) &&
               cmd[1] != L'\0') {
        this->report_error(statement_node, _(L"Variables may not be used as commands. In fish, "
                                             L"please define a function or use 'eval %ls'."),
                           cmd);
    } else if (wcschr(cmd, L'$')) {
        this->report_error(
            statement_node,
            _(L"Commands may not contain variables. In fish, please use 'eval %ls'."), cmd);
    } else if (err_code != ENOENT) {
        this->report_error(statement_node, _(L"The file '%ls' is not executable by this user"),
                           cmd ? cmd : L"UNKNOWN");
    } else {
        // Handle unrecognized commands with standard command not found handler that can make better
        // error messages.
        wcstring_list_t event_args;
        {
            parse_execution_result_t arg_result =
                this->determine_arguments(statement_node, &event_args, failglob);

            if (arg_result != parse_execution_success) {
                return arg_result;
            }

            event_args.insert(event_args.begin(), cmd_str);
        }

        event_fire_generic(L"fish_command_not_found", &event_args);

        // Here we want to report an error (so it shows a backtrace), but with no text.
        this->report_error(statement_node, L"");
    }

    // Set the last proc status appropriately.
    proc_set_last_status(err_code == ENOENT ? STATUS_UNKNOWN_COMMAND : STATUS_NOT_EXECUTABLE);

    return parse_execution_errored;
}

/// Creates a 'normal' (non-block) process.
parse_execution_result_t parse_execution_context_t::populate_plain_process(
    job_t *job, process_t *proc, const parse_node_t &statement) {
    assert(job != NULL);
    assert(proc != NULL);
    assert(statement.type == symbol_plain_statement);

    // We may decide that a command should be an implicit cd.
    bool use_implicit_cd = false;

    // Get the command. We expect to always get it here.
    wcstring cmd;
    bool got_cmd = tree.command_for_plain_statement(statement, src, &cmd);
    assert(got_cmd);

    // Expand it as a command. Return an error on failure.
    bool expanded = expand_one(cmd, EXPAND_SKIP_CMDSUBST | EXPAND_SKIP_VARIABLES, NULL);
    if (!expanded) {
        report_error(statement, ILLEGAL_CMD_ERR_MSG, cmd.c_str());
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
        if (!has_command &&
            tree.decoration_for_plain_statement(statement) == parse_statement_decoration_none) {
            // Implicit cd requires an empty argument and redirection list.
            const parse_node_t *args =
                get_child(statement, 1, symbol_arguments_or_redirections_list);
            if (args->child_count == 0) {
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
        const globspec_t glob_behavior = contains(cmd, L"set", L"count") ? nullglob : failglob;
        // Form the list of arguments. The command is the first argument. TODO: count hack, where we
        // treat 'count --help' as different from 'count $foo' that expands to 'count --help'. fish
        // 1.x never successfully did this, but it tried to!
        parse_execution_result_t arg_result =
            this->determine_arguments(statement, &argument_list, glob_behavior);
        if (arg_result != parse_execution_success) {
            return arg_result;
        }
        argument_list.insert(argument_list.begin(), cmd);

        // The set of IO redirections that we construct for the process.
        if (!this->determine_io_chain(statement, &process_io_chain)) {
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
parse_execution_result_t parse_execution_context_t::determine_arguments(
    const parse_node_t &parent, wcstring_list_t *out_arguments, globspec_t glob_behavior) {
    // Get all argument nodes underneath the statement. We guess we'll have that many arguments (but
    // may have more or fewer, if there are wildcards involved).
    const parse_node_tree_t::parse_node_list_t argument_nodes =
        tree.find_nodes(parent, symbol_argument);
    out_arguments->reserve(out_arguments->size() + argument_nodes.size());
    std::vector<completion_t> arg_expanded;
    for (size_t i = 0; i < argument_nodes.size(); i++) {
        const parse_node_t &arg_node = *argument_nodes.at(i);

        // Expect all arguments to have source.
        assert(arg_node.has_source());
        const wcstring arg_str = arg_node.get_source(src);

        // Expand this string.
        parse_error_list_t errors;
        arg_expanded.clear();
        int expand_ret = expand_string(arg_str, &arg_expanded, EXPAND_NO_DESCRIPTIONS, &errors);
        parse_error_offset_source_start(&errors, arg_node.source_start);
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
        }

        // Now copy over any expanded arguments. Do it using swap() to avoid extra allocations; this
        // is called very frequently.
        size_t old_arg_count = out_arguments->size();
        size_t new_arg_count = arg_expanded.size();
        out_arguments->resize(old_arg_count + new_arg_count);
        for (size_t i = 0; i < new_arg_count; i++) {
            wcstring &new_arg = arg_expanded.at(i).completion;
            out_arguments->at(old_arg_count + i).swap(new_arg);
        }
    }

    return parse_execution_success;
}

bool parse_execution_context_t::determine_io_chain(const parse_node_t &statement_node,
                                                   io_chain_t *out_chain) {
    io_chain_t result;
    bool errored = false;

    // We are called with a statement of varying types. We require that the statement have an
    // arguments_or_redirections_list child.
    const parse_node_t &args_and_redirections_list =
        tree.find_child(statement_node, symbol_arguments_or_redirections_list);

    // Get all redirection nodes underneath the statement.
    const parse_node_tree_t::parse_node_list_t redirect_nodes =
        tree.find_nodes(args_and_redirections_list, symbol_redirection);
    for (size_t i = 0; i < redirect_nodes.size(); i++) {
        const parse_node_t &redirect_node = *redirect_nodes.at(i);

        int source_fd = -1;  // source fd
        wcstring target;     // file path or target fd
        enum token_type redirect_type =
            tree.type_for_redirection(redirect_node, src, &source_fd, &target);

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
                    wchar_t *end = NULL;
                    errno = 0;
                    int old_fd = fish_wcstoi(target.c_str(), &end, 10);
                    if (old_fd < 0 || errno || *end) {
                        errored =
                            report_error(redirect_node, _(L"Requested redirection to '%ls', which "
                                                          L"is not a valid file descriptor"),
                                         target.c_str());
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
                fprintf(stderr, "Unexpected redirection type %ld. aborting.\n",
                        (long)redirect_type);
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
        out_chain->swap(result);
    }
    return !errored;
}

parse_execution_result_t parse_execution_context_t::populate_boolean_process(
    job_t *job, process_t *proc, const parse_node_t &bool_statement) {
    // Handle a boolean statement.
    bool skip_job = false;
    assert(bool_statement.type == symbol_boolean_statement);
    switch (parse_node_tree_t::statement_boolean_type(bool_statement)) {
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
            job_set_flag(job, JOB_NEGATE, !job_get_flag(job, JOB_NEGATE));
            break;
        }
    }

    if (skip_job) {
        return parse_execution_skipped;
    }
    const parse_node_t &subject = *tree.get_child(bool_statement, 1, symbol_statement);
    return this->populate_job_process(job, proc, subject);
}

parse_execution_result_t parse_execution_context_t::populate_block_process(
    job_t *job, process_t *proc, const parse_node_t &statement_node) {
    // We handle block statements by creating INTERNAL_BLOCK_NODE, that will bounce back to us when
    // it's time to execute them.
    assert(statement_node.type == symbol_block_statement ||
           statement_node.type == symbol_if_statement ||
           statement_node.type == symbol_switch_statement);

    // The set of IO redirections that we construct for the process.
    io_chain_t process_io_chain;
    bool errored = !this->determine_io_chain(statement_node, &process_io_chain);
    if (errored) return parse_execution_errored;

    proc->type = INTERNAL_BLOCK_NODE;
    proc->internal_block_node = this->get_offset(statement_node);
    proc->set_io_chain(process_io_chain);
    return parse_execution_success;
}

// Returns a process_t allocated with new. It's the caller's responsibility to delete it (!).
parse_execution_result_t parse_execution_context_t::populate_job_process(
    job_t *job, process_t *proc, const parse_node_t &statement_node) {
    assert(statement_node.type == symbol_statement);
    assert(statement_node.child_count == 1);

    // Get the "specific statement" which is boolean / block / if / switch / decorated.
    const parse_node_t &specific_statement = *get_child(statement_node, 0);

    parse_execution_result_t result = parse_execution_success;

    switch (specific_statement.type) {
        case symbol_boolean_statement: {
            result = this->populate_boolean_process(job, proc, specific_statement);
            break;
        }
        case symbol_block_statement:
        case symbol_if_statement:
        case symbol_switch_statement: {
            result = this->populate_block_process(job, proc, specific_statement);
            break;
        }
        case symbol_decorated_statement: {
            // Get the plain statement. It will pull out the decoration itself.
            const parse_node_t &plain_statement =
                tree.find_child(specific_statement, symbol_plain_statement);
            result = this->populate_plain_process(job, proc, plain_statement);
            break;
        }
        default: {
            fprintf(stderr, "'%ls' not handled by new parser yet\n",
                    specific_statement.describe().c_str());
            PARSER_DIE();
            break;
        }
    }

    return result;
}

parse_execution_result_t parse_execution_context_t::populate_job_from_job_node(
    job_t *j, const parse_node_t &job_node, const block_t *associated_block) {
    assert(job_node.type == symbol_job);

    // Tell the job what its command is.
    j->set_command(get_source(job_node));

    // We are going to construct process_t structures for every statement in the job. Get the first
    // statement.
    const parse_node_t *statement_node = get_child(job_node, 0, symbol_statement);
    assert(statement_node != NULL);

    parse_execution_result_t result = parse_execution_success;

    // Create processes. Each one may fail.
    std::vector<process_t *> processes;
    processes.push_back(new process_t());
    result = this->populate_job_process(j, processes.back(), *statement_node);

    // Construct process_ts for job continuations (pipelines), by walking the list until we hit the
    // terminal (empty) job continuation.
    const parse_node_t *job_cont = get_child(job_node, 1, symbol_job_continuation);
    assert(job_cont != NULL);
    while (result == parse_execution_success && job_cont->child_count > 0) {
        assert(job_cont->type == symbol_job_continuation);

        // Handle the pipe, whose fd may not be the obvious stdout.
        const parse_node_t &pipe_node = *get_child(*job_cont, 0, parse_token_type_pipe);
        int pipe_write_fd = fd_redirected_by_pipe(get_source(pipe_node));
        if (pipe_write_fd == -1) {
            result = report_error(pipe_node, ILLEGAL_FD_ERR_MSG, get_source(pipe_node).c_str());
            break;
        }
        processes.back()->pipe_write_fd = pipe_write_fd;

        // Get the statement node and make a process from it.
        const parse_node_t *statement_node = get_child(*job_cont, 1, symbol_statement);
        assert(statement_node != NULL);

        // Store the new process (and maybe with an error).
        processes.push_back(new process_t());
        result = this->populate_job_process(j, processes.back(), *statement_node);

        // Get the next continuation.
        job_cont = get_child(*job_cont, 2, symbol_job_continuation);
        assert(job_cont != NULL);
    }

    // Return what happened.
    if (result == parse_execution_success) {
        // Link up the processes.
        assert(!processes.empty());
        j->first_process = processes.at(0);
        for (size_t i = 1; i < processes.size(); i++) {
            processes.at(i - 1)->next = processes.at(i);
        }
    } else {
        // Clean up processes.
        for (size_t i = 0; i < processes.size(); i++) {
            const process_t *proc = processes.at(i);
            processes.at(i) = NULL;
            delete proc;
        }
    }
    return result;
}

parse_execution_result_t parse_execution_context_t::run_1_job(const parse_node_t &job_node,
                                                              const block_t *associated_block) {
    if (should_cancel_execution(associated_block)) {
        return parse_execution_cancelled;
    }

    // Get terminal modes.
    struct termios tmodes = {};
    if (shell_is_interactive()) {
        if (tcgetattr(STDIN_FILENO, &tmodes)) {
            // Need real error handling here.
            wperror(L"tcgetattr");
            return parse_execution_errored;
        }
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

        const parse_node_t &statement = *get_child(job_node, 0, symbol_statement);
        const parse_node_t &specific_statement = *get_child(statement, 0);
        assert(specific_statement_type_is_redirectable_block(specific_statement));
        switch (specific_statement.type) {
            case symbol_block_statement: {
                result = this->run_block_statement(specific_statement);
                break;
            }
            case symbol_if_statement: {
                result = this->run_if_statement(specific_statement);
                break;
            }
            case symbol_switch_statement: {
                result = this->run_switch_statement(specific_statement);
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
            profile_item->cmd = profiling_cmd_name_for_redirectable_block(specific_statement,
                                                                          this->tree, this->src);
            profile_item->skipped = false;
        }

        return result;
    }

    job_t *j = new job_t(acquire_job_id(), block_io);
    j->tmodes = tmodes;
    job_set_flag(j, JOB_CONTROL,
                 (job_control_mode == JOB_CONTROL_ALL) ||
                     ((job_control_mode == JOB_CONTROL_INTERACTIVE) && (shell_is_interactive())));

    job_set_flag(j, JOB_FOREGROUND, !tree.job_should_be_backgrounded(job_node));

    job_set_flag(j, JOB_TERMINAL, job_get_flag(j, JOB_CONTROL) && !is_subshell && !is_event);

    job_set_flag(j, JOB_SKIP_NOTIFICATION,
                 is_subshell || is_block || is_event || !shell_is_interactive());

    // Tell the current block what its job is. This has to happen before we populate it (#1394).
    parser->current_block()->job = j;

    // Populate the job. This may fail for reasons like command_not_found. If this fails, an error
    // will have been printed.
    parse_execution_result_t pop_result =
        this->populate_job_from_job_node(j, job_node, associated_block);

    // Clean up the job on failure or cancellation.
    bool populated_job = (pop_result == parse_execution_success);
    if (!populated_job || this->should_cancel_execution(associated_block)) {
        assert(parser->current_block()->job == j);
        parser->current_block()->job = NULL;
        delete j;
        j = NULL;
        populated_job = false;
    }

    // Store time it took to 'parse' the command.
    if (profile_item != NULL) {
        parse_time = get_time();
    }

    if (populated_job) {
        // Success. Give the job to the parser - it will clean it up.
        parser->job_add(j);

        // Check to see if this contained any external commands.
        bool job_contained_external_command = false;
        for (const process_t *proc = j->first_process; proc != NULL; proc = proc->next) {
            if (proc->type == EXTERNAL) {
                job_contained_external_command = true;
                break;
            }
        }

        // Actually execute the job.
        exec_job(*this->parser, j);

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
        profile_item->cmd = j ? j->command() : wcstring();
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
        const parse_node_t *job = tree.next_node_in_node_list(*job_list, symbol_job, &job_list);

        if (job != NULL) {
            result = this->run_1_job(*job, associated_block);
        }
    }

    // Returns the last job executed.
    return result;
}

parse_execution_result_t parse_execution_context_t::eval_node_at_offset(
    node_offset_t offset, const block_t *associated_block, const io_chain_t &io) {
    // Don't ever expect to have an empty tree if this is called.
    assert(!tree.empty());
    assert(offset < tree.size());

    // Apply this block IO for the duration of this function.
    scoped_push<io_chain_t> block_io_push(&block_io, io);

    const parse_node_t &node = tree.at(offset);

    // Currently, we only expect to execute the top level job list, or a block node. Assert that.
    assert(node.type == symbol_job_list || specific_statement_type_is_redirectable_block(node));

    enum parse_execution_result_t status = parse_execution_success;
    switch (node.type) {
        case symbol_job_list: {
            // We should only get a job list if it's the very first node. This is because this is
            // the entry point for both top-level execution (the first node) and INTERNAL_BLOCK_NODE
            // execution (which does block statements, but never job lists).
            assert(offset == 0);
            wcstring func_name;
            const parse_node_t *infinite_recursive_node =
                this->infinite_recursive_statement_in_job_list(node, &func_name);
            if (infinite_recursive_node != NULL) {
                // We have an infinite recursion.
                this->report_error(*infinite_recursive_node, INFINITE_FUNC_RECURSION_ERR_MSG,
                                   func_name.c_str());
                status = parse_execution_errored;
            } else {
                // No infinite recursion.
                status = this->run_job_list(node, associated_block);
            }
            break;
        }
        case symbol_block_statement: {
            status = this->run_block_statement(node);
            break;
        }
        case symbol_if_statement: {
            status = this->run_if_statement(node);
            break;
        }
        case symbol_switch_statement: {
            status = this->run_switch_statement(node);
            break;
        }
        default: {
            // In principle, we could support other node types. However we never expect to be passed
            // them - see above.
            fprintf(stderr, "Unexpected node %ls found in %s\n", node.describe().c_str(),
                    __FUNCTION__);
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
    const parse_node_t &node = tree.at(requested_index);
    if (!node.has_source()) {
        return -1;
    }

    size_t char_offset = tree.at(requested_index).source_start;
    return this->line_offset_of_character_at_offset(char_offset);
}

int parse_execution_context_t::line_offset_of_character_at_offset(size_t offset) {
    // Count the number of newlines, leveraging our cache.
    assert(offset <= src.size());

    // Easy hack to handle 0.
    if (offset == 0) {
        return 0;
    }

    // We want to return (one plus) the number of newlines at offsets less than the given offset.
    // cached_lineno_count is the number of newlines at indexes less than cached_lineno_offset.
    const wchar_t *str = src.c_str();
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
        const parse_node_t &node = tree.at(executing_node_idx);
        if (node.has_source()) {
            result = static_cast<int>(node.source_start);
        }
    }
    return result;
}
