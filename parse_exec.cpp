#include "parse_exec.h"
#include <stack>


struct exec_node_t
{
    node_offset_t parse_node_idx;
    
    exec_node_t(size_t pni) : parse_node_idx(pni)
    {
    }
    
    virtual ~exec_node_t();
};

exec_node_t::~exec_node_t()
{
}

struct exec_redirection_t : public exec_node_t
{

};

struct exec_argument_t : public exec_node_t
{
    
};

struct exec_statement_t
{
    enum
    {
        decoration_plain,
        decoration_command,
        decoration_builtin
    } decoration;
    
    std::vector<exec_argument_t> arguments;
    std::vector<exec_redirection_t> redirections;
};

class parse_exec_t
{
    parse_node_tree_t parse_tree;
    wcstring src;
    std::vector<exec_node_t> exec_nodes;
    
    parse_exec_t(const parse_node_tree_t &tree, const wcstring &s) : parse_tree(tree), src(s)
    {
    }
    
    void pop_push(uint32_t child_idx)
    {
        exec_node_t &top = exec_nodes.back();
        const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
        PARSE_ASSERT(child_idx < parse_node.child_count);
        node_offset_t child_node_idx = parse_node.child_start + child_idx;
        exec_nodes.pop_back();
        exec_nodes.push_back(child_node_idx);
        
    }
    
    void simulate(void);
    void enter_parse_node(size_t idx);
    void run_top_node(void);
};

void parse_exec_t::run_top_node()
{
    PARSE_ASSERT(! exec_nodes.empty());
    exec_node_t &top = exec_nodes.back();
    const parse_node_t &parse_node = parse_tree.at(top.parse_node_idx);
    
    switch (parse_node.type)
    {
        case symbol_statement_list:
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 2);
            if (parse_node.child_count == 0)
            {
                // Statement list done
                exec_nodes.pop_back();
            }
            else
            {
                // First child is a statement, next is the rest of the list
                node_offset_t head = parse_node.child_start;
                node_offset_t tail = parse_node.child_start + 1;
                exec_nodes.pop_back();
                exec_nodes.push_back(tail);
                exec_nodes.push_back(head);
            }
            break;
            
        case symbol_statement:
            PARSE_ASSERT(parse_node.child_count == 1);
            pop_push(0);
            break;
            
        case decorated_statement:
            PARSE_ASSERT(parse_node.child_count == 1 || parse_node.child_count == 2 );
            pop_push(0);
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
    PARSE_ASSERT(exec_nodes.empty());
    assemble_statement_list(0);
    enter_parse_node(0);
    run_node();
}

wcstring parse_execution_context_t::simulate()
{
    if (parse_tree.empty())
        return L"(empty!)";
    
    PARSE_ASSERT(node_idx < nodes.size());
    PARSE_ASSERT(nodes.at(node_idx).type == symbol_statement_list);
    
    wcstring result;
    
}

parse_execution_context_t::parse_execution_context_t(const parse_node_tree_t &n, const wcstring &s)
{
    ctx = new parse_exec_t(n, s);
}

wcstring parse_execution_context_t::simulate(void)
{
    return ctx->simulate();
}
