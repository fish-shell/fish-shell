#include "parse_productions.h"

using namespace parse_productions;
#define NO_PRODUCTION ((production_option_idx_t)(-1))

static bool production_is_empty(const production_t production)
{
    return production[0] == token_type_invalid;
}

/* Empty productions are allowed but must be first. Validate that the given production is in the valid range, i.e. it is either not empty or there is a non-empty production after it */
static bool production_is_valid(const production_options_t production_list, production_option_idx_t which)
{
    if (which < 0 || which >= MAX_PRODUCTIONS)
        return false;

    bool nonempty_found = false;
    for (int i=which; i < MAX_PRODUCTIONS; i++)
    {
        if (! production_is_empty(production_list[i]))
        {
            nonempty_found = true;
            break;
        }
    }
    return nonempty_found;
}

#define PRODUCTIONS(sym) static const production_options_t productions_##sym
#define RESOLVE(sym) static production_option_idx_t resolve_##sym (const parse_token_t &token1, const parse_token_t &token2)
#define RESOLVE_ONLY(sym) static production_option_idx_t resolve_##sym (const parse_token_t &input1, const parse_token_t &input2) { return 0; }

#define KEYWORD(x) ((x) + LAST_TOKEN_OR_SYMBOL + 1)


/* A job_list is a list of jobs, separated by semicolons or newlines */
PRODUCTIONS(job_list) =
{
    {},
    {symbol_job, symbol_job_list},
    {parse_token_type_end, symbol_job_list}
};

