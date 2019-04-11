// Programmatic representation of fish grammar
#ifndef FISH_PARSE_GRAMMAR_H
#define FISH_PARSE_GRAMMAR_H

#include <array>
#include <tuple>
#include <type_traits>
#include "parse_constants.h"
#include "tokenizer.h"

struct parse_token_t;
typedef uint8_t parse_node_tag_t;

using parse_node_tag_t = uint8_t;
struct parse_token_t;
namespace grammar {

using production_element_t = uint8_t;

enum {
    // The maximum length of any seq production.
    MAX_PRODUCTION_LENGTH = 6
};

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
using tok_andand = primitive<parse_token_type_andand>;
using tok_oror = primitive<parse_token_type_oror>;

// Define keyword types.
template <parse_keyword_t Keyword>
struct keyword {
    using type_tuple = std::tuple<>;
    static constexpr parse_token_type_t token = parse_token_type_string;
    static constexpr production_element_t element() {
        // Convert a parse_keyword_t enum to a production_element_t enum.
        return Keyword + LAST_TOKEN_OR_SYMBOL + 1;
    }
};

// Define special types.
// Comments are not emitted as part of productions, but specially by the parser.
struct comment {
    using type_tuple = std::tuple<>;
    static constexpr parse_token_type_t token = parse_special_type_comment;
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

// Template goo.
namespace detail {
template <typename T, typename Tuple>
struct tuple_contains;

template <typename T>
struct tuple_contains<T, std::tuple<>> : std::false_type {};

template <typename T, typename U, typename... Ts>
struct tuple_contains<T, std::tuple<U, Ts...>> : tuple_contains<T, std::tuple<Ts...>> {};

template <typename T, typename... Ts>
struct tuple_contains<T, std::tuple<T, Ts...>> : std::true_type {};

struct void_type {
    using type = void;
};

// Support for checking whether the index N is valid for T::type_tuple.
template <size_t N, typename T>
static constexpr bool index_valid() {
    return N < std::tuple_size<typename T::type_tuple>::value;
}

// Get the Nth type of T::type_tuple.
template <size_t N, typename T>
using tuple_element = std::tuple_element<N, typename T::type_tuple>;

// Get the Nth type of T::type_tuple, or void if N is out of bounds.
template <size_t N, typename T>
using tuple_element_or_void =
    typename std::conditional<index_valid<N, T>(), tuple_element<N, T>, void_type>::type::type;

// Make a tuple by mapping the Nth item of a list of 'seq's.
template <size_t N, typename... Ts>
struct tuple_nther {
    // A tuple of the Nth types of tuples (or voids).
    using type = std::tuple<tuple_element_or_void<N, Ts>...>;
};

// Given a list of Options, each one a seq, check to see if any of them contain type Desired at
// index Index.
template <typename Desired, size_t Index, typename... Options>
inline constexpr bool type_possible() {
    using nths = typename tuple_nther<Index, Options...>::type;
    return tuple_contains<Desired, nths>::value;
}
}  // namespace detail

// Partial specialization hack.
#define ELEM(T)                                   \
    template <>                                   \
    constexpr production_element_t element<T>() { \
        return symbol_##T;                        \
    }
#include "parse_grammar_elements.inc"

// Empty produces nothing.
struct empty {
    using type_tuple = std::tuple<>;
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

    static_assert(1 + sizeof...(Ts) <= MAX_PRODUCTION_LENGTH, "MAX_PRODUCTION_LENGTH too small");

    using type_tuple = std::tuple<T0, Ts...>;

