// Programmatic representation of fish grammar
#ifndef FISH_PARSE_GRAMMAR_H
#define FISH_PARSE_GRAMMAR_H

#include "parse_constants.h"
#include "tokenizer.h"
#include <array>

struct parse_token_t;
typedef uint8_t parse_node_tag_t;

using parse_node_tag_t = uint8_t;
struct parse_token_t;
namespace grammar {

using production_element_t = uint8_t;

// Define primitive types.
template <enum parse_token_type_t Token>
struct primitive {
    using type_tuple = std::tuple<>;
    static constexpr parse_token_type_t token = Token;
    static constexpr production_element_t element() { return Token; }
};

using tok_end = primitive<parse_token_type_end>;
using tok_string = primitive<parse_token_type_string>;
using tok_pipe = primitive<parse_token_type_pipe>;
using tok_background = primitive<parse_token_type_background>;
using tok_redirection = primitive<parse_token_type_redirection>;

// Define keyword types.
template <parse_keyword_t Keyword>
struct keyword {
    using type_tuple = std::tuple<>;
    static constexpr production_element_t element() {
        // Convert a parse_keyword_t enum to a production_element_t enum.
        return Keyword + LAST_TOKEN_OR_SYMBOL + 1;
    }
};

// Forward declare all the symbol types.
#define ELEM(T) struct T;
#include "parse_grammar_elements.inc"


// A production is a sequence of production elements.
// +1 to hold the terminating token_type_invalid
template <size_t Count>
using production_t = std::array<const production_element_t, Count + 1>;

// This is an ugly hack to avoid ODR violations
// Given some type, return a pointer to its production.
template <typename T>
const production_element_t *production_for() {
    static constexpr auto prod = T::production;
    return prod.data();
}

// Get some production element.
template <typename T>
constexpr production_element_t element() {
    return T::element();
}

// Partial specialization hack.
#define ELEM(T)                                   \
    template <>                                   \
    constexpr production_element_t element<T>() { \
        return symbol_##T;                        \
    }
#include "parse_grammar_elements.inc"

// Empty produces nothing.
struct empty {
    static constexpr production_t<0> production = {{token_type_invalid}};
    static const production_element_t *resolve(const parse_token_t &, const parse_token_t &,
                                               parse_node_tag_t *) {
        return production_for<empty>();
    }
};

// Sequence represents a list of (at least two) productions.
template <class T0, class... Ts>
struct seq {
    static constexpr production_t<1 + sizeof...(Ts)> production = {
        {element<T0>(), element<Ts>()..., token_type_invalid}};

    using type_tuple = std::tuple<T0, Ts...>;

