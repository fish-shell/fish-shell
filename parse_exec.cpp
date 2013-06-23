#include "parse_exec.h"
#include <stack>

typedef uint16_t sanity_id_t;
static sanity_id_t next_sanity_id()
{
    static sanity_id_t last_sanity_id;
    return ++last_sanity_id;
}

struct exec_node_t
{
    node_offset_t parse_node_idx;
    sanity_id_t command_sanity_id;
    
    exec_node_t(size_t pni) : parse_node_idx(pni)
    {
    }
    
};

struct exec_argument_t
{
    node_offset_t parse_node_idx;
    sanity_id_t command_sanity_id;
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
    
    uint16_t sanity_id;
    
    exec_basic_statement_t() : command_idx(0), decoration(decoration_plain)
    {
        sanity_id = next_sanity_id();
    }
    
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

struct exec_block_statement_t
{
    // Arguments
    exec_arguments_and_redirections_t arguments_and_redirections;
    
    const exec_argument_list_t &arguments() const
    {
        return arguments_and_redirections.arguments;
    }
    
    const exec_redirection_list_t &redirections() const
    {
        return arguments_and_redirections.redirections;
    }

};

struct exec_job_t
{
    // List of statements (separated with pipes)
    std::vector<exec_basic_statement_t> statements;
    
    void add_statement(const exec_basic_statement_t &statement)
    {
        statements.push_back(statement);
    }
};


class parse_exec_t
{
    parse_node_tree_t parse_tree;
    wcstring src;
    
    bool simulating;
    wcstring_list_t simulation_result;
    
    /* The stack of nodes as we execute them */
    std::vector<exec_node_t> exec_nodes;
    
    /* The stack of jobs being built */
    std::vector<exec_job_t> assembling_jobs;
    
    /* The stack of commands being built */
    std::vector<exec_basic_statement_t> assembling_statements;
    
    void get_node_string(node_offset_t idx, wcstring *output) const
    {
        const parse_node_t &node = parse_tree.at(idx);
        PARSE_ASSERT(node.source_start <= src.size());
        PARSE_ASSERT(node.source_start + node.source_length <= src.size());
        output->assign(src, node.source_start, node.source_length);
    }
    
    const parse_node_t &get_child(parse_node_t &parent, node_offset_t which) const
    {
        return parse_tree.at(parent.child_offset(which));
    }
    
    void pop_push_specific(node_offset_t idx1, node_offset_t idx2 = NODE_OFFSET_INVALID, node_offset_t idx3 = NODE_OFFSET_INVALID, node_offset_t idx4 = NODE_OFFSET_INVALID, node_offset_t idx5 = NODE_OFFSET_INVALID)
    {
        PARSE_ASSERT(! exec_nodes.empty());
        // Figure out the offset of the children
        exec_node_t &top = exec_nodes.back();
        const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
        node_offset_t child_node_idx = parse_node.child_start;
        
        // Remove the top node
        exec_nodes.pop_back();
        
        // Append the given children, backwards
        sanity_id_t command_sanity_id = assembling_statements.empty() ? 0 : assembling_statements.back().sanity_id;
        const node_offset_t idxs[] = {idx5, idx4, idx3, idx2, idx1};
        for (size_t q=0; q < sizeof idxs / sizeof *idxs; q++)
        {
            node_offset_t idx = idxs[q];
            if (idx != (node_offset_t)(-1))
            {
                PARSE_ASSERT(idx < parse_node.child_count);
                exec_nodes.push_back(child_node_idx + idx);
                exec_nodes.back().command_sanity_id = command_sanity_id;
            }
        }

    }
    
    void pop_push(node_offset_t child_idx, node_offset_t child_count = 1)
    {
        PARSE_ASSERT(! exec_nodes.empty());
        if (child_count == 0)
        {
            // No children, just remove the top node
            exec_nodes.pop_back();
        }
        else
        {
            // Figure out the offset of the children
            exec_node_t &top = exec_nodes.back();
            const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
            PARSE_ASSERT(child_idx < parse_node.child_count);
            node_offset_t child_node_idx = parse_node.child_start + child_idx;
            
            // Remove the top node
            exec_nodes.pop_back();
            
            // Append the given children, backwards
            sanity_id_t command_sanity_id = assembling_statements.empty() ? 0 : assembling_statements.back().sanity_id;
            node_offset_t cursor = child_count;
            while (cursor--)
            {
                exec_nodes.push_back(child_node_idx + cursor);
                exec_nodes.back().command_sanity_id = command_sanity_id;
            }
        }
    }
    
