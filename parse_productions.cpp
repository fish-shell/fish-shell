#include "parse_productions.h"

using namespace parse_productions;

#define PRODUCTIONS(sym) static const ProductionList_t sym##_productions
#define RESOLVE(sym) static int resolve_##sym (parse_token_type_t token_type, parse_keyword_t token_keyword)
#define RESOLVE_ONLY(sym) static int resolve_##sym (parse_token_type_t token_type, parse_keyword_t token_keyword) { return 0; }

/* A job_list is a list of jobs, separated by semicolons or newlines */
PRODUCTIONS(job_list) =
{
    {},
    {symbol_job, symbol_job_list},
    {parse_token_type_end, symbol_job_list}
};

RESOLVE(job_list)
{
    switch (token_type)
    {
        case parse_token_type_string:
            // 'end' is special
            switch (token_keyword)
            {
                case parse_keyword_end:
                case parse_keyword_else:
                    // End this job list
                    return 0;

                default:
                    // Normal string
                    return 1;
            }

        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
            return 1;

        case parse_token_type_end:
            // Empty line
            return 2;

        case parse_token_type_terminate:
            // no more commands, just transition to empty
            return 0;
            break;

        default:
            return NO_PRODUCTION;
    }
}

/* A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation */

PRODUCTIONS(job) =
{
    {symbol_statement, symbol_job_continuation}
};
RESOLVE_ONLY(job)

PRODUCTIONS(job_continuation) =
{
    {},
    {parse_token_type_pipe, symbol_statement, symbol_job_continuation}
};
RESOLVE(job_continuation)
{
    switch (token_type)
    {
        case parse_token_type_pipe:
            // Pipe, continuation
            return 1;

        default:
            // Not a pipe, no job continuation
            return 0;
    }
}

