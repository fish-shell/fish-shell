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

class parse_execution_context_t
{
    private:
    const parse_node_tree_t tree;
    const wcstring src;
    parser_t * const parser;
    parse_error_list_t errors;
    
    std::vector<profile_item_t*> profile_items;
    
    /* We maintain a stack of job lists to be executed, and something to do after the execution is finished. This is a pointer to member function that takes a node, a status, and the statement that was executed */
    typedef void (parse_execution_context_t::*statement_completion_handler_t)(const parse_node_t &node);
    
    struct parse_execution_stack_element_t
    {
        // These point into our tree, which is immutable
        const parse_node_t *job_or_job_list;
        statement_completion_handler_t completion_handler;
        const parse_node_t *node;
    };
    std::vector<parse_execution_stack_element_t> job_stack;
    
    void stack_push(const parse_node_t *job_or_job_list, statement_completion_handler_t completion_handler, const parse_node_t *node);
    
    /* No copying allowed */
    parse_execution_context_t(const parse_execution_context_t&);
    parse_execution_context_t& operator=(const parse_execution_context_t&);
    
    /* Report an error. Always returns true. */
    bool append_error(const parse_node_t &node, const wchar_t *fmt, ...);
    
    wcstring get_source(const parse_node_t &node) const;
    const parse_node_t *get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type = token_type_invalid);
    
    node_offset_t get_offset(const parse_node_t &node) const;
    
    process_t *create_job_process(job_t *job, const parse_node_t &statement_node);
    process_t *create_boolean_process(job_t *job, const parse_node_t &bool_statement);
    process_t *create_for_process(job_t *job, const parse_node_t &header, const parse_node_t &statement);
    process_t *create_while_process(job_t *job, const parse_node_t &header, const parse_node_t &statement);
    process_t *create_begin_process(job_t *job, const parse_node_t &header, const parse_node_t &statement);
    process_t *create_plain_process(job_t *job, const parse_node_t &statement);
    
    wcstring_list_t determine_arguments(const parse_node_t &parent, const parse_node_t **out_unmatched_wildcard_node);
    io_chain_t determine_io_chain(const parse_node_t &statement);
    
    void eval_1_job(const parse_node_t &job_node);
    void eval_job(job_t *j, const parse_node_t &job_node);
    
    void eval_next_stack_elem();
    
    public:
    parse_execution_context_t(const parse_node_tree_t &t, const wcstring s, parser_t *p);
    
    void eval_job_list(const parse_node_t &job_node);
    
};


#endif
