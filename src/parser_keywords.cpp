// Functions having to do with parser keywords, like testing if a function is a block command.
#include "config.h"  // IWYU pragma: keep

#include <string>
#include <unordered_set>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "parser_keywords.h"

bool parser_keywords_skip_arguments(const wcstring &cmd) {
    static const wcstring el = L"else";
    static const wcstring beg = L"begin";
    return cmd == el || cmd == beg;
}

static const std::unordered_set<wcstring> subcommand_keywords = {L"command", L"builtin", L"while", L"exec",
                                                     L"if",      L"and",     L"or",    L"not"};
bool parser_keywords_is_subcommand(const wcstring &cmd) {
    return parser_keywords_skip_arguments(cmd) || contains(subcommand_keywords, cmd);
}

static const std::unordered_set<wcstring> block_keywords = {L"for",      L"while",  L"if",
                                                L"function", L"switch", L"begin"};
bool parser_keywords_is_block(const wcstring &word) { return contains(block_keywords, word); }

static const std::unordered_set<wcstring> reserved_keywords = {L"end",      L"case",   L"else",     L"return",
                                                   L"continue", L"break",  L"argparse", L"read",
                                                   L"set",      L"status", L"test",     L"["};
bool parser_keywords_is_reserved(const wcstring &word) {
    return parser_keywords_is_block(word) || parser_keywords_is_subcommand(word) ||
           contains(reserved_keywords, word);
}