/* A statement is a normal command, or an if / while / and etc */
PRODUCTIONS(statement) =
{
    {symbol_boolean_statement},
    {symbol_block_statement},
    {symbol_if_statement},
    {symbol_switch_statement},
    {symbol_decorated_statement}
};
RESOLVE(statement)
{
    switch (token_type)
    {
        case parse_token_type_string:
            switch (token_keyword)
            {
                case parse_keyword_and:
                case parse_keyword_or:
                case parse_keyword_not:
                    return 0;

                case parse_keyword_for:
                case parse_keyword_while:
                case parse_keyword_function:
                case parse_keyword_begin:
                    return 1;

                case parse_keyword_if:
                    return 2;

                case parse_keyword_else:
                    //symbol_stack_pop();
                    return NO_PRODUCTION;

                case parse_keyword_switch:
                    return 3;

                case parse_keyword_end:
                    PARSER_DIE(); //todo
                    return NO_PRODUCTION;

                    // 'in' is only special within a for_header
                case parse_keyword_in:
                case parse_keyword_none:
                case parse_keyword_command:
                case parse_keyword_builtin:
                case parse_keyword_case:
                    return 4;
            }
            break;

        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_terminate:
            return NO_PRODUCTION;
            //parse_error(L"statement", token);

        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(if_statement) =
{
    {symbol_if_clause, symbol_else_clause, PRODUCE_KEYWORD(parse_keyword_end), symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(if_statement)

PRODUCTIONS(if_clause) =
{
    { PRODUCE_KEYWORD(parse_keyword_if), symbol_job, parse_token_type_end, symbol_job_list }
};
RESOLVE_ONLY(if_clause)

PRODUCTIONS(else_clause) =
{
    { },
    { PRODUCE_KEYWORD(parse_keyword_else), symbol_else_continuation }
};
RESOLVE(else_clause)
{
    switch (token_keyword)
    {
        case parse_keyword_else:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(else_continuation) =
{
    {symbol_if_clause, symbol_else_clause},
    {parse_token_type_end, symbol_job_list}
};
RESOLVE(else_continuation)
{
    switch (token_keyword)
    {
        case parse_keyword_if:
            return 0;
        default:
            return 1;
    }
}

PRODUCTIONS(switch_statement) =
{
    { PRODUCE_KEYWORD(parse_keyword_switch), parse_token_type_string, parse_token_type_end, symbol_case_item_list, PRODUCE_KEYWORD(parse_keyword_end)}
};
RESOLVE_ONLY(switch_statement)

PRODUCTIONS(case_item_list) =
{
    {},
    {symbol_case_item, symbol_case_item_list},
    {parse_token_type_end, symbol_case_item_list}
};
RESOLVE(case_item_list)
{
    if (token_keyword == parse_keyword_case) return 1;
    else if (token_type == parse_token_type_end) return 2; //empty line
    else return 0;
}

PRODUCTIONS(case_item) =
{
    {PRODUCE_KEYWORD(parse_keyword_case), symbol_argument_list, parse_token_type_end, symbol_job_list}
};
RESOLVE_ONLY(case_item)

PRODUCTIONS(argument_list_nonempty) =
{
    {parse_token_type_string, symbol_argument_list}
};
RESOLVE_ONLY(argument_list_nonempty)

PRODUCTIONS(argument_list) =
{
    {},
    {symbol_argument_list_nonempty}
};
RESOLVE(argument_list)
{
    switch (token_type)
    {
        case parse_token_type_string: return 1;
        default: return 0;
    }
}

PRODUCTIONS(block_statement) =
{
    {symbol_block_header, parse_token_type_end, symbol_job_list, PRODUCE_KEYWORD(parse_keyword_end), symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(block_statement)

PRODUCTIONS(block_header) =
{
    {symbol_for_header},
    {symbol_while_header},
    {symbol_function_header},
    {symbol_begin_header}
};
RESOLVE(block_header)
{
    switch (token_keyword)
    {
        case parse_keyword_else:
            return NO_PRODUCTION;
        case parse_keyword_for:
            return 0;
        case parse_keyword_while:
            return 1;
        case parse_keyword_function:
            return 2;
        case parse_keyword_begin:
            return 3;
        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(for_header) =
{
    {PRODUCE_KEYWORD(parse_keyword_for), parse_token_type_string, PRODUCE_KEYWORD(parse_keyword_in), symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(for_header)

PRODUCTIONS(while_header) =
{
    {PRODUCE_KEYWORD(parse_keyword_while), symbol_statement}
};
RESOLVE_ONLY(while_header)

PRODUCTIONS(begin_header) =
{
    {PRODUCE_KEYWORD(parse_keyword_begin)}
};
RESOLVE_ONLY(begin_header)

PRODUCTIONS(function_header) =
{
    {PRODUCE_KEYWORD(parse_keyword_function), parse_token_type_string, symbol_argument_list}
};
RESOLVE_ONLY(function_header)

/* A boolean statement is AND or OR or NOT */
PRODUCTIONS(boolean_statement) =
{
    {PRODUCE_KEYWORD(parse_keyword_and), symbol_statement},
    {PRODUCE_KEYWORD(parse_keyword_or), symbol_statement},
    {PRODUCE_KEYWORD(parse_keyword_not), symbol_statement}
};
RESOLVE(boolean_statement)
{
    switch (token_keyword)
    {
        case parse_keyword_and:
            return 0;
        case parse_keyword_or:
            return 1;
        case parse_keyword_not:
            return 2;
        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(decorated_statement) =
{
    {PRODUCE_KEYWORD(parse_keyword_command), symbol_plain_statement},
    {PRODUCE_KEYWORD(parse_keyword_builtin), symbol_plain_statement},
    {symbol_plain_statement}
};
RESOLVE(decorated_statement)
{
    switch (token_keyword)
    {
        case parse_keyword_command:
            return 0;
        case parse_keyword_builtin:
            return 1;
        default:
            return 2;
    }
}

PRODUCTIONS(plain_statement) =
{
    {parse_token_type_string, symbol_arguments_or_redirections_list, symbol_optional_background}
};
RESOLVE_ONLY(plain_statement)

PRODUCTIONS(arguments_or_redirections_list) =
{
    {},
    {symbol_argument_or_redirection, symbol_arguments_or_redirections_list}
};
RESOLVE(arguments_or_redirections_list)
{
    switch (token_type)
    {
        case parse_token_type_string:
        case parse_token_type_redirection:
            return 1;
        default:
            return 0;
    }
}

