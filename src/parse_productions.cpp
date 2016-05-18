#include "config.h"  // IWYU pragma: keep

#include <stdio.h>

#include "common.h"
#include "parse_constants.h"
#include "parse_productions.h"
#include "parse_tree.h"

using namespace parse_productions;

#define NO_PRODUCTION NULL

// Herein are encoded the productions for our LL2 fish grammar.
//
// Each symbol (e.g. symbol_job_list) has a corresponding function (e.g. resolve_job_lits). The
// function accepts two tokens, representing the first and second lookahead, and returns returns a
// production representing the rule, or NULL on error. There is also a tag value which is returned
// by reference; the tag is a sort of node annotation.
//
// Productions are generally a static const array, and we return a pointer to the array (yes,
// really).

#define RESOLVE(sym)                          \
    static const production_t *resolve_##sym( \
        const parse_token_t &token1, const parse_token_t &token2, parse_node_tag_t *out_tag)

// Hacktastic?
#define RESOLVE_ONLY(sym)                                                                      \
    extern const production_t sym##_only;                                                      \
    static const production_t *resolve_##sym(                                                  \
        const parse_token_t &token1, const parse_token_t &token2, parse_node_tag_t *out_tag) { \
        return &sym##_only;                                                                    \
    }                                                                                          \
    const production_t sym##_only

#define KEYWORD(x) ((x) + LAST_TOKEN_OR_SYMBOL + 1)

/// Helper macro to define an array.
#define P static const production_t

/// A job_list is a list of jobs, separated by semicolons or newlines.
RESOLVE(job_list) {
    P list_end = {};
    P normal = {symbol_job, symbol_job_list};
    P empty_line = {parse_token_type_end, symbol_job_list};
    switch (token1.type) {
        case parse_token_type_string: {
            // Some keywords are special.
            switch (token1.keyword) {
                case parse_keyword_end:
                case parse_keyword_else:
                case parse_keyword_case: {
                    return &list_end;  // end this job list
                }
                default: {
                    return &normal;  // normal string
                }
            }
        }
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background: {
            return &normal;
        }
        case parse_token_type_end: {
            return &empty_line;
        }
        case parse_token_type_terminate: {
            return &list_end;  // no more commands, just transition to empty
        }
        default: { return NO_PRODUCTION; }
    }
}

// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like
// if statements, where we require a command). To represent "non-empty", we require a statement,
// followed by a possibly empty job_continuation.

RESOLVE_ONLY(job) = {symbol_statement, symbol_job_continuation, symbol_optional_background};

RESOLVE(job_continuation) {
    P empty = {};
    P piped = {parse_token_type_pipe, symbol_statement, symbol_job_continuation};
    switch (token1.type) {
        case parse_token_type_pipe: {
            return &piped;  // pipe, continuation
        }
        default: {
            return &empty;  // not a pipe, no job continuation
        }
    }
}

// A statement is a normal command, or an if / while / and etc.
RESOLVE(statement) {
    P boolean = {symbol_boolean_statement};
    P block = {symbol_block_statement};
    P ifs = {symbol_if_statement};
    P switchs = {symbol_switch_statement};
    P decorated = {symbol_decorated_statement};
    // The only block-like builtin that takes any parameters is 'function' So go to decorated
    // statements if the subsequent token looks like '--'. The logic here is subtle:
    //
    // If we are 'begin', then we expect to be invoked with no arguments.
    // If we are 'function', then we are a non-block if we are invoked with -h or --help
    // If we are anything else, we require an argument, so do the same thing if the subsequent token
    // is a statement terminator.
    if (token1.type == parse_token_type_string) {
        // If we are a function, then look for help arguments. Otherwise, if the next token looks
        // like an option (starts with a dash), then parse it as a decorated statement.
        if (token1.keyword == parse_keyword_function && token2.is_help_argument) {
            return &decorated;
        } else if (token1.keyword != parse_keyword_function && token2.has_dash_prefix) {
            return &decorated;
        }

        // Likewise if the next token doesn't look like an argument at all. This corresponds to e.g.
        // a "naked if".
        bool naked_invocation_invokes_help =
            (token1.keyword != parse_keyword_begin && token1.keyword != parse_keyword_end);
        if (naked_invocation_invokes_help &&
            (token2.type == parse_token_type_end || token2.type == parse_token_type_terminate)) {
            return &decorated;
        }
    }

    switch (token1.type) {
        case parse_token_type_string: {
            switch (token1.keyword) {
                case parse_keyword_and:
                case parse_keyword_or:
                case parse_keyword_not: {
                    return &boolean;
                }
                case parse_keyword_for:
                case parse_keyword_while:
                case parse_keyword_function:
                case parse_keyword_begin: {
                    return &block;
                }
                case parse_keyword_if: {
                    return &ifs;
                }
                case parse_keyword_else: {
                    return NO_PRODUCTION;
                }
                case parse_keyword_switch: {
                    return &switchs;
                }
                case parse_keyword_end: {
                    return NO_PRODUCTION;
                }
                // All other keywords fall through to decorated statement.
                default: { return &decorated; }
            }
            break;
        }
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_terminate: {
            return NO_PRODUCTION;
            // parse_error(L"statement", token);
        }
        default: { return NO_PRODUCTION; }
    }
}

