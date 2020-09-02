// Provides the "linkage" between an ast and actual execution structures (job_t, etc.).
#ifndef FISH_PARSE_EXECUTION_H
#define FISH_PARSE_EXECUTION_H

#include <stddef.h>

#include "ast.h"
#include "common.h"
#include "io.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "proc.h"

class block_t;
class cancellation_group_t;
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
    const std::shared_ptr<cancellation_group_t> cancel_group;

    // The currently executing job node, used to indicate the line number.
    const ast::job_t *executing_job_node{};

    // Cached line number information.
    size_t cached_lineno_offset = 0;
    int cached_lineno_count = 0;

    /// The block IO chain.
    /// For example, in `begin; foo ; end < file.txt` this would have the 'file.txt' IO.
    io_chain_t block_io{};

    // No copying allowed.
    parse_execution_context_t(const parse_execution_context_t &) = delete;
    parse_execution_context_t &operator=(const parse_execution_context_t &) = delete;

    // Check to see if we should end execution.
    // \return the eval result to end with, or none() to continue on.
    // This will never return end_execution_reason_t::ok.
    maybe_t<end_execution_reason_t> check_end_execution() const;

    // Report an error, setting $status to \p status. Always returns
    // 'end_execution_reason_t::error'.
    end_execution_reason_t report_error(int status, const ast::node_t &node, const wchar_t *fmt,
                                        ...) const;
    end_execution_reason_t report_errors(int status, const parse_error_list_t &error_list) const;

    /// Command not found support.
    end_execution_reason_t handle_command_not_found(const wcstring &cmd,
                                                    const ast::decorated_statement_t &statement,
                                                    int err_code);

    // Utilities
    wcstring get_source(const ast::node_t &node) const;
    const ast::decorated_statement_t *infinite_recursive_statement_in_job_list(
        const ast::job_list_t &job_list, wcstring *out_func_name) const;

    // Expand a command which may contain variables, producing an expand command and possibly
    // arguments. Prints an error message on error.
    end_execution_reason_t expand_command(const ast::decorated_statement_t &statement,
                                          wcstring *out_cmd, wcstring_list_t *out_args) const;

    /// Indicates whether a job is a simple block (one block, no redirections).
    bool job_is_simple_block(const ast::job_t &job) const;

    enum process_type_t process_type_for_command(const ast::decorated_statement_t &statement,
                                                 const wcstring &cmd) const;
    end_execution_reason_t apply_variable_assignments(
        process_t *proc, const ast::variable_assignment_list_t &variable_assignments,
        const block_t **block);

    // These create process_t structures from statements.
    end_execution_reason_t populate_job_process(
        job_t *job, process_t *proc, const ast::statement_t &statement,
        const ast::variable_assignment_list_t &variable_assignments_list_t);
    end_execution_reason_t populate_not_process(job_t *job, process_t *proc,
                                                const ast::not_statement_t &not_statement);
    end_execution_reason_t populate_plain_process(job_t *job, process_t *proc,
                                                  const ast::decorated_statement_t &statement);

    template <typename Type>
    end_execution_reason_t populate_block_process(job_t *job, process_t *proc,
                                                  const ast::statement_t &statement,
                                                  const Type &specific_statement);

    // These encapsulate the actual logic of various (block) statements.
    end_execution_reason_t run_block_statement(const ast::block_statement_t &statement,
                                               const block_t *associated_block);
    end_execution_reason_t run_for_statement(const ast::for_header_t &header,
                                             const ast::job_list_t &contents);
    end_execution_reason_t run_if_statement(const ast::if_statement_t &statement,
                                            const block_t *associated_block);
    end_execution_reason_t run_switch_statement(const ast::switch_statement_t &statement);
    end_execution_reason_t run_while_statement(const ast::while_header_t &header,
                                               const ast::job_list_t &contents,
                                               const block_t *associated_block);
    end_execution_reason_t run_function_statement(const ast::block_statement_t &statement,
                                                  const ast::function_header_t &header);
    end_execution_reason_t run_begin_statement(const ast::job_list_t &contents);

    enum globspec_t { failglob, nullglob };
    using ast_args_list_t = std::vector<const ast::argument_t *>;

    static ast_args_list_t get_argument_nodes(const ast::argument_list_t &args);
    static ast_args_list_t get_argument_nodes(const ast::argument_or_redirection_list_t &args);

    end_execution_reason_t expand_arguments_from_nodes(const ast_args_list_t &argument_nodes,
                                                       wcstring_list_t *out_arguments,
                                                       globspec_t glob_behavior);

    // Determines the list of redirections for a node.
    end_execution_reason_t determine_redirections(const ast::argument_or_redirection_list_t &list,
                                                  redirection_spec_list_t *out_redirections);

    end_execution_reason_t run_1_job(const ast::job_t &job, const block_t *associated_block);
    end_execution_reason_t test_and_run_1_job_conjunction(const ast::job_conjunction_t &jc,
                                                          const block_t *associated_block);
    end_execution_reason_t run_job_conjunction(const ast::job_conjunction_t &job_expr,
                                               const block_t *associated_block);
    end_execution_reason_t run_job_list(const ast::job_list_t &job_list_node,
                                        const block_t *associated_block);
    end_execution_reason_t run_job_list(const ast::andor_job_list_t &job_list_node,
                                        const block_t *associated_block);
    end_execution_reason_t populate_job_from_job_node(job_t *j, const ast::job_t &job_node,
                                                      const block_t *associated_block);

    // Returns the line number of the node. Not const since it touches cached_lineno_offset.
    int line_offset_of_node(const ast::job_t *node);
    int line_offset_of_character_at_offset(size_t offset);

   public:
    /// Construct a context in preparation for evaluating a node in a tree, with the given block_io.
    /// The cancel group is never null and should be provided when resolving job groups.
    /// The execution context may access the parser and parent job group (if any) through ctx.
    parse_execution_context_t(parsed_source_ref_t pstree, const operation_context_t &ctx,
                              std::shared_ptr<cancellation_group_t> cancel_group,
                              io_chain_t block_io);

    /// Returns the current line number, indexed from 1. Not const since it touches
    /// cached_lineno_offset.
    int get_current_line_number();

    /// Returns the source offset, or -1.
    int get_current_source_offset() const;

    /// Returns the source string.
    const wcstring &get_source() const { return pstree->src; }

    /// Return the parsed ast.
    const ast::ast_t &ast() const { return pstree->ast; }

    /// Start executing at the given node. Returns 0 if there was no error, 1 if there was an
    /// error.
    end_execution_reason_t eval_node(const ast::statement_t &statement,
                                     const block_t *associated_block);
    end_execution_reason_t eval_node(const ast::job_list_t &job_list,
                                     const block_t *associated_block);
};

#endif