    void pop()
    {
        PARSE_ASSERT(! exec_nodes.empty());
        exec_nodes.pop_back();
    }
    
    void pop_push_all()
    {
        exec_node_t &top = exec_nodes.back();
        const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
        pop_push(0, parse_node.child_count);
    }
    
    void assemble_1_argument_or_redirection(node_offset_t idx, exec_arguments_and_redirections_t *output) const
    {
        const parse_node_t &node = parse_tree.at(idx);
        PARSE_ASSERT(output != NULL);
        PARSE_ASSERT(node.type == symbol_argument_or_redirection);
        PARSE_ASSERT(node.child_count == 1);
        node_offset_t child_idx = node.child_offset(0);
        const parse_node_t &child = parse_tree.at(child_idx);
        switch (child.type)
        {
            case parse_token_type_string:
                // Argument
                {
                    exec_argument_t arg = exec_argument_t();
                    arg.parse_node_idx = idx;
                    output->arguments.push_back(arg);
                }
                break;
                
            case parse_token_type_redirection:
                // Redirection
                {
                    exec_redirection_t redirect = exec_redirection_t();
                    redirect.parse_node_idx = idx;
                    output->redirections.push_back(redirect);
                }
                break;
                
            default:
                PARSER_DIE();
                break;
        }
    }
    
    void assemble_arguments_and_redirections(node_offset_t start_idx, exec_arguments_and_redirections_t *output) const
    {
        node_offset_t idx = start_idx;
        for (;;)
        {
            const parse_node_t &node = parse_tree.at(idx);
            PARSE_ASSERT(node.type == symbol_arguments_or_redirections_list);
            PARSE_ASSERT(node.child_count == 0 || node.child_count == 2);
            if (node.child_count == 0)
            {
                // No more children
                break;
            }
            else
            {
                // Skip to next child
                assemble_1_argument_or_redirection(node.child_offset(0), output);
                idx = node.child_offset(1);
            }
        }
    }
    
    void assemble_command_for_plain_statement(node_offset_t idx, parse_keyword_t decoration)
    {
        const parse_node_t &node = parse_tree.at(idx);
        PARSE_ASSERT(node.type == symbol_plain_statement);
        PARSE_ASSERT(node.child_count == 2);
        exec_basic_statement_t statement;
        statement.set_decoration(decoration);
        statement.command_idx = node.child_offset(0);
        assemble_arguments_and_redirections(node.child_offset(1), &statement.arguments_and_redirections);
        assembling_jobs.back().add_statement(statement);   
    }
    
    void job_assembly_complete()
    {
        PARSE_ASSERT(! assembling_jobs.empty());
        const exec_job_t &job = assembling_jobs.back();
        
        if (simulating)
        {
            simulate_job(job);
        }
        assembling_jobs.pop_back();
    }
    
    void simulate_job(const exec_job_t &job)
    {
        PARSE_ASSERT(simulating);
        wcstring line;
        for (size_t i=0; i < job.statements.size(); i++)
        {
            if (i > 0)
            {
                line.append(L" <pipe> ");
            }
            const exec_basic_statement_t &statement = job.statements.at(i);
            switch (statement.decoration)
            {
                case exec_basic_statement_t::decoration_builtin:
                    line.append(L"<builtin> ");
                    break;
                
                case exec_basic_statement_t::decoration_command:
                    line.append(L"<command> ");
                    break;
                    
                default:
                break;
            }
            
            wcstring tmp;
            get_node_string(statement.command_idx, &tmp);
            line.append(L"cmd:");
            line.append(tmp);
            for (size_t i=0; i < statement.arguments().size(); i++)
            {
                const exec_argument_t &arg = statement.arguments().at(i);
                get_node_string(arg.parse_node_idx, &tmp);
                line.append(L" ");
                line.append(L"arg:");            
                line.append(tmp);
            }
        }
        simulation_result.push_back(line);        
    }
    