    template <typename Desired, size_t Index>
    static constexpr bool type_possible() {
        using element_t = detail::tuple_element_or_void<Index, seq>;
        return std::is_same<Desired, element_t>::value;
    }

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
struct alternative {};

// Following are the grammar productions.
#define BODY(T) static constexpr parse_token_type_t token = symbol_##T;

#define DEF(T) struct T : public

#define DEF_ALT(T) struct T : public alternative
#define ALT_BODY(T, ...)                                                                     \
    BODY(T)                                                                                  \
    using type_tuple = std::tuple<>;                                                         \
    template <typename Desired, size_t Index>                                                \
    static constexpr bool type_possible() {                                                  \
        return detail::type_possible<Desired, Index, __VA_ARGS__>();                         \
    }                                                                                        \
    static const production_element_t *resolve(const parse_token_t &, const parse_token_t &, \
                                               parse_node_tag_t *)

// A job_list is a list of job_conjunctions, separated by semicolons or newlines
DEF_ALT(job_list) {
    using normal = seq<job_decorator, job_conjunction, job_list>;
    using empty_line = seq<tok_end, job_list>;
    using empty = grammar::empty;
    ALT_BODY(job_list, normal, empty_line, empty);
};

// Job decorators are 'and' and 'or'. These apply to the whole job.
DEF_ALT(job_decorator) {
    using ands = single<keyword<parse_keyword_and>>;
    using ors = single<keyword<parse_keyword_or>>;
    using empty = grammar::empty;
    ALT_BODY(job_decorator, ands, ors, empty);
};

// A job_conjunction is a job followed by a continuation.
DEF(job_conjunction) produces_sequence<job, job_conjunction_continuation> {
    BODY(job_conjunction)
};

DEF_ALT(job_conjunction_continuation) {
    using andands = seq<tok_andand, optional_newlines, job_conjunction>;
    using orors = seq<tok_oror, optional_newlines, job_conjunction>;
    using empty = grammar::empty;
    ALT_BODY(job_conjunction_continuation, andands, orors, empty);
};

// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases
// like if statements, where we require a command). To represent "non-empty", we require a
// statement, followed by a possibly empty job_continuation, and then optionally a background
// specifier '&'
DEF(job) produces_sequence<statement, job_continuation, optional_background>{BODY(job)};

DEF_ALT(job_continuation) {
    using piped = seq<tok_pipe, optional_newlines, statement, job_continuation>;
    using empty = grammar::empty;
    ALT_BODY(job_continuation, piped, empty);
};

// A statement is a normal command, or an if / while / and etc
DEF_ALT(statement) {
    using nots = single<not_statement>;
    using block = single<block_statement>;
    using ifs = single<if_statement>;
    using switchs = single<switch_statement>;
    using decorated = single<decorated_statement>;
    ALT_BODY(statement, nots, block, ifs, switchs, decorated);
};

// A block is a conditional, loop, or begin/end
DEF(if_statement)
produces_sequence<if_clause, else_clause, end_command, arguments_or_redirections_list>{
    BODY(if_statement)};

DEF(if_clause)
produces_sequence<keyword<parse_keyword_if>, job_conjunction, tok_end, andor_job_list, job_list>{
    BODY(if_clause)};

DEF_ALT(else_clause) {
    using empty = grammar::empty;
    using else_cont = seq<keyword<parse_keyword_else>, else_continuation>;
    ALT_BODY(else_clause, empty, else_cont);
};

DEF_ALT(else_continuation) {
    using else_if = seq<if_clause, else_clause>;
    using else_only = seq<tok_end, job_list>;
    ALT_BODY(else_continuation, else_if, else_only);
};

DEF(switch_statement)
produces_sequence<keyword<parse_keyword_switch>, argument, tok_end, case_item_list, end_command,
                  arguments_or_redirections_list>{BODY(switch_statement)};

DEF_ALT(case_item_list) {
    using empty = grammar::empty;
    using case_items = seq<case_item, case_item_list>;
    using blank_line = seq<tok_end, case_item_list>;
    ALT_BODY(case_item_list, empty, case_items, blank_line);
};

DEF(case_item) produces_sequence<keyword<parse_keyword_case>, argument_list, tok_end, job_list> {
    BODY(case_item)
};

DEF(block_statement)
produces_sequence<block_header, job_list, end_command, arguments_or_redirections_list>{
    BODY(block_statement)};

DEF_ALT(block_header) {
    using forh = single<for_header>;
    using whileh = single<while_header>;
    using funch = single<function_header>;
    using beginh = single<begin_header>;
    ALT_BODY(block_header, forh, whileh, funch, beginh);
};

DEF(for_header)
produces_sequence<keyword<parse_keyword_for>, tok_string, keyword<parse_keyword_in>, argument_list,
                  tok_end> {
    BODY(for_header)
};

DEF(while_header)
produces_sequence<keyword<parse_keyword_while>, job_conjunction, tok_end, andor_job_list>{
    BODY(while_header)};

DEF(begin_header) produces_single<keyword<parse_keyword_begin>>{BODY(begin_header)};

// Functions take arguments, and require at least one (the name). No redirections allowed.
DEF(function_header)
produces_sequence<keyword<parse_keyword_function>, argument, argument_list, tok_end>{
    BODY(function_header)};

DEF_ALT(not_statement) {
    using nots = seq<keyword<parse_keyword_not>, statement>;
    using exclams = seq<keyword<parse_keyword_exclam>, statement>;
    ALT_BODY(not_statement, nots, exclams);
};

// An andor_job_list is zero or more job lists, where each starts with an `and` or `or` boolean
// statement.
DEF_ALT(andor_job_list) {
    using empty = grammar::empty;
    using andor_job = seq<job_decorator, job_conjunction, andor_job_list>;
    using empty_line = seq<tok_end, andor_job_list>;
    ALT_BODY(andor_job_list, empty, andor_job, empty_line);
};

// A decorated_statement is a command with a list of arguments_or_redirections, possibly with
// "builtin" or "command" or "exec"
DEF_ALT(decorated_statement) {
    using plains = single<plain_statement>;
    using cmds = seq<keyword<parse_keyword_command>, plain_statement>;
    using builtins = seq<keyword<parse_keyword_builtin>, plain_statement>;
    using execs = seq<keyword<parse_keyword_exec>, plain_statement>;
    // Ideally, `evals` should be defined as `seq<keyword<parse_keyword_eval>,
    // arguments_or_redirections_list`, but other parts of the code have the logic hard coded to
    // search for a process at the head of a statement, and bug out if we do that.
    // We also can't define `evals` as a `seq<keyword<parse_keyword_eval>, plain_statement>` because
    // `expand.cpp` hard-codes its "command substitution at the head of a statement is not allowed"
    // check without any way of telling it to perform the substitution anyway. Our solution is to
    // create an empty function called `eval` that never actually gets executed and convert a
    // decorated statement `eval ...` into a plain statement `eval ...`
    using evals = seq<plain_statement>;
    ALT_BODY(decorated_statement, plains, cmds, builtins, execs, evals);
};

DEF(plain_statement)
produces_sequence<tok_string, arguments_or_redirections_list>{BODY(plain_statement)};

DEF_ALT(argument_list) {
    using empty = grammar::empty;
    using arg = seq<argument, argument_list>;
    ALT_BODY(argument_list, empty, arg);
};

DEF_ALT(arguments_or_redirections_list) {
    using empty = grammar::empty;
    using arg = seq<argument, arguments_or_redirections_list>;
    using redir = seq<redirection, arguments_or_redirections_list>;
    ALT_BODY(arguments_or_redirections_list, empty, arg, redir);
};

DEF(argument) produces_single<tok_string>{BODY(argument)};
DEF(redirection) produces_sequence<tok_redirection, tok_string>{BODY(redirection)};

DEF_ALT(optional_background) {
    using empty = grammar::empty;
    using background = single<tok_background>;
    ALT_BODY(optional_background, empty, background);
};

DEF(end_command) produces_single<keyword<parse_keyword_end>>{BODY(end_command)};

// Note optional_newlines only allows newline-style tok_end, not semicolons.
DEF_ALT(optional_newlines) {
    using empty = grammar::empty;
    using newlines = seq<tok_end, optional_newlines>;
    ALT_BODY(optional_newlines, empty, newlines);
};

// A freestanding_argument_list is equivalent to a normal argument list, except it may contain
// TOK_END (newlines, and even semicolons, for historical reasons)
DEF_ALT(freestanding_argument_list) {
    using empty = grammar::empty;
    using arg = seq<argument, freestanding_argument_list>;
    using semicolon = seq<tok_end, freestanding_argument_list>;
    ALT_BODY(freestanding_argument_list, empty, arg, semicolon);
};
}  // namespace grammar
#endif
