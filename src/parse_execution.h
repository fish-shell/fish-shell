/**\file parse_execution.h

   Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.).
*/
#ifndef FISH_PARSE_EXECUTION_H
#define FISH_PARSE_EXECUTION_H

#include <stddef.h>

#include "common.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"
#include <stdbool.h>

class parser_t;
struct block_t;

enum parse_execution_result_t
{
    /* The job was successfully executed (though it have failed on its own). */
    parse_execution_success,

    /* The job did not execute due to some error (e.g. failed to wildcard expand). An error will have been printed and proc_last_status will have been set. */
    parse_execution_errored,

    /* The job was cancelled (e.g. Ctrl-C) */
    parse_execution_cancelled,

    /* The job was skipped (e.g. due to a not-taken 'and' command). This is a special return allowed only from the populate functions, not the run functions. */
    parse_execution_skipped
};

class parse_execution_context_t
{
private:
    const parse_node_tree_t tree;
    const wcstring src;
    io_chain_t block_io;
    parser_t * const parser;
    //parse_error_list_t errors;

    int eval_level;

    /* The currently executing node index, used to indicate the line number */
    node_offset_t executing_node_idx;

    /* Cached line number information */
    size_t cached_lineno_offset;
    int cached_lineno_count;

    /* No copying allowed */
    parse_execution_context_t(const parse_execution_context_t&);
    parse_execution_context_t& operator=(const parse_execution_context_t&);

    /* Should I cancel? */
    bool should_cancel_execution(const block_t *block) const;

    /* Ways that we can stop executing a block. These are in a sort of ascending order of importance, e.g. `exit` should trump `break` */
    enum execution_cancellation_reason_t
    {
        execution_cancellation_none,
        execution_cancellation_loop_control,
        execution_cancellation_skip,
        execution_cancellation_exit
    };
    execution_cancellation_reason_t cancellation_reason(const block_t *block) const;

    /* Report an error. Always returns true. */
    parse_execution_result_t report_error(const parse_node_t &node, const wchar_t *fmt, ...) const;
    parse_execution_result_t report_errors(const parse_error_list_t &errors) const;

    /* Wildcard error helper */
    parse_execution_result_t report_unmatched_wildcard_error(const parse_node_t &unmatched_wildcard);

    /* Command not found support */
    parse_execution_result_t handle_command_not_found(const wcstring &cmd, const parse_node_t &statement_node, int err_code);

    /* Utilities */
    wcstring get_source(const parse_node_t &node) const;
    const parse_node_t *get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type = token_type_invalid) const;
    node_offset_t get_offset(const parse_node_t &node) const;
    const parse_node_t *infinite_recursive_statement_in_job_list(const parse_node_t &job_list, wcstring *out_func_name) const;

    /* Indicates whether a job is a simple block (one block, no redirections) */
    bool job_is_simple_block(const parse_node_t &node) const;

    enum process_type_t process_type_for_command(const parse_node_t &plain_statement, const wcstring &cmd) const;

    /* These create process_t structures from statements */
    parse_execution_result_t populate_job_process(job_t *job, process_t *proc, const parse_node_t &statement_node);
    parse_execution_result_t populate_boolean_process(job_t *job, process_t *proc, const parse_node_t &bool_statement);
    parse_execution_result_t populate_plain_process(job_t *job, process_t *proc, const parse_node_t &statement);
    parse_execution_result_t populate_block_process(job_t *job, process_t *proc, const parse_node_t &statement_node);

    /* These encapsulate the actual logic of various (block) statements. */
    parse_execution_result_t run_block_statement(const parse_node_t &statement);
    parse_execution_result_t run_for_statement(const parse_node_t &header, const parse_node_t &contents);
    parse_execution_result_t run_if_statement(const parse_node_t &statement);
    parse_execution_result_t run_switch_statement(const parse_node_t &statement);
    parse_execution_result_t run_while_statement(const parse_node_t &header, const parse_node_t &contents);
    parse_execution_result_t run_function_statement(const parse_node_t &header, const parse_node_t &block_end_command);
    parse_execution_result_t run_begin_statement(const parse_node_t &header, const parse_node_t &contents);

    enum globspec_t
    {
        failglob,
        nullglob
    };
    parse_execution_result_t determine_arguments(const parse_node_t &parent, wcstring_list_t *out_arguments, globspec_t glob_behavior);

    /* Determines the IO chain. Returns true on success, false on error */
    bool determine_io_chain(const parse_node_t &statement, io_chain_t *out_chain);

    parse_execution_result_t run_1_job(const parse_node_t &job_node, const block_t *associated_block);
    parse_execution_result_t run_job_list(const parse_node_t &job_list_node, const block_t *associated_block);
    parse_execution_result_t populate_job_from_job_node(job_t *j, const parse_node_t &job_node, const block_t *associated_block);

    /* Returns the line number of the node at the given index, indexed from 0. Not const since it touches cached_lineno_offset */
    int line_offset_of_node_at_offset(node_offset_t idx);
    int line_offset_of_character_at_offset(size_t char_idx);

public:
    parse_execution_context_t(moved_ref<parse_node_tree_t> t, const wcstring &s, parser_t *p, int initial_eval_level);

    /* Returns the current eval level */
    int current_eval_level() const
    {
        return eval_level;
    }

    /* Returns the current line number, indexed from 1. Not const since it touches cached_lineno_offset */
    int get_current_line_number();

    /* Returns the source offset, or -1 */
    int get_current_source_offset() const;

    /* Returns the source string */
    const wcstring &get_source() const
    {
        return src;
    }

    /* Start executing at the given node offset. Returns 0 if there was no error, 1 if there was an error */
    parse_execution_result_t eval_node_at_offset(node_offset_t offset, const block_t *associated_block, const io_chain_t &io);

};


#endif
