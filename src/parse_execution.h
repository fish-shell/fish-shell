// Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.).
#ifndef FISH_PARSE_EXECUTION_H
#define FISH_PARSE_EXECUTION_H

#include <stddef.h>

#include "common.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"

class block_t;
class operation_context_t;
class parser_t;

/// An eval_result represents evaluation errors including wildcards which failed to match, syntax
/// errors, or other expansion errors. It also tracks when evaluation was skipped due to signal
/// cancellation. Note it does not track the exit status of commands.
enum class end_execution_reason_t {
    /// Evaluation was successfull.
    ok,

    /// Evaluation was skipped due to control flow (break or return).
    control_flow,

    /// Evaluation was cancelled, e.g. because of a signal or exit.
    cancelled,

    /// A parse error or failed expansion (but not an error exit status from a command).
    error,
};

class parse_execution_context_t {
   private:
    parsed_source_ref_t pstree;
    parser_t *const parser;
    const operation_context_t &ctx;
    // The currently executing job node, used to indicate the line number.
    tnode_t<grammar::job> executing_job_node{};
    // Cached line number information.
    size_t cached_lineno_offset = 0;
    int cached_lineno_count = 0;
    // The lineage for any jobs created by this context.
    const job_lineage_t lineage;
    // No copying allowed.
    parse_execution_context_t(const parse_execution_context_t &) = delete;
    parse_execution_context_t &operator=(const parse_execution_context_t &) = delete;

    // Check to see if we should end execution.
    // \return the eval result to end with, or none() to continue on.
    // This will never return end_execution_reason_t::ok.
    maybe_t<end_execution_reason_t> check_end_execution() const;

    // Report an error, setting $status to \p status. Always returns
    // 'end_execution_reason_t::error'.
    end_execution_reason_t report_error(int status, const parse_node_t &node, const wchar_t *fmt,
                                        ...) const;
    end_execution_reason_t report_errors(int status, const parse_error_list_t &error_list) const;

    /// Command not found support.
    end_execution_reason_t handle_command_not_found(const wcstring &cmd,
                                                    tnode_t<grammar::plain_statement> statement,
                                                    int err_code);

    // Utilities
    wcstring get_source(const parse_node_t &node) const;
    tnode_t<grammar::plain_statement> infinite_recursive_statement_in_job_list(
        tnode_t<grammar::job_list> job_list, wcstring *out_func_name) const;

    // Expand a command which may contain variables, producing an expand command and possibly
    // arguments. Prints an error message on error.
    end_execution_reason_t expand_command(tnode_t<grammar::plain_statement> statement,
                                          wcstring *out_cmd, wcstring_list_t *out_args) const;

    /// Return whether we should skip a job with the given bool statement type.
    bool should_skip(parse_job_decoration_t type) const;

    /// Indicates whether a job is a simple block (one block, no redirections).
    bool job_is_simple_block(tnode_t<grammar::job> job) const;

    enum process_type_t process_type_for_command(tnode_t<grammar::plain_statement> statement,
                                                 const wcstring &cmd) const;
    end_execution_reason_t apply_variable_assignments(
        process_t *proc, tnode_t<grammar::variable_assignments> variable_assignments,
        const block_t **block);

    // These create process_t structures from statements.
    end_execution_reason_t populate_job_process(
        job_t *job, process_t *proc, tnode_t<grammar::statement> statement,
        tnode_t<grammar::variable_assignments> variable_assignments);
    end_execution_reason_t populate_not_process(job_t *job, process_t *proc,
                                                tnode_t<grammar::not_statement> not_statement);
    end_execution_reason_t populate_plain_process(job_t *job, process_t *proc,
                                                  tnode_t<grammar::plain_statement> statement);

    template <typename Type>
    end_execution_reason_t populate_block_process(job_t *job, process_t *proc,
                                                  tnode_t<grammar::statement> statement,
                                                  tnode_t<Type> specific_statement);

    // These encapsulate the actual logic of various (block) statements.
    end_execution_reason_t run_block_statement(tnode_t<grammar::block_statement> statement,
                                               const block_t *associated_block);
    end_execution_reason_t run_for_statement(tnode_t<grammar::for_header> header,
                                             tnode_t<grammar::job_list> contents);
    end_execution_reason_t run_if_statement(tnode_t<grammar::if_statement> statement,
                                            const block_t *associated_block);
    end_execution_reason_t run_switch_statement(tnode_t<grammar::switch_statement> statement);
    end_execution_reason_t run_while_statement(tnode_t<grammar::while_header> header,
                                               tnode_t<grammar::job_list> contents,
                                               const block_t *associated_block);
    end_execution_reason_t run_function_statement(tnode_t<grammar::block_statement> statement,
                                                  tnode_t<grammar::function_header> header);
    end_execution_reason_t run_begin_statement(tnode_t<grammar::job_list> contents);

    enum globspec_t { failglob, nullglob };
    using argument_node_list_t = std::vector<tnode_t<grammar::argument>>;
    end_execution_reason_t expand_arguments_from_nodes(const argument_node_list_t &argument_nodes,
                                                       wcstring_list_t *out_arguments,
                                                       globspec_t glob_behavior);

    // Determines the list of redirections for a node.
    end_execution_reason_t determine_redirections(
        tnode_t<grammar::arguments_or_redirections_list> node,
        redirection_spec_list_t *out_redirections);

    end_execution_reason_t run_1_job(tnode_t<grammar::job> job, const block_t *associated_block);
    end_execution_reason_t run_job_conjunction(tnode_t<grammar::job_conjunction> job_expr,
                                               const block_t *associated_block);
    template <typename Type>
    end_execution_reason_t run_job_list(tnode_t<Type> job_list_node,
                                        const block_t *associated_block);
    end_execution_reason_t populate_job_from_job_node(job_t *j, tnode_t<grammar::job> job_node,
                                                      const block_t *associated_block);

    // Returns the line number of the node. Not const since it touches cached_lineno_offset.
    int line_offset_of_node(tnode_t<grammar::job> node);
    int line_offset_of_character_at_offset(size_t offset);

   public:
    parse_execution_context_t(parsed_source_ref_t pstree, parser_t *p,
                              const operation_context_t &ctx, job_lineage_t lineage);

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
    end_execution_reason_t eval_node(tnode_t<grammar::statement> statement,
                                     const block_t *associated_block);
    end_execution_reason_t eval_node(tnode_t<grammar::job_list> job_list,
                                     const block_t *associated_block);
};

#endif
