/** \file parser_keywords.c

Functions having to do with parser keywords, like testing if a function is a block command.
*/
#include "fallback.h" // IWYU pragma: keep
#include "common.h"
#include "parser_keywords.h"

bool parser_keywords_skip_arguments(const wcstring &cmd)
{
    return contains(cmd,
                    L"else",
                    L"begin");
}


bool parser_keywords_is_subcommand(const wcstring &cmd)
{

    return parser_keywords_skip_arguments(cmd) ||
           contains(cmd,
                    L"command",
                    L"builtin",
                    L"while",
                    L"exec",
                    L"if",
                    L"and",
                    L"or",
                    L"not");

}

bool parser_keywords_is_block(const wcstring &word)
{
    return contains(word,
                    L"for",
                    L"while",
                    L"if",
                    L"function",
                    L"switch",
                    L"begin");
}

bool parser_keywords_is_reserved(const wcstring &word)
{
    return parser_keywords_is_block(word) ||
           parser_keywords_is_subcommand(word) ||
           contains(word,
                    L"end",
                    L"case",
                    L"else",
                    L"return",
                    L"continue",
                    L"break");
}

