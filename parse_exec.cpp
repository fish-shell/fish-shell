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

struct exec_redirection_t
{

};

struct exec_basic_statement_t
{
    // Node containing the command
    node_offset_t command_idx;
    
    // Decoration
    enum
    {
        decoration_plain,
        decoration_command,
        decoration_builtin
    } decoration;
    
    std::vector<exec_argument_t> arguments;
    std::vector<exec_redirection_t> redirections;
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
};

class parse_exec_t
{
    parse_node_tree_t parse_tree;
    wcstring src;
    
    bool simulating;
    wcstring_list_t simulation_result;
    
    /* The stack of nodes as we execute them */
    std::vector<exec_node_t> exec_nodes;
    
    /* The stack of commands being built */
    std::vector<exec_basic_statement_t> assembling_statements;
    
    void get_node_string(node_offset_t idx, wcstring *output) const
    {
        const parse_node_t &node = parse_tree.at(idx);
        PARSE_ASSERT(node.source_start <= src.size());
        PARSE_ASSERT(node.source_start + node.source_length <= src.size());
        output->assign(src, node.source_start, node.source_length);
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
    
    void assemble_command(node_offset_t idx)
    {
        // Set the command for our top basic statement
        PARSE_ASSERT(! assembling_statements.empty());
        assembling_statements.back().command_idx = idx;
    }
    
    void assemble_argument_or_redirection(node_offset_t idx)
    {
        const parse_node_t &node = parse_tree.at(idx);
        PARSE_ASSERT(! assembling_statements.empty());
        exec_basic_statement_t &statement = assembling_statements.back();
        switch (node.type)
        {
            case parse_token_type_string:
                // Argument
                {
                    exec_argument_t arg = exec_argument_t();
                    arg.parse_node_idx = idx;
                    arg.command_sanity_id = statement.sanity_id;
                    statement.arguments.push_back(arg);
                }
                break;
                
            case parse_token_type_redirection:
                // Redirection
                break;
                
            default:
                PARSER_DIE();
                break;
        }
        
    }
    
    void assembly_complete()
    {
        // Finished building a command
        PARSE_ASSERT(! assembling_statements.empty());
        const exec_basic_statement_t &statement = assembling_statements.back();
        
        if (simulating)
        {
            simulate_statement(statement);
        }
        assembling_statements.pop_back();
    }
    
    void simulate_statement(const exec_basic_statement_t &statement)
    {
        PARSE_ASSERT(simulating);
        wcstring line;
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
        for (size_t i=0; i < statement.arguments.size(); i++)
        {
            const exec_argument_t &arg = statement.arguments.at(i);
            get_node_string(arg.parse_node_idx, &tmp);
            line.append(L" ");
            line.append(L"arg:");            
            line.append(tmp);
        }
        simulation_result.push_back(line);
    }
    
    void enter_parse_node(size_t idx);
    void run_top_node(void);
    exec_basic_statement_t *create_basic_statement(void);
    
    public:
    parse_exec_t(const parse_node_tree_t &tree, const wcstring &s) : parse_tree(tree), src(s), simulating(false)
    {
    }
    wcstring simulate(void);
};

exec_basic_statement_t *parse_exec_t::create_basic_statement()
{
    assembling_statements.push_back(exec_basic_statement_t());
    return &assembling_statements.back();
}

void parse_exec_t::run_top_node()
{
    PARSE_ASSERT(! exec_nodes.empty());
    exec_node_t &top = exec_nodes.back();
    const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
    bool log = false;
    
    if (log)
    {
        wcstring tmp;
        tmp.append(exec_nodes.size(), L' ');
        tmp.append(parse_node.describe());
        printf("%ls\n", tmp.c_str());
    }
    
    switch (parse_node.type)
    {
        case symbol_statement_list:
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 2);
            pop_push_all();
            break;
            
        case symbol_statement:
            PARSE_ASSERT(parse_node.child_count == 1);
            pop_push_all();
            break;
            
        case symbol_decorated_statement:
        {
            PARSE_ASSERT(parse_node.child_count == 1 || parse_node.child_count == 2 );
            exec_basic_statement_t *cmd = create_basic_statement();
            cmd->set_decoration(parse_node.tag);
            
            // Push the last node (skip any decoration)
            pop_push(parse_node.child_count - 1, 1);
            break;
        }
            
        case symbol_plain_statement:
            PARSE_ASSERT(parse_node.child_count == 3);
            // Extract the command
            PARSE_ASSERT(! assembling_statements.empty());
            assemble_command(parse_node.child_start + 0);
            // Jump to statement list, then terminator
            pop_push(1, 2);
            break;
            
        case symbol_arguments_or_redirections_list:
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 2);
            pop_push_all();
            break;
        
        case symbol_argument_or_redirection:
            PARSE_ASSERT(parse_node.child_count == 1);
            assemble_argument_or_redirection(parse_node.child_start + 0);
            pop();
            break;
            
        case parse_token_type_end:
            PARSE_ASSERT(parse_node.child_count == 0);
            assembly_complete();
            pop();
            break;
            
        default:
            fprintf(stderr, "Unhandled token type %ld\n", (long)parse_node.type);
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
