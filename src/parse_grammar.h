// Programmatic representation of fish grammar
#ifndef FISH_PARSE_GRAMMAR_H
#define FISH_PARSE_GRAMMAR_H

#include "parse_constants.h"
#include "tokenizer.h"

namespace grammar {
using production_element_t = uint8_t;

// Forward declarations.
#define ELEM(T) struct T;
#include "parse_grammar_elements.inc"

// A production is a sequence of production elements.
template <int WHICH, uint8_t count = 0>
struct production_t {
    production_element_t elems[count];
};

template <int TOKEN>
struct prim {};

using tok_end = prim<TOK_END>;
using tok_string = prim<TOK_STRING>;

template <int WHICH>
struct keyword {};

// A producer holds various productions.
template <typename T>
struct producer {};

// Empty produces nothing.
struct empty : public producer<production_t<0>> {};

// Not sure if we need this.
template <class A>
struct single {};

template <class A>
using produces_single = producer<single<A>>;

// Alternative represents a choice.
template <class A1, class A2, class A3 = empty, class A4 = empty, class A5 = empty>
struct alternative {};

template <class... Args>
using produces_alternative = producer<alternative<Args...>>;

// Sequence represents a list of productions.
template <class A1, class A2, class A3 = empty, class A4 = empty, class A5 = empty,
          class A6 = empty>
struct seq {};

template <class... Args>
using produces_sequence = producer<seq<Args...>>;

// Following are the grammar productions.
#define DEF(T) struct T : public

// A job_list is a list of jobs, separated by semicolons or newlines
DEF(job_list)
produces_alternative<empty,               //
                     seq<job, job_list>,  //
                     seq<job, job_list>>{};

// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases
// like if statements, where we require a command). To represent "non-empty", we require a
// statement, followed by a possibly empty job_continuation, and then optionally a background
// specifier '&'
DEF(job) produces_sequence<statement, job_continuation, optional_background>{};

DEF(job_continuation)
produces_alternative<empty,  //
                     seq<prim<TOK_PIPE>, statement, job_continuation>>{};

// A statement is a normal command, or an if / while / and etc
DEF(statement)
produces_alternative<boolean_statement,  //
                     block_statement,    //
                     if_statement,       //
                     switch_statement,   //
                     decorated_statement>{};

// A block is a conditional, loop, or begin/end
DEF(if_statement)
produces_sequence<if_clause, else_clause, end_command, arguments_or_redirections_list>{};

DEF(if_clause)
produces_sequence<keyword<parse_keyword_if>, job, tok_end, andor_job_list, job_list>{};

DEF(else_clause) produces_alternative<empty, seq<keyword<parse_keyword_else>, else_continuation>>{};
DEF(else_continuation)
produces_alternative<seq<if_clause, else_clause>,  //
                     seq<tok_end, job_list>>{};

DEF(switch_statement)
produces_sequence<keyword<parse_keyword_switch>, argument, tok_end, case_item_list, end_command,
                  arguments_or_redirections_list>{};

DEF(case_item_list)
produces_alternative<empty,                           //
                     seq<case_item, case_item_list>,  //
                     seq<tok_end, case_item_list>>{};

DEF(case_item) produces_sequence<keyword<parse_keyword_case>, argument_list, tok_end, job_list>{};

DEF(block_statement)
produces_sequence<block_header, job_list, end_command, arguments_or_redirections_list>{};
DEF(block_header) produces_alternative<for_header, while_header, function_header, begin_header>{};

DEF(for_header)
produces_sequence<keyword<parse_keyword_for>, tok_string, keyword<parse_keyword_in>, argument_list,
                  tok_end>{};

DEF(while_header) produces_sequence<keyword<parse_keyword_while>, job, tok_end, andor_job_list>{};

struct begin_header : produces_single<keyword<parse_keyword_begin>> {};

// Functions take arguments, and require at least one (the name). No redirections allowed.
DEF(function_header)
produces_sequence<keyword<parse_keyword_function>, argument, argument_list, tok_end>{};

//  A boolean statement is AND or OR or NOT
DEF(boolean_statement)
produces_alternative<seq<keyword<parse_keyword_and>, statement>,  //
                     seq<keyword<parse_keyword_or>, statement>,   //
                     seq<keyword<parse_keyword_not>, statement>>{};
// An andor_job_list is zero or more job lists, where each starts with an `and` or `or` boolean
// statement.
DEF(andor_job_list)
produces_alternative<empty,                     //
                     seq<job, andor_job_list>,  //
                     seq<tok_end, andor_job_list>>{};

// A decorated_statement is a command with a list of arguments_or_redirections, possibly with
// "builtin" or "command" or "exec"
DEF(decorated_statement)
produces_alternative<plain_statement,                                       //
                     seq<keyword<parse_keyword_command>, plain_statement>,  //
                     seq<keyword<parse_keyword_builtin>, plain_statement>,  //
                     seq<keyword<parse_keyword_exec>, plain_statement>>{};

DEF(plain_statement) produces_sequence<tok_string, arguments_or_redirections_list>{};

DEF(argument_list) produces_alternative<empty, seq<argument, argument_list>>{};
DEF(arguments_or_redirections_list)
produces_alternative<empty, seq<argument_or_redirection, arguments_or_redirections_list>>{};

DEF(argument_or_redirection) produces_alternative<argument, redirection>{};
DEF(argument) produces_single<tok_string>{};
DEF(optional_background) produces_alternative<empty, prim<TOK_BACKGROUND>>{};
DEF(end_command) produces_single<keyword<parse_keyword_end>>{};

// A freestanding_argument_list is equivalent to a normal argument list, except it may contain
// TOK_END (newlines, and even semicolons, for historical reasons)
DEF(freestanding_argument_list)
produces_alternative<empty,                                      //
                     seq<argument, freestanding_argument_list>,  //
                     seq<tok_end, freestanding_argument_list>>{};
}
#endif
