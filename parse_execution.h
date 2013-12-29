/**\file parse_execution.h

   Provides the "linkage" between a parse_node_tree_t and actual execution structures (job_t, etc.).
*/

#ifndef FISH_PARSE_EXECUTION_H
#define FISH_PARSE_EXECUTION_H

#include "config.h"
#include "util.h"
#include "parse_tree.h"
#include "proc.h"

class job_t;
struct profile_item_t;
struct block_t;

class parse_execution_context_t
{
    private:
    const parse_node_tree_t tree;
    const wcstring src;
    io_chain_t block_io;
    parser_t * const parser;
    parse_error_list_t errors;
    
    int eval_level;
    std::vector<profile_item_t*> profile_items;
    
    /* No copying allowed */
    parse_execution_context_t(const parse_execution_context_t&);
    parse_execution_context_t& operator=(const parse_execution_context_t&);
    
    /* Should I cancel? */
    bool should_cancel_execution(const block_t *block) const;
    
    /* Report an error. Always returns true. */
    bool append_error(const parse_node_t &node, const wchar_t *fmt, ...);
    /* Wildcard error helper */
    bool append_unmatched_wildcard_error(const parse_node_t &unmatched_wildcard);
    
    /* Utilities */
    wcstring get_source(const parse_node_t &node) const;
    const parse_node_t *get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type = token_type_invalid) const;
    node_offset_t get_offset(const parse_node_t &node) const;
    
    /* These create process_t structures from statements */
    process_t *create_job_process(job_t *job, const parse_node_t &statement_node);
    process_t *create_boolean_process(job_t *job, const parse_node_t &bool_statement);
    process_t *create_plain_process(job_t *job, const parse_node_t &statement);
    process_t *create_block_process(job_t *job, const parse_node_t &statement_node);
    
    /* These encapsulate the actual logic of various (block) statements. They just do what the statement says. */
    int run_block_statement(const parse_node_t &statement);
    int run_for_statement(const parse_node_t &header, const parse_node_t &contents);
    int run_if_statement(const parse_node_t &statement);
    int run_switch_statement(const parse_node_t &statement);
    int run_while_statement(const parse_node_t &header, const parse_node_t &contents);
    int run_function_statement(const parse_node_t &header, const parse_node_t &contents);
    int run_begin_statement(const parse_node_t &header, const parse_node_t &contents);
    
    wcstring_list_t determine_arguments(const parse_node_t &parent, const parse_node_t **out_unmatched_wildcard_node);
    
    /* Determines the IO chain. Returns true on success, false on error */
    bool determine_io_chain(const parse_node_t &statement, io_chain_t *out_chain);
    
    int run_1_job(const parse_node_t &job_node, const block_t *associated_block);
    int run_job_list(const parse_node_t &job_list_node, const block_t *associated_block);
    bool populate_job_from_job_node(job_t *j, const parse_node_t &job_node);
    
    public:
    parse_execution_context_t(const parse_node_tree_t &t, const wcstring &s, parser_t *p);
    
    /* Start executing at the given node offset. Returns 0 if there was no error, 1 if there was an error */
    int eval_node_at_offset(node_offset_t offset, const block_t *associated_block, const io_chain_t &io);
    
};


#endif
