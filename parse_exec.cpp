#include "parse_exec.h"
#include <stack>

struct exec_node_t
{
    node_offset_t parse_node_idx;
    node_offset_t body_parse_node_idx;
    bool visited;

    explicit exec_node_t(node_offset_t pni) : parse_node_idx(pni), body_parse_node_idx(NODE_OFFSET_INVALID), visited(false)
    {
    }

    explicit exec_node_t(node_offset_t pni, node_offset_t body_pni) : parse_node_idx(pni), body_parse_node_idx(body_pni), visited(false)
    {
    }
};

exec_basic_statement_t::exec_basic_statement_t() : command_idx(0), decoration(decoration_plain)
{

}


class parse_exec_t
{
    parse_node_tree_t parse_tree;
    wcstring src;

    /* The stack of nodes as we execute them */
    std::vector<exec_node_t> exec_nodes;

    /* The stack of commands being built */
    std::vector<exec_basic_statement_t> assembling_statements;

    /* Current visitor (very transient) */
    struct parse_execution_visitor_t * visitor;

    const parse_node_t &get_child(const parse_node_t &parent, node_offset_t which) const
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
        const node_offset_t idxs[] = {idx5, idx4, idx3, idx2, idx1};
        for (size_t q=0; q < sizeof idxs / sizeof *idxs; q++)
        {
            node_offset_t idx = idxs[q];
            if (idx != (node_offset_t)(-1))
            {
                PARSE_ASSERT(idx < parse_node.child_count);
                exec_nodes.push_back(exec_node_t(child_node_idx + idx));
            }
        }

    }

    void push(node_offset_t global_idx)
    {
        exec_nodes.push_back(exec_node_t(global_idx));
    }

    void push(const exec_node_t &node)
    {
        exec_nodes.push_back(node);
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
            node_offset_t cursor = child_count;
            while (cursor--)
            {
                exec_nodes.push_back(exec_node_t(child_node_idx + cursor));
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
                arg.parse_node_idx = child_idx;
                output->arguments.push_back(arg);
            }
            break;

            case parse_token_type_redirection:
                // Redirection
            {
                exec_redirection_t redirect = exec_redirection_t();
                redirect.parse_node_idx = child_idx;
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
        visitor->visit_basic_statement(statement);
    }

    void assemble_block_statement(node_offset_t parse_node_idx)
    {

        const parse_node_t &node = parse_tree.at(parse_node_idx);
        PARSE_ASSERT(node.type == symbol_block_statement);
        PARSE_ASSERT(node.child_count == 5);

        // Fetch arguments and redirections. These ought to be evaluated before the job list
        exec_block_statement_t statement;
        assemble_arguments_and_redirections(node.child_offset(4), &statement.arguments_and_redirections);

        // Generic visit
        visitor->enter_block_statement(statement);

        // Dig into the header to discover the type
        const parse_node_t &header_parent = parse_tree.at(node.child_offset(0));
        PARSE_ASSERT(header_parent.type == symbol_block_header);
        PARSE_ASSERT(header_parent.child_count == 1);
        const node_offset_t header_idx = header_parent.child_offset(0);

        // Fetch body (job list)
        node_offset_t body_idx = node.child_offset(2);
        PARSE_ASSERT(parse_tree.at(body_idx).type == symbol_job_list);

        pop();
        push(exec_node_t(header_idx, body_idx));
    }

    /* which: 0 -> if, 1 -> else if, 2 -> else */
    void assemble_if_else_clause(exec_node_t &exec_node, const parse_node_t &node, int which)
    {
        if (which == 0)
        {
            PARSE_ASSERT(node.type == symbol_if_clause);
            PARSE_ASSERT(node.child_count == 4);
        }
        else if (which == 2)
        {
            PARSE_ASSERT(node.type == symbol_else_continuation);
            PARSE_ASSERT(node.child_count == 2);
        }

        struct exec_if_clause_t clause;
        if (which == 0)
        {
            clause.body = node.child_offset(3);
        }
        else
        {
            clause.body = node.child_offset(1);
        }
        if (! exec_node.visited)
        {
            visitor->enter_if_clause(clause);
            exec_node.visited = true;
            if (which == 0)
            {
                push(node.child_offset(1));
            }
        }
        else
        {
            visitor->exit_if_clause(clause);
            pop();
        }
    }

    void assemble_arguments(node_offset_t start_idx, exec_argument_list_t *output) const
    {
        node_offset_t idx = start_idx;
        for (;;)
        {
            const parse_node_t &node = parse_tree.at(idx);
            PARSE_ASSERT(node.type == symbol_argument_list || node.type == symbol_argument_list_nonempty);
            if (node.type == symbol_argument_list)
            {
                // argument list, may be empty
                PARSE_ASSERT(node.child_count == 0 || node.child_count == 1);
                if (node.child_count == 0)
                {
                    break;
                }
                else
                {
                    idx = node.child_offset(0);
                }
            }
            else
            {
                // nonempty argument list
                PARSE_ASSERT(node.child_count == 2);
                output->push_back(exec_argument_t(node.child_offset(0)));
                idx = node.child_offset(1);
            }
        }
    }

    void assemble_1_case_item(exec_switch_statement_t *statement, node_offset_t node_idx)
    {
        const parse_node_t &node = parse_tree.at(node_idx);
        PARSE_ASSERT(node.type == symbol_case_item);

        // add a new case
        size_t len = statement->cases.size();
        statement->cases.resize(len + 1);
        exec_switch_case_t &new_case = statement->cases.back();

        // assemble it
        new_case.body = node.child_offset(3);
        assemble_arguments(node.child_offset(1), &new_case.arguments);


    }

    void assemble_case_item_list(exec_switch_statement_t *statement, node_offset_t node_idx)
    {
        const parse_node_t &node = parse_tree.at(node_idx);
        PARSE_ASSERT(node.type == symbol_case_item_list);
        PARSE_ASSERT(node.child_count == 0 || node.child_count == 2);
        if (node.child_count == 2)
        {
            assemble_1_case_item(statement, node.child_offset(0));
            assemble_case_item_list(statement, node.child_offset(1));
        }
    }

    void assemble_switch_statement(const exec_node_t &exec_node, const parse_node_t &parse_node)
    {
        PARSE_ASSERT(parse_node.type == symbol_switch_statement);
        exec_switch_statement_t statement;

        statement.argument.parse_node_idx = parse_node.child_offset(1);
        assemble_case_item_list(&statement, parse_node.child_offset(3));

        visitor->visit_switch_statement(statement);

        // pop off the switch
        pop();
    }

    void assemble_function_header(const exec_node_t &exec_node, const parse_node_t &header)
    {
        PARSE_ASSERT(header.type == symbol_function_header);
        PARSE_ASSERT(&header == &parse_tree.at(exec_node.parse_node_idx));
        PARSE_ASSERT(exec_node.body_parse_node_idx != NODE_OFFSET_INVALID);
        exec_function_header_t function_info;
        function_info.name_idx = header.child_offset(1);
        function_info.body_idx = exec_node.body_parse_node_idx;
        assemble_arguments(header.child_offset(2), &function_info.arguments);
        visitor->visit_function(function_info);

        // Always pop
        pop();
    }


    void enter_parse_node(size_t idx);
    void run_top_node(void);

public:

    void get_node_string(node_offset_t idx, wcstring *output) const
    {
        const parse_node_t &node = parse_tree.at(idx);
        PARSE_ASSERT(node.source_start <= src.size());
        PARSE_ASSERT(node.source_start + node.source_length <= src.size());
        output->assign(src, node.source_start, node.source_length);
    }

    bool visit_next_node(parse_execution_visitor_t *v);

    parse_exec_t(const parse_node_tree_t &tree, const wcstring &s) : parse_tree(tree), src(s), visitor(NULL)
    {
        if (! parse_tree.empty())
        {
            exec_nodes.push_back(exec_node_t(0));
        }
    }
};