RESOLVE_ONLY(if_statement) = {symbol_if_clause, symbol_else_clause, symbol_end_command,
                              symbol_arguments_or_redirections_list};
RESOLVE_ONLY(if_clause) = {KEYWORD(parse_keyword_if), symbol_job, parse_token_type_end,
                           symbol_andor_job_list, symbol_job_list};

RESOLVE(else_clause) {
    P empty = {};
    P else_cont = {KEYWORD(parse_keyword_else), symbol_else_continuation};
    switch (token1.keyword) {
        case parse_keyword_else: {
            return &else_cont;
        }
        default: { return &empty; }
    }
}

RESOLVE(else_continuation) {
    P elseif = {symbol_if_clause, symbol_else_clause};
    P elseonly = {parse_token_type_end, symbol_job_list};

    switch (token1.keyword) {
        case parse_keyword_if: {
            return &elseif;
        }
        default: { return &elseonly; }
    }
}

RESOLVE_ONLY(switch_statement) = {
    KEYWORD(parse_keyword_switch), symbol_argument,    parse_token_type_end,
    symbol_case_item_list,         symbol_end_command, symbol_arguments_or_redirections_list};

RESOLVE(case_item_list) {
    P empty = {};
    P case_item = {symbol_case_item, symbol_case_item_list};
    P blank_line = {parse_token_type_end, symbol_case_item_list};
    if (token1.keyword == parse_keyword_case)
        return &case_item;
    else if (token1.type == parse_token_type_end)
        return &blank_line;
    else
        return &empty;
}

RESOLVE_ONLY(case_item) = {KEYWORD(parse_keyword_case), symbol_argument_list, parse_token_type_end,
                           symbol_job_list};

RESOLVE(andor_job_list) {
    P list_end = {};
    P andor_job = {symbol_job, symbol_andor_job_list};
    P empty_line = {parse_token_type_end, symbol_andor_job_list};

    if (token1.type == parse_token_type_end) {
        return &empty_line;
    } else if (token1.keyword == parse_keyword_and || token1.keyword == parse_keyword_or) {
        // Check that the argument to and/or is a string that's not help. Otherwise it's either 'and
        // --help' or a naked 'and', and not part of this list.
        if (token2.type == parse_token_type_string && !token2.is_help_argument) {
            return &andor_job;
        }
    }
    // All other cases end the list.
    return &list_end;
}

RESOLVE(argument_list) {
    P empty = {};
    P arg = {symbol_argument, symbol_argument_list};
    switch (token1.type) {
        case parse_token_type_string: {
            return &arg;
        }
        default: { return &empty; }
    }
}

RESOLVE(freestanding_argument_list) {
    P empty = {};
    P arg = {symbol_argument, symbol_freestanding_argument_list};
    P semicolon = {parse_token_type_end, symbol_freestanding_argument_list};

    switch (token1.type) {
        case parse_token_type_string: {
            return &arg;
        }
        case parse_token_type_end: {
            return &semicolon;
        }
        default: { return &empty; }
    }
}

RESOLVE_ONLY(block_statement) = {symbol_block_header, symbol_job_list, symbol_end_command,
                                 symbol_arguments_or_redirections_list};

RESOLVE(block_header) {
    P forh = {symbol_for_header};
    P whileh = {symbol_while_header};
    P funch = {symbol_function_header};
    P beginh = {symbol_begin_header};

    switch (token1.keyword) {
        case parse_keyword_for: {
            return &forh;
        }
        case parse_keyword_while: {
            return &whileh;
        }
        case parse_keyword_function: {
            return &funch;
        }
        case parse_keyword_begin: {
            return &beginh;
        }
        default: { return NO_PRODUCTION; }
    }
}

RESOLVE_ONLY(for_header) = {KEYWORD(parse_keyword_for), parse_token_type_string,
                            KEYWORD(parse_keyword_in), symbol_argument_list, parse_token_type_end};
RESOLVE_ONLY(while_header) = {KEYWORD(parse_keyword_while), symbol_job, parse_token_type_end,
                              symbol_andor_job_list};
RESOLVE_ONLY(begin_header) = {KEYWORD(parse_keyword_begin)};
RESOLVE_ONLY(function_header) = {KEYWORD(parse_keyword_function), symbol_argument,
                                 symbol_argument_list, parse_token_type_end};

