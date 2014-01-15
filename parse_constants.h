/**\file parse_constants.h

    Constants used in the programmatic representation of fish code.
*/

#ifndef fish_parse_constants_h
#define fish_parse_constants_h

#define PARSE_ASSERT(a) assert(a)
#define PARSER_DIE() do { fprintf(stderr, "Parser dying!\n"); exit_without_destructors(-1); } while (0)


enum parse_token_type_t
{
    token_type_invalid,

    // Non-terminal tokens
    symbol_job_list,
    symbol_job,
    symbol_job_continuation,
    symbol_statement,
    symbol_block_statement,
    symbol_block_header,
    symbol_for_header,
    symbol_while_header,
    symbol_begin_header,
    symbol_function_header,

    symbol_if_statement,
    symbol_if_clause,
    symbol_else_clause,
    symbol_else_continuation,

    symbol_switch_statement,
    symbol_case_item_list,
    symbol_case_item,

    symbol_boolean_statement,
    symbol_decorated_statement,
    symbol_plain_statement,
    symbol_arguments_or_redirections_list,
    symbol_argument_or_redirection,

    symbol_argument_list,

    symbol_argument,
    symbol_redirection,

    symbol_optional_background,

    symbol_end_command,

    // Terminal types
    parse_token_type_string,
    parse_token_type_pipe,
    parse_token_type_redirection,
    parse_token_type_background,
    parse_token_type_end,

    // Special terminal type that means no more tokens forthcoming
    parse_token_type_terminate,

    // Very special terminal types that don't appear in the production list
    parse_special_type_parse_error,
    parse_special_type_tokenizer_error,
    parse_special_type_comment,

    FIRST_TERMINAL_TYPE = parse_token_type_string,
    LAST_TERMINAL_TYPE = parse_token_type_terminate,

    LAST_TOKEN_OR_SYMBOL = parse_token_type_terminate,
    FIRST_PARSE_TOKEN_TYPE = parse_token_type_string
};

enum parse_keyword_t
{
    parse_keyword_none,
    parse_keyword_if,
    parse_keyword_else,
    parse_keyword_for,
    parse_keyword_in,
    parse_keyword_while,
    parse_keyword_begin,
    parse_keyword_function,
    parse_keyword_switch,
    parse_keyword_case,
    parse_keyword_end,
    parse_keyword_and,
    parse_keyword_or,
    parse_keyword_not,
    parse_keyword_command,
    parse_keyword_builtin,

    LAST_KEYWORD = parse_keyword_builtin
};

/* Statement decorations. This matches the order of productions in decorated_statement */
enum parse_statement_decoration_t
{
    parse_statement_decoration_none,
    parse_statement_decoration_command,
    parse_statement_decoration_builtin
};

/* Parse error code list */
enum parse_error_code_t
{
    parse_error_none,

    /* Matching values from enum parser_error */
    parse_error_syntax,
    parse_error_eval,
    parse_error_cmdsubst,

    parse_error_generic, // unclassified error types

    //tokenizer errors
    parse_error_tokenizer_unterminated_quote,
    parse_error_tokenizer_unterminated_subshell,
    parse_error_tokenizer_unterminated_escape,
    parse_error_tokenizer_other,

    parse_error_unbalancing_end, //end outside of block
    parse_error_unbalancing_else, //else outside of if
    parse_error_unbalancing_case, //case outside of switch

    parse_error_double_pipe, // foo || bar, has special error message
    parse_error_double_background // foo && bar, has special error message
};

enum
{
    PARSER_TEST_ERROR = 1,
    PARSER_TEST_INCOMPLETE = 2
};
typedef unsigned int parser_test_error_bits_t;


/** Maximum number of function calls. */
#define FISH_MAX_STACK_DEPTH 128

/** Error message on a function that calls itself immediately */
#define INFINITE_FUNC_RECURSION_ERR_MSG _( L"The function '%ls' calls itself immediately, which would result in an infinite loop.")


/** Error message on reaching maximum call stack depth */
#define CALL_STACK_LIMIT_EXCEEDED_ERR_MSG _( L"The function call stack limit has been exceeded. Do you have an accidental infinite loop?")

/** Error message when a non-string token is found when expecting a command name */
#define CMD_OR_ERR_MSG _( L"Expected a command, but instead found a pipe. Did you mean 'COMMAND; or COMMAND'? See the help section for the 'or' builtin command by typing 'help or'.")

/** Error message when a non-string token is found when expecting a command name */
#define CMD_AND_ERR_MSG _( L"Expected a command, but instead found a '&'. Did you mean 'COMMAND; and COMMAND'? See the help section for the 'and' builtin command by typing 'help and'.")

/** Error message when encountering an illegal command name */
#define ILLEGAL_CMD_ERR_MSG _( L"Illegal command name '%ls'")

/** Error message when encountering an illegal file descriptor */
#define ILLEGAL_FD_ERR_MSG _( L"Illegal file descriptor in redirection '%ls'")

/** Error message for wildcards with no matches */
#define WILDCARD_ERR_MSG _( L"No matches for wildcard '%ls'.")

/** Error when using break outside of loop */
#define INVALID_BREAK_ERR_MSG _( L"break command while not inside of loop" )

/** Error when using continue outside of loop */
#define INVALID_CONTINUE_ERR_MSG _( L"continue command while not inside of loop" )

/** Error when using return builtin outside of function definition */
#define INVALID_RETURN_ERR_MSG _( L"'return' builtin command outside of function definition" )

/** Error message for Posix-style assignment: foo=bar */
#define COMMAND_ASSIGN_ERR_MSG _( L"Unknown command '%ls'. Did you mean 'set %ls %ls'? See the help section on the set command by typing 'help set'.")

/**
   While block description
*/
#define WHILE_BLOCK N_( L"'while' block" )

/**
   For block description
*/
#define FOR_BLOCK N_( L"'for' block" )

/**
   Breakpoint block
*/
#define BREAKPOINT_BLOCK N_( L"Block created by breakpoint" )



/**
   If block description
*/
#define IF_BLOCK N_( L"'if' conditional block" )


/**
   Function definition block description
*/
#define FUNCTION_DEF_BLOCK N_( L"function definition block" )


/**
   Function invocation block description
*/
#define FUNCTION_CALL_BLOCK N_( L"function invocation block" )

/**
   Function invocation block description
*/
#define FUNCTION_CALL_NO_SHADOW_BLOCK N_( L"function invocation block with no variable shadowing" )


/**
   Switch block description
*/
#define SWITCH_BLOCK N_( L"'switch' block" )


/**
   Fake block description
*/
#define FAKE_BLOCK N_( L"unexecutable block" )


/**
   Top block description
*/
#define TOP_BLOCK N_( L"global root block" )


/**
   Command substitution block description
*/
#define SUBST_BLOCK N_( L"command substitution block" )


/**
   Begin block description
*/
#define BEGIN_BLOCK N_( L"'begin' unconditional block" )


/**
   Source block description
*/
#define SOURCE_BLOCK N_( L"Block created by the . builtin" )

/**
   Source block description
*/
#define EVENT_BLOCK N_( L"event handler block" )


/**
   Unknown block description
*/
#define UNKNOWN_BLOCK N_( L"unknown/invalid block" )



#endif