    static const production_element_t *resolve(const parse_token_t &, const parse_token_t &,
                                               parse_node_tag_t *) {
        return production_for<seq>();
    }
};

template <class... Args>
using produces_sequence = seq<Args...>;

// Ergonomic way to create a production for a single element.
template <class T>
using single = seq<T>;

template <class T>
using produces_single = single<T>;

// Alternative represents a choice.
struct alternative {
};

// Following are the grammar productions.
#define BODY(T) static constexpr parse_token_type_t token = symbol_##T;

#define DEF(T) struct T : public

#define DEF_ALT(T) struct T : public alternative
#define ALT_BODY(T)                                                                          \
    BODY(T)                                                                                  \
    using type_tuple = std::tuple<>;                                                         \
    static const production_element_t *resolve(const parse_token_t &, const parse_token_t &, \
                                               parse_node_tag_t *);

// A job_list is a list of jobs, separated by semicolons or newlines
DEF_ALT(job_list) {
    using normal = seq<job, job_list>;
    using empty_line = seq<tok_end, job_list>;
    using empty = grammar::empty;
    ALT_BODY(job_list);
};

// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases
// like if statements, where we require a command). To represent "non-empty", we require a
// statement, followed by a possibly empty job_continuation, and then optionally a background
// specifier '&'
DEF(job) produces_sequence<statement, job_continuation, optional_background>{BODY(job)};

DEF_ALT(job_continuation) {
    using piped = seq<tok_pipe, statement, job_continuation>;
    ALT_BODY(job_continuation);
};

// A statement is a normal command, or an if / while / and etc
DEF_ALT(statement) {
    using boolean = single<boolean_statement>;
    using block = single<block_statement>;
    using ifs = single<if_statement>;
    using switchs = single<switch_statement>;
    using decorated = single<decorated_statement>;
    ALT_BODY(statement);
};

// A block is a conditional, loop, or begin/end
DEF(if_statement)
produces_sequence<if_clause, else_clause, end_command, arguments_or_redirections_list>{
    BODY(if_statement)};

DEF(if_clause)
produces_sequence<keyword<parse_keyword_if>, job, tok_end, andor_job_list, job_list>{
    BODY(if_clause)};

DEF_ALT(else_clause) {
    using empty = grammar::empty;
    using else_cont = seq<keyword<parse_keyword_else>, else_continuation>;
    ALT_BODY(else_clause);
};

DEF_ALT(else_continuation) {
    using else_if = seq<if_clause, else_clause>;
    using else_only = seq<tok_end, job_list>;
    ALT_BODY(else_continuation);
};

DEF(switch_statement)
produces_sequence<keyword<parse_keyword_switch>, argument, tok_end, case_item_list, end_command,
                  arguments_or_redirections_list>{BODY(switch_statement)};

DEF_ALT(case_item_list) {
    using empty = grammar::empty;
    using case_items = seq<case_item, case_item_list>;
    using blank_line = seq<tok_end, case_item_list>;
    ALT_BODY(case_item_list);
};

DEF(case_item) produces_sequence<keyword<parse_keyword_case>, argument_list, tok_end, job_list> {
    BODY(case_item);
};

DEF(block_statement)
produces_sequence<block_header, job_list, end_command, arguments_or_redirections_list>{};

DEF_ALT(block_header) {
    using forh = single<for_header>;
    using whileh = single<while_header>;
    using funch = single<function_header>;
    using beginh = single<begin_header>;
    ALT_BODY(block_header);
};

DEF(for_header)
produces_sequence<keyword<parse_keyword_for>, tok_string, keyword<parse_keyword_in>, argument_list,
                  tok_end>{};

DEF(while_header)
produces_sequence<keyword<parse_keyword_while>, job, tok_end, andor_job_list>{BODY(while_header)};

DEF(begin_header) produces_single<keyword<parse_keyword_begin>>{BODY(begin_header)};

// Functions take arguments, and require at least one (the name). No redirections allowed.
DEF(function_header)
produces_sequence<keyword<parse_keyword_function>, argument, argument_list, tok_end>{};

//  A boolean statement is AND or OR or NOT
DEF_ALT(boolean_statement) {
    using ands = seq<keyword<parse_keyword_and>, statement>;
    using ors = seq<keyword<parse_keyword_or>, statement>;
    using nots = seq<keyword<parse_keyword_not>, statement>;
    ALT_BODY(boolean_statement);
};

// An andor_job_list is zero or more job lists, where each starts with an `and` or `or` boolean
// statement.
DEF_ALT(andor_job_list) {
    using empty = grammar::empty;
    using andor_job = seq<job, andor_job_list>;
    using empty_line = seq<tok_end, andor_job_list>;
    ALT_BODY(andor_job_list);
};

// A decorated_statement is a command with a list of arguments_or_redirections, possibly with
// "builtin" or "command" or "exec"
DEF_ALT(decorated_statement) {
    using plains = single<plain_statement>;
    using cmds = seq<keyword<parse_keyword_command>, plain_statement>;
    using builtins = seq<keyword<parse_keyword_builtin>, plain_statement>;
    using execs = seq<keyword<parse_keyword_exec>, plain_statement>;
    ALT_BODY(decorated_statement);
};

DEF(plain_statement)
produces_sequence<tok_string, arguments_or_redirections_list>{BODY(plain_statement)};

DEF_ALT(argument_list) {
    using empty = grammar::empty;
    using arg = seq<argument, argument_list>;
    ALT_BODY(argument_list);
};

DEF_ALT(arguments_or_redirections_list) {
    using empty = grammar::empty;
    using value = seq<argument_or_redirection, arguments_or_redirections_list>;
    ALT_BODY(arguments_or_redirections_list);
};

DEF_ALT(argument_or_redirection) {
    using arg = single<argument>;
    using redir = single<redirection>;
    ALT_BODY(argument_or_redirection);
};

DEF(argument) produces_single<tok_string>{BODY(argument)};
DEF(redirection) produces_sequence<tok_redirection, tok_string>{BODY(redirection)};

DEF_ALT(optional_background) {
    using empty = grammar::empty;
    using background = single<tok_background>;
    ALT_BODY(optional_background);
};

DEF(end_command) produces_single<keyword<parse_keyword_end>>{BODY(end_command)};

// A freestanding_argument_list is equivalent to a normal argument list, except it may contain
// TOK_END (newlines, and even semicolons, for historical reasons)
DEF_ALT(freestanding_argument_list) {
    using empty = grammar::empty;
    using arg = seq<argument, freestanding_argument_list>;
    using semicolon = seq<tok_end, freestanding_argument_list>;
    ALT_BODY(freestanding_argument_list);
};
}
#endif