void parse_exec_t::run_top_node()
{
    PARSE_ASSERT(! exec_nodes.empty());
    exec_node_t &exec_node = exec_nodes.back();
    const node_offset_t parse_node_idx = exec_node.parse_node_idx;
    const parse_node_t &parse_node = parse_tree.at(exec_node.parse_node_idx);
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
                visitor->exit_job_list();
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
                visitor->enter_job_list();
                pop_push(0, 2);
            }
            break;

        case symbol_job:
        {
            PARSE_ASSERT(parse_node.child_count == 2);
            visitor->enter_job();
            pop_push_all();
            break;
        }

        case symbol_job_continuation:
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 3);
            if (parse_node.child_count == 0)
            {
                // All done with this job
                visitor->exit_job();
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
            assemble_block_statement(parse_node_idx);
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
            assemble_function_header(exec_node, parse_node);
            break;
        }

        case symbol_if_statement:
        {
            PARSE_ASSERT(parse_node.child_count == 4);
            pop_push(0, 2);
            break;
        }

        case symbol_if_clause:
        {
            PARSE_ASSERT(parse_node.child_count == 4);
            assemble_if_else_clause(exec_node, parse_node, 0);
            pop();
            break;
        }

        case symbol_else_clause:
        {
            PARSE_ASSERT(parse_node.child_count == 0 || parse_node.child_count == 2);
            if (parse_node.child_count == 0)
            {
                // No else
                pop();
            }
            else
            {
                // We have an else
                pop_push(1);
            }
            break;
        }

        case symbol_else_continuation:
        {
            // Figure out if this is an else if or a terminating else
            PARSE_ASSERT(parse_node.child_count == 2);
            const parse_node_t &first_child = get_child(parse_node, 1);
            PARSE_ASSERT(first_child.type == symbol_if_clause || first_child.type == parse_token_type_end);
            if (first_child.type == symbol_if_clause)
            {
                pop_push_all();
            }
            else
            {
                // else
                assemble_if_else_clause(exec_node, parse_node, 2);
                pop();
            }
            break;
        }

        case symbol_switch_statement:
        {
            assemble_switch_statement(exec_node, parse_node);
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
        case symbol_case_item_list:
        case symbol_plain_statement:
        case symbol_arguments_or_redirections_list:
        case symbol_argument_or_redirection:
            fprintf(stderr, "Unexpected token type %ls at index %ld. This should have been handled by the parent.\n", token_type_description(parse_node.type).c_str(), exec_node.parse_node_idx);
            PARSER_DIE();
            break;

        case parse_token_type_end:
            PARSE_ASSERT(parse_node.child_count == 0);
            pop();
            break;

        default:
            fprintf(stderr, "Unhandled token type %ls at index %ld\n", token_type_description(parse_node.type).c_str(), exec_node.parse_node_idx);
            PARSER_DIE();
            break;

    }
}

bool parse_exec_t::visit_next_node(parse_execution_visitor_t *v)
{
    PARSE_ASSERT(v != NULL);
    PARSE_ASSERT(visitor == NULL);
    if (exec_nodes.empty())
    {
        return false;
    }

    visitor = v;
    run_top_node();
    visitor = NULL;
    return true;
}

void parse_exec_t::enter_parse_node(size_t idx)
{
    PARSE_ASSERT(idx < parse_tree.size());
    exec_node_t exec(idx);
    exec_nodes.push_back(exec);
}


parse_execution_context_t::parse_execution_context_t(const parse_node_tree_t &n, const wcstring &s)
{
    ctx = new parse_exec_t(n, s);
}

parse_execution_context_t::~parse_execution_context_t()
{
    delete ctx;
}

bool parse_execution_context_t::visit_next_node(parse_execution_visitor_t *visitor)
{
    return ctx->visit_next_node(visitor);
}

void parse_execution_context_t::get_source(node_offset_t idx, wcstring *result) const
{
    return ctx->get_node_string(idx, result);
}




