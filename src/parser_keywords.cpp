// Functions having to do with parser keywords, like testing if a function is a block command.
#include "config.h"  // IWYU pragma: keep

#include "parser_keywords.h"

#include <string>
#include <unordered_set>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep

using string_set_t = std::unordered_set<wcstring>;

static const wcstring skip_keywords[]{
    L"else",
    L"begin",
};

static const wcstring subcommand_keywords[]{L"command", L"builtin", L"while", L"exec", L"if",
                                            L"and",     L"or",      L"not",   L"time", L"begin"};

static const string_set_t block_keywords = {L"for",      L"while",  L"if",
                                            L"function", L"switch", L"begin"};

static const wcstring reserved_keywords[] = {L"end",      L"case",   L"else",     L"return",
                                             L"continue", L"break",  L"argparse", L"read",
                                             L"set",      L"status", L"test",     L"["};

// The lists above are purposely implemented separately from the logic below, so that future
// maintainers may assume the contents of the list based off their names, and not off what the
// functions below require them to contain.

static size_t list_max_length(const string_set_t &list) {
    size_t result = 0;
    for (const auto &w : list) {
        if (w.length() > result) {
            result = w.length();
        }
    }
    return result;
}

bool parser_keywords_skip_arguments(const wcstring &cmd) {
    return cmd == skip_keywords[0] || cmd == skip_keywords[1];
}

bool parser_keywords_is_subcommand(const wcstring &cmd) {
    const static string_set_t search_list = ([]() {
        string_set_t results;
        results.insert(std::begin(subcommand_keywords), std::end(subcommand_keywords));
        results.insert(std::begin(skip_keywords), std::end(skip_keywords));
        return results;
    })();

    const static auto max_len = list_max_length(search_list);
    const static auto not_found = search_list.end();

    // Everything above is executed only at startup, this is the actual optimized search routine:
    return cmd.length() <= max_len && search_list.find(cmd) != not_found;
}

bool parser_keywords_is_block(const wcstring &word) {
    const static auto max_len = list_max_length(block_keywords);
    const static auto not_found = block_keywords.end();

    // Everything above is executed only at startup, this is the actual optimized search routine:
    return word.length() <= max_len && block_keywords.find(word) != not_found;
}

bool parser_keywords_is_reserved(const wcstring &word) {
    const static string_set_t search_list = ([]() {
        string_set_t results;
        results.insert(std::begin(subcommand_keywords), std::end(subcommand_keywords));
        results.insert(std::begin(skip_keywords), std::end(skip_keywords));
        results.insert(std::begin(block_keywords), std::end(block_keywords));
        results.insert(std::begin(reserved_keywords), std::end(reserved_keywords));
        return results;
    })();
    const static size_t max_len = list_max_length(search_list);
    return word.length() <= max_len && search_list.count(word) > 0;
}