RESOLVE(job_list)
{
    switch (token1.type)
    {
        case parse_token_type_string:
            // some keywords are special
            switch (token1.keyword)
            {
                case parse_keyword_end:
                case parse_keyword_else:
                case parse_keyword_case:
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

        default:
            return NO_PRODUCTION;
    }
}

/* A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation */

PRODUCTIONS(job) =
{
    {symbol_statement, symbol_job_continuation, symbol_optional_background}
};
RESOLVE_ONLY(job)

PRODUCTIONS(job_continuation) =
{
    {},
    {parse_token_type_pipe, symbol_statement, symbol_job_continuation}
};
RESOLVE(job_continuation)
{
    switch (token1.type)
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
    /* The only block-like builtin that takes any parameters is 'function' So go to decorated statements if the subsequent token looks like '--'.
        The logic here is subtle:
            If we are 'begin', then we expect to be invoked with no arguments.
            If we are 'function', then we are a non-block if we are invoked with -h or --help
            If we are anything else, we require an argument, so do the same thing if the subsequent token is a statement terminator.
    */

    if (token1.type == parse_token_type_string)
    {
        // If we are a function, then look for help arguments
        // Otherwise, if the next token looks like an option (starts with a dash), then parse it as a decorated statement
        if (token1.keyword == parse_keyword_function && token2.is_help_argument)
        {
            return 4;
        }
        else if (token1.keyword != parse_keyword_function && token2.has_dash_prefix)
        {
            return 4;
        }

        // Likewise if the next token doesn't look like an argument at all. This corresponds to e.g. a "naked if".
        bool naked_invocation_invokes_help = (token1.keyword != parse_keyword_begin && token1.keyword != parse_keyword_end);
        if (naked_invocation_invokes_help && (token2.type == parse_token_type_end || token2.type == parse_token_type_terminate))
        {
            return 4;
        }

    }

    switch (token1.type)
    {
        case parse_token_type_string:
            switch (token1.keyword)
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
                    return NO_PRODUCTION;

                case parse_keyword_switch:
                    return 3;

                case parse_keyword_end:
                    return NO_PRODUCTION;

                    // All other keywords fall through to decorated statement
                default:
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
    {symbol_if_clause, symbol_else_clause, symbol_end_command, symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(if_statement)

PRODUCTIONS(if_clause) =
{
    { KEYWORD(parse_keyword_if), symbol_job, parse_token_type_end, symbol_job_list }
};
RESOLVE_ONLY(if_clause)

PRODUCTIONS(else_clause) =
{
    { },
    { KEYWORD(parse_keyword_else), symbol_else_continuation }
};
RESOLVE(else_clause)
{
    switch (token1.keyword)
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
    switch (token1.keyword)
    {
        case parse_keyword_if:
            return 0;
        default:
            return 1;
    }
}

PRODUCTIONS(switch_statement) =
{
    { KEYWORD(parse_keyword_switch), symbol_argument, parse_token_type_end, symbol_case_item_list, symbol_end_command, symbol_arguments_or_redirections_list}
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
    if (token1.keyword == parse_keyword_case) return 1;
    else if (token1.type == parse_token_type_end) return 2; //empty line
    else return 0;
}

PRODUCTIONS(case_item) =
{
    {KEYWORD(parse_keyword_case), symbol_argument_list, parse_token_type_end, symbol_job_list}
};
RESOLVE_ONLY(case_item)

PRODUCTIONS(argument_list) =
{
    {},
    {symbol_argument, symbol_argument_list}
};
RESOLVE(argument_list)
{
    switch (token1.type)
    {
        case parse_token_type_string:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(freestanding_argument_list) =
{
    {},
    {symbol_argument, symbol_freestanding_argument_list},
    {parse_token_type_end, symbol_freestanding_argument_list},
};
RESOLVE(freestanding_argument_list)
{
    switch (token1.type)
    {
        case parse_token_type_string:
            return 1;
        case parse_token_type_end:
            return 2;
        default:
            return 0;
    }
}


PRODUCTIONS(block_statement) =
{
    {symbol_block_header, symbol_job_list, symbol_end_command, symbol_arguments_or_redirections_list}
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
    switch (token1.keyword)
    {
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
    {KEYWORD(parse_keyword_for), parse_token_type_string, KEYWORD
        (parse_keyword_in), symbol_argument_list, parse_token_type_end}
};
RESOLVE_ONLY(for_header)

PRODUCTIONS(while_header) =
{
    {KEYWORD(parse_keyword_while), symbol_job, parse_token_type_end}
};
RESOLVE_ONLY(while_header)

PRODUCTIONS(begin_header) =
{
    {KEYWORD(parse_keyword_begin)}
};
RESOLVE_ONLY(begin_header)

PRODUCTIONS(function_header) =
{
    {KEYWORD(parse_keyword_function), symbol_argument, symbol_argument_list, parse_token_type_end}
};
RESOLVE_ONLY(function_header)

/* A boolean statement is AND or OR or NOT */
PRODUCTIONS(boolean_statement) =
{
    {KEYWORD(parse_keyword_and), symbol_statement},
    {KEYWORD(parse_keyword_or), symbol_statement},
    {KEYWORD(parse_keyword_not), symbol_statement}
};
RESOLVE(boolean_statement)
{
    switch (token1.keyword)
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
    {symbol_plain_statement},
    {KEYWORD(parse_keyword_command), symbol_plain_statement},
    {KEYWORD(parse_keyword_builtin), symbol_plain_statement},
    {KEYWORD(parse_keyword_exec), symbol_plain_statement}
};
RESOLVE(decorated_statement)
{
    /* If this is e.g. 'command --help' then the command is 'command' and not a decoration. If the second token is not a string, then this is a naked 'command' and we should execute it as undecorated. */
    if (token2.type != parse_token_type_string || token2.has_dash_prefix)
    {
        return 0;
    }

    switch (token1.keyword)
    {
        default:
            return 0;
        case parse_keyword_command:
            return 1;
        case parse_keyword_builtin:
            return 2;
        case parse_keyword_exec:
            return 3;
    }
}

PRODUCTIONS(plain_statement) =
{
    {parse_token_type_string, symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(plain_statement)

PRODUCTIONS(arguments_or_redirections_list) =
{
    {},
    {symbol_argument_or_redirection, symbol_arguments_or_redirections_list}
};
RESOLVE(arguments_or_redirections_list)
{
    switch (token1.type)
    {
        case parse_token_type_string:
        case parse_token_type_redirection:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(argument_or_redirection) =
{
    {symbol_argument},
    {symbol_redirection}
};
RESOLVE(argument_or_redirection)
{
    switch (token1.type)
    {
        case parse_token_type_string:
            return 0;
        case parse_token_type_redirection:
            return 1;
        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(argument) =
{
    {parse_token_type_string}
};
RESOLVE_ONLY(argument)

PRODUCTIONS(redirection) =
{
    {parse_token_type_redirection, parse_token_type_string}
};
RESOLVE_ONLY(redirection)

PRODUCTIONS(optional_background) =
{
    {},
    { parse_token_type_background }
};

RESOLVE(optional_background)
{
    switch (token1.type)
    {
        case parse_token_type_background:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(end_command) =
{
    {KEYWORD(parse_keyword_end)}
};
RESOLVE_ONLY(end_command)

#define TEST(sym) case (symbol_##sym): production_list = & productions_ ## sym ; resolver = resolve_ ## sym ; break;
const production_t *parse_productions::production_for_token(parse_token_type_t node_type, const parse_token_t &input1, const parse_token_t &input2, production_option_idx_t *out_which_production, wcstring *out_error_text)
{
    bool log_it = false;
    if (log_it)
    {
        fprintf(stderr, "Resolving production for %ls with input token <%ls>\n", token_type_description(node_type).c_str(), input1.describe().c_str());
    }

    /* Fetch the list of productions and the function to resolve them */
    const production_options_t *production_list = NULL;
    production_option_idx_t (*resolver)(const parse_token_t &input1, const parse_token_t &input2) = NULL;
    switch (node_type)
    {
            TEST(job_list)
            TEST(job)
            TEST(statement)
            TEST(job_continuation)
            TEST(boolean_statement)
            TEST(block_statement)
            TEST(if_statement)
            TEST(if_clause)
            TEST(else_clause)
            TEST(else_continuation)
            TEST(switch_statement)
            TEST(decorated_statement)
            TEST(case_item_list)
            TEST(case_item)
            TEST(argument_list)
            TEST(freestanding_argument_list)
            TEST(block_header)
            TEST(for_header)
            TEST(while_header)
            TEST(begin_header)
            TEST(function_header)
            TEST(plain_statement)
            TEST(arguments_or_redirections_list)
            TEST(argument_or_redirection)
            TEST(argument)
            TEST(redirection)
            TEST(optional_background)
            TEST(end_command)

        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_end:
        case parse_token_type_terminate:
            fprintf(stderr, "Terminal token type %ls passed to %s\n", token_type_description(node_type).c_str(), __FUNCTION__);
            PARSER_DIE();
            break;

        case parse_special_type_parse_error:
        case parse_special_type_tokenizer_error:
        case parse_special_type_comment:
            fprintf(stderr, "Special type %ls passed to %s\n", token_type_description(node_type).c_str(), __FUNCTION__);
            PARSER_DIE();
            break;


        case token_type_invalid:
            fprintf(stderr, "token_type_invalid passed to %s\n", __FUNCTION__);
            PARSER_DIE();
            break;

    }
    PARSE_ASSERT(production_list != NULL);
    PARSE_ASSERT(resolver != NULL);

    const production_t *result = NULL;
    production_option_idx_t which = resolver(input1, input2);

    if (log_it)
    {
        fprintf(stderr, "\tresolved to %u\n", (unsigned)which);
    }


    if (which == NO_PRODUCTION)
    {
        if (log_it)
        {
            fprintf(stderr, "Node type '%ls' has no production for input '%ls' (in %s)\n", token_type_description(node_type).c_str(), input1.describe().c_str(), __FUNCTION__);
        }
        result = NULL;
    }
    else
    {
        PARSE_ASSERT(production_is_valid(*production_list, which));
        result = &((*production_list)[which]);
    }
    *out_which_production = which;
    return result;
}