    void enter_parse_node(size_t idx);
    void run_top_node(void);
    exec_job_t *create_job(void);
    
    public:
    parse_exec_t(const parse_node_tree_t &tree, const wcstring &s) : parse_tree(tree), src(s), simulating(false)
    {
    }
    wcstring simulate(void);
};

exec_job_t *parse_exec_t::create_job()
{
    assembling_jobs.push_back(exec_job_t());
    return &assembling_jobs.back();
}

void parse_exec_t::run_top_node()
{
    PARSE_ASSERT(! exec_nodes.empty());
    exec_node_t &top = exec_nodes.back();
    const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
    bool log = true;
    
    if (log)
    {
        wcstring tmp;
        tmp.append(exec_nodes.size(), L' ');
        tmp.append(parse_node.describe());
        printf("%ls\n", tmp.c_str());
    }
    
    switch (parse_node.type)
    {
        case symbol_job_list:
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 2);
            if (parse_node.child_count == 0)
            {
                // No more jobs, done
                pop();
            }
            else if (parse_tree.at(parse_node.child_start + 0).type == parse_token_type_end)
            {
                // Empty job, so just skip it
                pop_push(1, 1);
            }
            else
            {
                // Normal job
                pop_push(0, 2);
            }
            break;
        
        case symbol_job:
        {
            PARSE_ASSERT(parse_node.child_count == 2);
            exec_job_t *job = create_job();
            pop_push_all();
            break;
        }
        
        case symbol_job_continuation:
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 3);
            if (parse_node.child_count == 0)
            {
                // All done with this job
                job_assembly_complete();
                pop();
            }
            else
            {
                // Skip the pipe
                pop_push(1, 2);
            }
            break;            
        
        case symbol_statement:
        {
            PARSE_ASSERT(parse_node.child_count == 1);
            pop_push_all();
            break;
        }
        
        case symbol_block_statement:
        {
            PARSE_ASSERT(parse_node.child_count == 5);
            pop_push_specific(0, 2, 4);
            break;
        }
        
        case symbol_block_header:
        {
            PARSE_ASSERT(parse_node.child_count == 1);
            pop_push_all();
            break;
        }
        
        case symbol_function_header:
        {
            PARSE_ASSERT(parse_node.child_count == 3);
            //pop_push_all();
            pop();
            break;
        }
            
        case symbol_decorated_statement:
        {
            PARSE_ASSERT(parse_node.child_count == 1 || parse_node.child_count == 2);
            
            node_offset_t plain_statement_idx = parse_node.child_offset(parse_node.child_count - 1);
            parse_keyword_t decoration = static_cast<parse_keyword_t>(parse_node.tag);
            assemble_command_for_plain_statement(plain_statement_idx, decoration);
            pop();
            break;
        }

        // The following symbols should be handled by their parents, i.e. never pushed on our stack
        case symbol_plain_statement:
        case symbol_arguments_or_redirections_list:
        case symbol_argument_or_redirection:
            PARSER_DIE();
            break;
            
        case parse_token_type_end:
            PARSE_ASSERT(parse_node.child_count == 0);
            pop();
            break;
            
        default:
            fprintf(stderr, "Unhandled token type %ls at index %ld\n", token_type_description(parse_node.type).c_str(), top.parse_node_idx);
            PARSER_DIE();
            break;
        
    }
}

void parse_exec_t::enter_parse_node(size_t idx)
{
    PARSE_ASSERT(idx < parse_tree.size());
    exec_node_t exec(idx);
    exec_nodes.push_back(exec);
}

wcstring parse_exec_t::simulate(void)
{
    if (parse_tree.empty())
        return L"(empty!)";
    
    PARSE_ASSERT(exec_nodes.empty());
    simulating = true;
    
    enter_parse_node(0);
    while (! exec_nodes.empty())
    {
        run_top_node();
    }
    
    wcstring result;
    for (size_t i=0; i < simulation_result.size(); i++)
    {
        result.append(simulation_result.at(i));
        result.append(L"\n");
    }
    
    return result;
}

parse_execution_context_t::parse_execution_context_t(const parse_node_tree_t &n, const wcstring &s)
{
    ctx = new parse_exec_t(n, s);
}

wcstring parse_execution_context_t::simulate(void)
{
    return ctx->simulate();
}
