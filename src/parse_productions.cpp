#include "config.h"  // IWYU pragma: keep

#include <stdio.h>

#include "common.h"
#include "parse_constants.h"
#include "parse_grammar.h"
#include "parse_productions.h"
#include "parse_tree.h"

using namespace parse_productions;
using namespace grammar;

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

#define RESOLVE(SYM)                          \
    const production_element_t *SYM::resolve( \
        const parse_token_t &token1, const parse_token_t &token2, parse_node_tag_t *out_tag)

/// A job_list is a list of jobs, separated by semicolons or newlines.
RESOLVE(job_list) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.type) {
        case parse_token_type_string: {
            // Some keywords are special.
            switch (token1.keyword) {
                case parse_keyword_end:
                case parse_keyword_else:
                case parse_keyword_case: {
                    return production_for<empty>();  // end this job list
                }
                default: {
                    return production_for<normal>();  // normal string
                }
            }
        }
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background: {
            return production_for<normal>();
        }
        case parse_token_type_end: {
            return production_for<empty_line>();
        }
        case parse_token_type_terminate: {
            return production_for<empty>();  // no more commands, just transition to empty
        }
        default: { return NO_PRODUCTION; }
    }
}

// A job decorator is AND or OR
RESOLVE(job_decorator) {
    UNUSED(token2);

    switch (token1.keyword) {
        case parse_keyword_and: {
            *out_tag = parse_bool_and;
            return production_for<ands>();
        }
        case parse_keyword_or: {
            *out_tag = parse_bool_or;
            return production_for<ors>();
        }
        default: {
            *out_tag = parse_bool_none;
            return production_for<empty>();
        }
    }
}

RESOLVE(job_conjunction_continuation) {
    UNUSED(token2);
    UNUSED(out_tag);
    switch (token1.type) {
        case parse_token_type_andand:
            *out_tag = parse_bool_and;
            return production_for<andands>();
        case parse_token_type_oror:
            *out_tag = parse_bool_or;
            return production_for<orors>();
        default:
            return production_for<empty>();
    }
}

RESOLVE(job_continuation) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.type) {
        case parse_token_type_pipe: {
            return production_for<piped>();  // pipe, continuation
        }
        default: {
            return production_for<empty>();  // not a pipe, no job continuation
        }
    }
}

// A statement is a normal command, or an if / while / and etc.
RESOLVE(statement) {
    UNUSED(out_tag);

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
            return production_for<decorated>();
        } else if (token1.keyword != parse_keyword_function && token2.has_dash_prefix) {
            return production_for<decorated>();
        }

        // Likewise if the next token doesn't look like an argument at all. This corresponds to e.g.
        // a "naked if".
        bool naked_invocation_invokes_help =
            (token1.keyword != parse_keyword_begin && token1.keyword != parse_keyword_end);
        if (naked_invocation_invokes_help &&
            (token2.type == parse_token_type_end || token2.type == parse_token_type_terminate)) {
            return production_for<decorated>();
        }
    }

    switch (token1.type) {
        case parse_token_type_string: {
            switch (token1.keyword) {
                case parse_keyword_not:
                case parse_keyword_exclam: {
                    return production_for<nots>();
                }
                case parse_keyword_for:
                case parse_keyword_while:
                case parse_keyword_function:
                case parse_keyword_begin: {
                    return production_for<block>();
                }
                case parse_keyword_if: {
                    return production_for<ifs>();
                }
                case parse_keyword_else: {
                    return NO_PRODUCTION;
                }
                case parse_keyword_switch: {
                    return production_for<switchs>();
                }
                case parse_keyword_end: {
                    return NO_PRODUCTION;
                }
                // All other keywords fall through to decorated statement.
                default: { return production_for<decorated>(); }
            }
            break;
        }
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_terminate: {
            return NO_PRODUCTION;
        }
        default: { return NO_PRODUCTION; }
    }
}

RESOLVE(else_clause) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.keyword) {
        case parse_keyword_else: {
            return production_for<else_cont>();
        }
        default: { return production_for<empty>(); }
    }
}

RESOLVE(else_continuation) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.keyword) {
        case parse_keyword_if: {
            return production_for<else_if>();
        }
        default: { return production_for<else_only>(); }
    }
}

RESOLVE(case_item_list) {
    UNUSED(token2);
    UNUSED(out_tag);

    if (token1.keyword == parse_keyword_case)
        return production_for<case_items>();
    else if (token1.type == parse_token_type_end)
        return production_for<blank_line>();
    else
        return production_for<empty>();
}

RESOLVE(not_statement) {
    UNUSED(token2);
    UNUSED(out_tag);
    switch (token1.keyword) {
        case parse_keyword_not:
            return production_for<nots>();
        case parse_keyword_exclam:
            return production_for<exclams>();
        default:
            return NO_PRODUCTION;
    }
}

