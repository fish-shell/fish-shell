// Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.).
#ifndef FISH_PARSE_EXECUTION_H
#define FISH_PARSE_EXECUTION_H

#include <stddef.h>

#include "common.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"

class parser_t;
struct block_t;

enum parse_execution_result_t {
    /// The job was successfully executed (though it have failed on its own).
    parse_execution_success,
    /// The job did not execute due to some error (e.g. failed to wildcard expand). An error will
    /// have been printed and proc_last_status will have been set.
    parse_execution_errored,
    /// The job was cancelled (e.g. Ctrl-C).
    parse_execution_cancelled,
    /// The job was skipped (e.g. due to a not-taken 'and' command). This is a special return
    /// allowed only from the populate functions, not the run functions.
    parse_execution_skipped
};

class parse_execution_context_t {
   private:
    parsed_source_ref_t pstree;
    io_chain_t block_io;
    parser_t *const parser;
    // The currently executing job node, used to indicate the line number.
    tnode_t<grammar::job> executing_job_node{};
    // Cached line number information.
    size_t cached_lineno_offset = 0;
    int cached_lineno_count = 0;
    // The parent job for any jobs created by this context.
    const std::shared_ptr<job_t> parent_job;
    // No copying allowed.
    parse_execution_context_t(const parse_execution_context_t &) = delete;
    parse_execution_context_t &operator=(const parse_execution_context_t &) = delete;

    // Should I cancel?
    bool should_cancel_execution(const block_t *block) const;

    // Ways that we can stop executing a block. These are in a sort of ascending order of
    // importance, e.g. `exit` should trump `break`.
    enum execution_cancellation_reason_t {
        execution_cancellation_none,
        execution_cancellation_loop_control,
        execution_cancellation_skip,
        execution_cancellation_exit
    };
    execution_cancellation_reason_t cancellation_reason(const block_t *block) const;

    // Report an error. Always returns true.
    parse_execution_result_t report_error(const parse_node_t &node, const wchar_t *fmt, ...) const;
    parse_execution_result_t report_errors(const parse_error_list_t &errors) const;

    // Wildcard error helper.
    parse_execution_result_t report_unmatched_wildcard_error(
        const parse_node_t &unmatched_wildcard) const;

    /// Command not found support.
    parse_execution_result_t handle_command_not_found(const wcstring &cmd,
                                                      tnode_t<grammar::plain_statement> statement,
                                                      int err_code);

    // Utilities
    wcstring get_source(const parse_node_t &node) const;
    tnode_t<grammar::plain_statement> infinite_recursive_statement_in_job_list(
        tnode_t<grammar::job_list> job_list, wcstring *out_func_name) const;
    bool is_function_context() const;

    // Expand a command which may contain variables, producing an expand command and possibly
    // arguments. Prints an error message on error.
    parse_execution_result_t expand_command(tnode_t<grammar::plain_statement> statement,
                                            wcstring *out_cmd, wcstring_list_t *out_args) const;

    /// Return whether we should skip a job with the given bool statement type.
    bool should_skip(parse_bool_statement_type_t type) const;

    /// Indicates whether a job is a simple block (one block, no redirections).
    bool job_is_simple_block(tnode_t<grammar::job> job) const;

    enum process_type_t process_type_for_command(tnode_t<grammar::plain_statement> statement,
                                                 const wcstring &cmd) const;

    // These create process_t structures from statements.
    parse_execution_result_t populate_job_process(job_t *job, process_t *proc,
                                                  tnode_t<grammar::statement> statement);
    parse_execution_result_t populate_not_process(job_t *job, process_t *proc,
                                                  tnode_t<grammar::not_statement> not_statement);
    parse_execution_result_t populate_plain_process(job_t *job, process_t *proc,
                                                    tnode_t<grammar::plain_statement> statement);

    template <typename Type>
    parse_execution_result_t populate_block_process(job_t *job, process_t *proc,
                                                    tnode_t<grammar::statement> statement,
                                                    tnode_t<Type> specific_statement);

    // These encapsulate the actual logic of various (block) statements.
    parse_execution_result_t run_block_statement(tnode_t<grammar::block_statement> statement,
                                                 const block_t *associated_block);
    parse_execution_result_t run_for_statement(tnode_t<grammar::for_header> header,
                                               tnode_t<grammar::job_list> contents);
    parse_execution_result_t run_if_statement(tnode_t<grammar::if_statement> statement,
                                              const block_t *associated_block);
    parse_execution_result_t run_switch_statement(tnode_t<grammar::switch_statement> statement);
    parse_execution_result_t run_while_statement(tnode_t<grammar::while_header> statement,
                                                 tnode_t<grammar::job_list> contents,
                                                 const block_t *associated_block);
    parse_execution_result_t run_function_statement(tnode_t<grammar::function_header> header,
                                                    tnode_t<grammar::job_list> body);
    parse_execution_result_t run_begin_statement(tnode_t<grammar::job_list> contents);

    enum globspec_t { failglob, nullglob };
    using argument_node_list_t = std::vector<tnode_t<grammar::argument>>;
    parse_execution_result_t expand_arguments_from_nodes(const argument_node_list_t &argument_nodes,
                                                         wcstring_list_t *out_arguments,
                                                         globspec_t glob_behavior);

    // Determines the IO chain. Returns true on success, false on error.
    bool determine_io_chain(tnode_t<grammar::arguments_or_redirections_list> node,
                            io_chain_t *out_chain);

    parse_execution_result_t run_1_job(tnode_t<grammar::job> job, const block_t *associated_block);
    parse_execution_result_t run_job_conjunction(tnode_t<grammar::job_conjunction> job_conj, const block_t *associated_block);
    template <typename Type>
    parse_execution_result_t run_job_list(tnode_t<Type> job_list_node,
                                          const block_t *associated_block);
    parse_execution_result_t populate_job_from_job_node(job_t *j, tnode_t<grammar::job> job_node,
                                                        const block_t *associated_block);

    // Returns the line number of the node. Not const since it touches cached_lineno_offset.
    int line_offset_of_node(tnode_t<grammar::job> node);
    int line_offset_of_character_at_offset(size_t char_idx);

   public:
    parse_execution_context_t(parsed_source_ref_t pstree, parser_t *p,
                              std::shared_ptr<job_t> parent);

    /// Returns the current line number, indexed from 1. Not const since it touches
    /// cached_lineno_offset.
    int get_current_line_number();

    /// Returns the source offset, or -1.
    int get_current_source_offset() const;

    /// Returns the source string.
    const wcstring &get_source() const { return pstree->src; }

    /// Return the parse tree.
    const parse_node_tree_t &tree() const { return pstree->tree; }

    /// Start executing at the given node. Returns 0 if there was no error, 1 if there was an
    /// error.
    parse_execution_result_t eval_node(tnode_t<grammar::statement> statement,
                                       const block_t *associated_block, const io_chain_t &io);
    parse_execution_result_t eval_node(tnode_t<grammar::job_list> job_list,
                                       const block_t *associated_block, const io_chain_t &io);
};

#endif
