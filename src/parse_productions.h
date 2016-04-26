/**\file parse_tree.h

    Programmatic representation of fish code.
*/
#ifndef FISH_PARSE_TREE_CONSTRUCTION_H
#define FISH_PARSE_TREE_CONSTRUCTION_H

#include <sys/types.h>
#include <stdbool.h>

#include "parse_constants.h"

struct parse_token_t;

namespace parse_productions
{

#define MAX_SYMBOLS_PER_PRODUCTION 6

/* A production is an array of unsigned char. Symbols are encoded directly as their symbol value. Keywords are encoded with an offset of LAST_TOKEN_OR_SYMBOL + 1. So essentially we glom together keywords and symbols. */
typedef uint8_t production_element_t;
typedef production_element_t const production_t[MAX_SYMBOLS_PER_PRODUCTION];

/* Resolve the type from a production element */
inline parse_token_type_t production_element_type(production_element_t elem)
{
    if (elem > LAST_TOKEN_OR_SYMBOL)
    {
        return parse_token_type_string;
    }
    else
    {
        return static_cast<parse_token_type_t>(elem);
    }
}

/* Resolve the keyword from a production element */
inline parse_keyword_t production_element_keyword(production_element_t elem)
{
    if (elem > LAST_TOKEN_OR_SYMBOL)
    {
        // First keyword is LAST_TOKEN_OR_SYMBOL + 1
        return static_cast<parse_keyword_t>(elem - LAST_TOKEN_OR_SYMBOL - 1);
    }
    else
    {
        return parse_keyword_none;
    }
}

/* Check if an element is valid */
inline bool production_element_is_valid(production_element_t elem)
{
    return elem != token_type_invalid;
}

/* Fetch a production. We are passed two input tokens. The first input token is guaranteed to not be invalid; the second token may be invalid if there's no more tokens. We may also set flags. */
const production_t *production_for_token(parse_token_type_t node_type, const parse_token_t &input1, const parse_token_t &input2, uint8_t *out_tag);

}


#endif
