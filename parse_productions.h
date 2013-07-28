/**\file parse_tree.h

    Programmatic representation of fish code.
*/

#ifndef FISH_PARSE_TREE_CONSTRUCTION_H
#define FISH_PARSE_TREE_CONSTRUCTION_H

#include "parse_tree.h"

namespace parse_productions
{

#define MAX_PRODUCTIONS 5
#define MAX_SYMBOLS_PER_PRODUCTION 5


typedef uint32_t production_tag_t;

/* A production is an array of unsigned char. Symbols are encoded directly as their symbol value. Keywords are encoded with an offset of LAST_TOKEN_OR_SYMBOL + 1. So essentially we glom together keywords and symbols. */
typedef uint8_t production_element_t;

/* An index into a production option list */
typedef uint8_t production_option_idx_t;

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


inline bool production_element_is_valid(production_element_t elem)
{
    return elem != token_type_invalid;
}

typedef production_element_t const production_t[MAX_SYMBOLS_PER_PRODUCTION];

typedef production_t production_options_t[MAX_PRODUCTIONS];

#define PRODUCE_KEYWORD(x) ((x) + LAST_TOKEN_OR_SYMBOL + 1)

const production_t *production_for_token(parse_token_type_t node_type, parse_token_type_t input_type, parse_keyword_t input_keyword, production_option_idx_t *out_idx, production_tag_t *out_tag);

}


#endif
