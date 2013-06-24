/**\file parse_exec.h

    Programmatic execution of a parse tree
*/

#ifndef FISH_PARSE_EXEC_H
#define FISH_PARSE_EXEC_H

#include "parse_tree.h"

struct parse_execution_visitor_t;
class parse_exec_t;
class parse_execution_context_t
{
    parse_exec_t *ctx; //owned
    
    public:
    parse_execution_context_t(const parse_node_tree_t &n, const wcstring &s);
    ~parse_execution_context_t();
    
    bool visit_next_node(parse_execution_visitor_t *visitor);
    
    // Gets the source for a node at a given index
    void get_source(node_offset_t idx, wcstring *result) const;
};


struct exec_argument_t
{
    node_offset_t parse_node_idx;
};
typedef std::vector<exec_argument_t> exec_argument_list_t;

struct exec_redirection_t
{
    node_offset_t parse_node_idx;
};
typedef std::vector<exec_redirection_t> exec_redirection_list_t;

struct exec_arguments_and_redirections_t
{
    exec_argument_list_t arguments;
    exec_redirection_list_t redirections;
};

struct exec_basic_statement_t
{
    // Node containing the command
    node_offset_t command_idx;
    
    // Arguments
    exec_arguments_and_redirections_t arguments_and_redirections;
    
    // Decoration
    enum
    {
        decoration_plain,
        decoration_command,
        decoration_builtin
    } decoration;
        
    exec_basic_statement_t();
    
    void set_decoration(uint32_t k)
    {
        PARSE_ASSERT(k == parse_keyword_none || k == parse_keyword_command || k == parse_keyword_builtin);
        switch (k)
        {
            case parse_keyword_none:
                decoration = decoration_plain;
                break;
            case parse_keyword_command:
                decoration = decoration_command;
                break;
            case parse_keyword_builtin:
                decoration = decoration_builtin;
                break;
            default:
                PARSER_DIE();
                break;
        }
    }
    
    const exec_argument_list_t &arguments() const
    {
        return arguments_and_redirections.arguments;
    }
    
    const exec_redirection_list_t &redirections() const
    {
        return arguments_and_redirections.redirections;
    }
};

struct exec_function_header_t
{
    // Node containing the function name
    node_offset_t name_idx;
    
    // Node containing the function body
    node_offset_t body_idx;
    
    // Arguments
    exec_arguments_and_redirections_t arguments_and_redirections;
};

struct exec_block_statement_t
{
    // Arguments
    exec_arguments_and_redirections_t arguments_and_redirections;
};

struct if_header_t
{
    // Node containing the body of the if statement
    node_offset_t body;
};

struct parse_execution_visitor_t
{
    node_offset_t node_idx;
    parse_execution_context_t *context;
    
    parse_execution_visitor_t() : node_idx(0), context(NULL)
    {
    }
    
    virtual bool enter_job_list(void) { return true; }
    virtual bool enter_job(void) { return true; }
    virtual void visit_statement(void) { }
    virtual void visit_function(const exec_function_header_t &function) { }
    virtual bool enter_block_statement(const exec_block_statement_t &statement) { return true; }
    
    virtual void enter_if_header(const if_header_t &statement) { }
    virtual void exit_if_header(const if_header_t &statement) { }
    
    virtual void visit_boolean_statement(void) { }
    virtual void visit_basic_statement(const exec_basic_statement_t &statement) { }
    virtual void exit_job(void) { }
    virtual void exit_job_list(void) { }
};

#endif