RESOLVE(andor_job_list) {
    UNUSED(out_tag);

    if (token1.type == parse_token_type_end) {
        return production_for<empty_line>();
    } else if (token1.keyword == parse_keyword_and || token1.keyword == parse_keyword_or) {
        // Check that the argument to and/or is a string that's not help. Otherwise it's either 'and
        // --help' or a naked 'and', and not part of this list.
        if (token2.type == parse_token_type_string && !token2.is_help_argument) {
            return production_for<andor_job>();
        }
    }
    // All other cases end the list.
    return production_for<empty>();
}

RESOLVE(argument_list) {
    UNUSED(token2);
    UNUSED(out_tag);
    switch (token1.type) {
        case parse_token_type_string: {
            return production_for<arg>();
        }
        default: { return production_for<empty>(); }
    }
}

RESOLVE(freestanding_argument_list) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.type) {
        case parse_token_type_string: {
            return production_for<arg>();
        }
        case parse_token_type_end: {
            return production_for<semicolon>();
        }
        default: { return production_for<empty>(); }
    }
}

RESOLVE(block_header) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.keyword) {
        case parse_keyword_for: {
            return production_for<forh>();
        }
        case parse_keyword_while: {
            return production_for<whileh>();
        }
        case parse_keyword_function: {
            return production_for<funch>();
        }
        case parse_keyword_begin: {
            return production_for<beginh>();
        }
        default: { return NO_PRODUCTION; }
    }
}

RESOLVE(decorated_statement) {

    // If this is e.g. 'command --help' then the command is 'command' and not a decoration. If the
    // second token is not a string, then this is a naked 'command' and we should execute it as
    // undecorated.
    if (token2.type != parse_token_type_string || token2.has_dash_prefix) {
        return production_for<plains>();
    }

    switch (token1.keyword) {
        case parse_keyword_command: {
            *out_tag = parse_statement_decoration_command;
            return production_for<cmds>();
        }
        case parse_keyword_builtin: {
            *out_tag = parse_statement_decoration_builtin;
            return production_for<builtins>();
        }
        case parse_keyword_exec: {
            *out_tag = parse_statement_decoration_exec;
            return production_for<execs>();
        }
        default: {
            *out_tag = parse_statement_decoration_none;
            return production_for<plains>();
        }
    }
}

RESOLVE(arguments_or_redirections_list) {
    UNUSED(token2);
    UNUSED(out_tag);

    switch (token1.type) {
        case parse_token_type_string:
            return production_for<arg>();
        case parse_token_type_redirection:
            return production_for<redir>();
        default:
            return production_for<empty>();
    }
}

RESOLVE(optional_newlines) {
    UNUSED(token2);
    UNUSED(out_tag);
    if (token1.is_newline) return production_for<newlines>();
    return production_for<empty>();
}

RESOLVE(optional_background) {
    UNUSED(token2);

    switch (token1.type) {
        case parse_token_type_background: {
            *out_tag = parse_background;
            return production_for<background>();
        }
        default: {
            *out_tag = parse_no_background;
            return production_for<empty>();
        }
    }
}


const production_element_t *parse_productions::production_for_token(parse_token_type_t node_type,
                                                                    const parse_token_t &input1,
                                                                    const parse_token_t &input2,
                                                                    parse_node_tag_t *out_tag) {
    // this is **extremely** chatty
    debug(6, "Resolving production for %ls with input token <%ls>",
          token_type_description(node_type), input1.describe().c_str());

    // Fetch the function to resolve the list of productions.
    const production_element_t *(*resolver)(const parse_token_t &input1,  //!OCLINT(unused param)
                                            const parse_token_t &input2,  //!OCLINT(unused param)
                                            parse_node_tag_t *out_tag) =  //!OCLINT(unused param)
        NULL;
    switch (node_type) {
// Handle all of our grammar elements
#define ELEM(SYM)                \
    case (symbol_##SYM):         \
        resolver = SYM::resolve; \
        break;
#include "parse_grammar_elements.inc"

        // Everything else is an error.
        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_andand:
        case parse_token_type_oror:
        case parse_token_type_end:
        case parse_token_type_terminate: {
            debug(0, "Terminal token type %ls passed to %s", token_type_description(node_type),
                  __FUNCTION__);
            PARSER_DIE();
            break;
        }
        case parse_special_type_parse_error:
        case parse_special_type_tokenizer_error:
        case parse_special_type_comment: {
            debug(0, "Special type %ls passed to %s\n", token_type_description(node_type),
                  __FUNCTION__);
            PARSER_DIE();
            break;
        }
        case token_type_invalid: {
            debug(0, "token_type_invalid passed to %s", __FUNCTION__);
            PARSER_DIE();
            break;
        }
    }
    PARSE_ASSERT(resolver != NULL);

    const production_element_t *result = resolver(input1, input2, out_tag);
    if (result == NULL) {
        debug(5, "Node type '%ls' has no production for input '%ls' (in %s)",
              token_type_description(node_type), input1.describe().c_str(), __FUNCTION__);
    }

    return result;
}