// A boolean statement is AND or OR or NOT.
RESOLVE(boolean_statement) {
    P ands = {KEYWORD(parse_keyword_and), symbol_statement};
    P ors = {KEYWORD(parse_keyword_or), symbol_statement};
    P nots = {KEYWORD(parse_keyword_not), symbol_statement};

    switch (token1.keyword) {
        case parse_keyword_and: {
            *out_tag = parse_bool_and;
            return &ands;
        }
        case parse_keyword_or: {
            *out_tag = parse_bool_or;
            return &ors;
        }
        case parse_keyword_not: {
            *out_tag = parse_bool_not;
            return &nots;
        }
        default: { return NO_PRODUCTION; }
    }
}

RESOLVE(decorated_statement) {
    P plains = {symbol_plain_statement};
    P cmds = {KEYWORD(parse_keyword_command), symbol_plain_statement};
    P builtins = {KEYWORD(parse_keyword_builtin), symbol_plain_statement};
    P execs = {KEYWORD(parse_keyword_exec), symbol_plain_statement};

    // If this is e.g. 'command --help' then the command is 'command' and not a decoration. If the
    // second token is not a string, then this is a naked 'command' and we should execute it as
    // undecorated.
    if (token2.type != parse_token_type_string || token2.has_dash_prefix) {
        return &plains;
    }

    switch (token1.keyword) {
        case parse_keyword_command: {
            *out_tag = parse_statement_decoration_command;
            return &cmds;
        }
        case parse_keyword_builtin: {
            *out_tag = parse_statement_decoration_builtin;
            return &builtins;
        }
        case parse_keyword_exec: {
            *out_tag = parse_statement_decoration_exec;
            return &execs;
        }
        default: {
            *out_tag = parse_statement_decoration_none;
            return &plains;
        }
    }
}

RESOLVE_ONLY(plain_statement) = {parse_token_type_string, symbol_arguments_or_redirections_list};

RESOLVE(arguments_or_redirections_list) {
    P empty = {};
    P value = {symbol_argument_or_redirection, symbol_arguments_or_redirections_list};
    switch (token1.type) {
        case parse_token_type_string:
        case parse_token_type_redirection: {
            return &value;
        }
        default: { return &empty; }
    }
}

RESOLVE(argument_or_redirection) {
    P arg = {symbol_argument};
    P redir = {symbol_redirection};
    switch (token1.type) {
        case parse_token_type_string: {
            return &arg;
        }
        case parse_token_type_redirection: {
            return &redir;
        }
        default: { return NO_PRODUCTION; }
    }
}

RESOLVE_ONLY(argument) = {parse_token_type_string};
RESOLVE_ONLY(redirection) = {parse_token_type_redirection, parse_token_type_string};

RESOLVE(optional_background) {
    P empty = {};
    P background = {parse_token_type_background};
    switch (token1.type) {
        case parse_token_type_background: {
            *out_tag = parse_background;
            return &background;
        }
        default: {
            *out_tag = parse_no_background;
            return &empty;
        }
    }
}

RESOLVE_ONLY(end_command) = {KEYWORD(parse_keyword_end)};

#define TEST(sym)                 \
    case (symbol_##sym):          \
        resolver = resolve_##sym; \
        break;
const production_t *parse_productions::production_for_token(parse_token_type_t node_type,
                                                            const parse_token_t &input1,
                                                            const parse_token_t &input2,
                                                            parse_node_tag_t *out_tag) {
    const bool log_it = false;
    if (log_it) {
        fprintf(stderr, "Resolving production for %ls with input token <%ls>\n",
                token_type_description(node_type), input1.describe().c_str());
    }

    // Fetch the function to resolve the list of productions.
    const production_t *(*resolver)(const parse_token_t &input1, const parse_token_t &input2,
                                    parse_node_tag_t *out_tag) = NULL;
    switch (node_type) {
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
        TEST(andor_job_list)
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
        case parse_token_type_terminate: {
            fprintf(stderr, "Terminal token type %ls passed to %s\n",
                    token_type_description(node_type), __FUNCTION__);
            PARSER_DIE();
            break;
        }
        case parse_special_type_parse_error:
        case parse_special_type_tokenizer_error:
        case parse_special_type_comment: {
            fprintf(stderr, "Special type %ls passed to %s\n", token_type_description(node_type),
                    __FUNCTION__);
            PARSER_DIE();
            break;
        }
        case token_type_invalid: {
            fprintf(stderr, "token_type_invalid passed to %s\n", __FUNCTION__);
            PARSER_DIE();
            break;
        }
    }
    PARSE_ASSERT(resolver != NULL);

    const production_t *result = resolver(input1, input2, out_tag);
    if (result == NULL) {
        if (log_it) {
            fprintf(stderr, "Node type '%ls' has no production for input '%ls' (in %s)\n",
                    token_type_description(node_type), input1.describe().c_str(), __FUNCTION__);
        }
    }

    return result;
}
