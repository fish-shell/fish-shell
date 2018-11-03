// Functions used for implementing the test builtin.
//
// Implemented from scratch (yes, really) by way of IEEE 1003.1 as reference.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>

#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "builtin.h"
#include "common.h"
#include "io.h"
#include "wutil.h"  // IWYU pragma: keep

using std::unique_ptr;
using std::move;

namespace {
namespace test_expressions {

enum token_t {
    test_unknown,  // arbitrary string

    test_bang,  // "!", inverts sense

    test_filetype_b,  // "-b", for block special files
    test_filetype_c,  // "-c", for character special files
    test_filetype_d,  // "-d", for directories
    test_filetype_e,  // "-e", for files that exist
    test_filetype_f,  // "-f", for for regular files
    test_filetype_G,  // "-G", for check effective group id
    test_filetype_g,  // "-g", for set-group-id
    test_filetype_h,  // "-h", for symbolic links
    test_filetype_k,  // "-k", for sticky bit
    test_filetype_L,  // "-L", same as -h
    test_filetype_O,  // "-O", for check effective user id
    test_filetype_p,  // "-p", for FIFO
    test_filetype_S,  // "-S", socket

    test_filesize_s,  // "-s", size greater than zero

    test_filedesc_t,  // "-t", whether the fd is associated with a terminal

    test_fileperm_r,  // "-r", read permission
    test_fileperm_u,  // "-u", whether file is setuid
    test_fileperm_w,  // "-w", whether file write permission is allowed
    test_fileperm_x,  // "-x", whether file execute/search is allowed

    test_string_n,          // "-n", non-empty string
    test_string_z,          // "-z", true if length of string is 0
    test_string_equal,      // "=", true if strings are identical
    test_string_not_equal,  // "!=", true if strings are not identical

    test_number_equal,          // "-eq", true if numbers are equal
    test_number_not_equal,      // "-ne", true if numbers are not equal
    test_number_greater,        // "-gt", true if first number is larger than second
    test_number_greater_equal,  // "-ge", true if first number is at least second
    test_number_lesser,         // "-lt", true if first number is smaller than second
    test_number_lesser_equal,   // "-le", true if first number is at most second

    test_combine_and,  // "-a", true if left and right are both true
    test_combine_or,   // "-o", true if either left or right is true

    test_paren_open,   // "(", open paren
    test_paren_close,  // ")", close paren
};

/// Our number type. We support both doubles and long longs. We have to support these separately
/// because some integers are not representable as doubles; these may come up in practice (e.g.
/// inodes).
class number_t {
    // A number has an integral base and a floating point delta.
    // Conceptually the number is base + delta.
    // We enforce the property that 0 <= delta < 1.
    long long base;
    double delta;

   public:
    number_t(long long base, double delta) : base(base), delta(delta) {
        assert(0.0 <= delta && delta < 1.0 && "Invalid delta");
    }
    number_t() : number_t(0, 0.0) {}

    // Compare two numbers. Returns an integer -1, 0, 1 corresponding to whether we are less than,
    // equal to, or greater than the rhs.
    int compare(number_t rhs) const {
        if (this->base != rhs.base) return (this->base > rhs.base) - (this->base < rhs.base);
        return (this->delta > rhs.delta) - (this->delta < rhs.delta);
    }

    // Return true if the number is a tty()/
    bool isatty() const {
        if (delta != 0.0 || base > INT_MAX || base < INT_MIN) return false;
        return ::isatty(static_cast<int>(base));
    }
};

static bool binary_primary_evaluate(test_expressions::token_t token, const wcstring &left,
                                    const wcstring &right, wcstring_list_t &errors);
static bool unary_primary_evaluate(test_expressions::token_t token, const wcstring &arg,
                                   wcstring_list_t &errors);

enum { UNARY_PRIMARY = 1 << 0, BINARY_PRIMARY = 1 << 1 };

struct token_info_t {
    token_t tok;
    unsigned int flags;
};

const token_info_t * const token_for_string(const wcstring &str) {
    static const std::map<wcstring, const token_info_t> token_infos = {
        {L"", {test_unknown, 0}},
        {L"!", {test_bang, 0}},
        {L"-b", {test_filetype_b, UNARY_PRIMARY}},
        {L"-c", {test_filetype_c, UNARY_PRIMARY}},
        {L"-d", {test_filetype_d, UNARY_PRIMARY}},
        {L"-e", {test_filetype_e, UNARY_PRIMARY}},
        {L"-f", {test_filetype_f, UNARY_PRIMARY}},
        {L"-G", {test_filetype_G, UNARY_PRIMARY}},
        {L"-g", {test_filetype_g, UNARY_PRIMARY}},
        {L"-h", {test_filetype_h, UNARY_PRIMARY}},
        {L"-k", {test_filetype_k, UNARY_PRIMARY}},
        {L"-L", {test_filetype_L, UNARY_PRIMARY}},
        {L"-O", {test_filetype_O, UNARY_PRIMARY}},
        {L"-p", {test_filetype_p, UNARY_PRIMARY}},
        {L"-S", {test_filetype_S, UNARY_PRIMARY}},
        {L"-s", {test_filesize_s, UNARY_PRIMARY}},
        {L"-t", {test_filedesc_t, UNARY_PRIMARY}},
        {L"-r", {test_fileperm_r, UNARY_PRIMARY}},
        {L"-u", {test_fileperm_u, UNARY_PRIMARY}},
        {L"-w", {test_fileperm_w, UNARY_PRIMARY}},
        {L"-x", {test_fileperm_x, UNARY_PRIMARY}},
        {L"-n", {test_string_n, UNARY_PRIMARY}},
        {L"-z", {test_string_z, UNARY_PRIMARY}},
        {L"=", {test_string_equal, BINARY_PRIMARY}},
        {L"!=", {test_string_not_equal, BINARY_PRIMARY}},
        {L"-eq", {test_number_equal, BINARY_PRIMARY}},
        {L"-ne", {test_number_not_equal, BINARY_PRIMARY}},
        {L"-gt", {test_number_greater, BINARY_PRIMARY}},
        {L"-ge", {test_number_greater_equal, BINARY_PRIMARY}},
        {L"-lt", {test_number_lesser, BINARY_PRIMARY}},
        {L"-le", {test_number_lesser_equal, BINARY_PRIMARY}},
        {L"-a", {test_combine_and, 0}},
        {L"-o", {test_combine_or, 0}},
        {L"(", {test_paren_open, 0}},
        {L")", {test_paren_close, 0}}};

    auto t = token_infos.find(str);
    if (t != token_infos.end()) return &t->second;
    return &token_infos.find(L"")->second;
}

// Grammar.
//
//  <expr> = <combining_expr>
//
//  <combining_expr> = <unary_expr> and/or <combining_expr> |
//                     <unary_expr>
//
//  <unary_expr> = bang <unary_expr> |
//                <primary>
//
//  <primary> = <unary_primary> arg |
//              arg <binary_primary> arg |
//              '(' <expr> ')'

class expression;
class test_parser {
   private:
    wcstring_list_t strings;
    wcstring_list_t errors;

    unique_ptr<expression> error(const wchar_t *fmt, ...);
    void add_error(const wchar_t *fmt, ...);

    const wcstring &arg(unsigned int idx) { return strings.at(idx); }

   public:
    explicit test_parser(wcstring_list_t val) : strings(std::move(val)) {}

    unique_ptr<expression> parse_expression(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_3_arg_expression(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_4_arg_expression(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_combining_expression(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_unary_expression(unsigned int start, unsigned int end);

    unique_ptr<expression> parse_primary(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_parenthentical(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_unary_primary(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_binary_primary(unsigned int start, unsigned int end);
    unique_ptr<expression> parse_just_a_string(unsigned int start, unsigned int end);

    static unique_ptr<expression> parse_args(const wcstring_list_t &args, wcstring &err,
                                             wchar_t *program_name);
};

struct range_t {
    unsigned int start;
    unsigned int end;

    range_t(unsigned s, unsigned e) : start(s), end(e) {}
};

/// Base class for expressions.
class expression {
   protected:
    expression(token_t what, range_t where) : token(what), range(std::move(where)) {}

   public:
    const token_t token;
    range_t range;

    virtual ~expression() = default;

    /// Evaluate returns true if the expression is true (i.e. STATUS_CMD_OK).
    virtual bool evaluate(wcstring_list_t &errors) = 0;
};

/// Single argument like -n foo or "just a string".
class unary_primary : public expression {
   public:
    wcstring arg;
    unary_primary(token_t tok, range_t where, wcstring what)
        : expression(tok, where), arg(std::move(what)) {}
    bool evaluate(wcstring_list_t &errors) override;
};

/// Two argument primary like foo != bar.
class binary_primary : public expression {
   public:
    wcstring arg_left;
    wcstring arg_right;

    binary_primary(token_t tok, range_t where, wcstring left, wcstring right)
        : expression(tok, where), arg_left(std::move(left)), arg_right(std::move(right)) {}
    bool evaluate(wcstring_list_t &errors) override;
};

/// Unary operator like bang.
class unary_operator : public expression {
   public:
    unique_ptr<expression> subject;
    unary_operator(token_t tok, range_t where, unique_ptr<expression> exp)
        : expression(tok, where), subject(move(exp)) {}
    bool evaluate(wcstring_list_t &errors) override;
};

/// Combining expression. Contains a list of AND or OR expressions. It takes more than two so that
/// we don't have to worry about precedence in the parser.
class combining_expression : public expression {
   public:
    const std::vector<unique_ptr<expression>> subjects;
    const std::vector<token_t> combiners;

    combining_expression(token_t tok, range_t where, std::vector<unique_ptr<expression>> exprs,
                         const std::vector<token_t> &combs)
        : expression(tok, where), subjects(std::move(exprs)), combiners(std::move(combs)) {
        // We should have one more subject than combiner.
        assert(subjects.size() == combiners.size() + 1);
    }

    ~combining_expression() override = default;

    bool evaluate(wcstring_list_t &errors) override;
};

/// Parenthetical expression.
class parenthetical_expression : public expression {
   public:
    unique_ptr<expression> contents;
    parenthetical_expression(token_t tok, range_t where, unique_ptr<expression> expr)
        : expression(tok, where), contents(move(expr)) {}

    bool evaluate(wcstring_list_t &errors) override;
};

void test_parser::add_error(const wchar_t *fmt, ...) {
    assert(fmt != NULL);
    va_list va;
    va_start(va, fmt);
    this->errors.push_back(vformat_string(fmt, va));
    va_end(va);
}

unique_ptr<expression> test_parser::error(const wchar_t *fmt, ...) {
    assert(fmt != NULL);
    va_list va;
    va_start(va, fmt);
    this->errors.push_back(vformat_string(fmt, va));
    va_end(va);
    return NULL;
}

unique_ptr<expression> test_parser::parse_unary_expression(unsigned int start, unsigned int end) {
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }
    token_t tok = token_for_string(arg(start))->tok;
    if (tok == test_bang) {
        unique_ptr<expression> subject(parse_unary_expression(start + 1, end));
        if (subject.get()) {
            return make_unique<unary_operator>(tok, range_t(start, subject->range.end),
                                               move(subject));
        }
        return NULL;
    }
    return parse_primary(start, end);
}

/// Parse a combining expression (AND, OR).
unique_ptr<expression> test_parser::parse_combining_expression(unsigned int start,
                                                               unsigned int end) {
    if (start >= end) return NULL;

    std::vector<unique_ptr<expression>> subjects;
    std::vector<token_t> combiners;
    unsigned int idx = start;
    bool first = true;

    while (idx < end) {
        if (!first) {
            // This is not the first expression, so we expect a combiner.
            token_t combiner = token_for_string(arg(idx))->tok;
            if (combiner != test_combine_and && combiner != test_combine_or) {
                /* Not a combiner, we're done */
                this->errors.insert(
                    this->errors.begin(),
                    format_string(L"Expected a combining operator like '-a' at index %u", idx));
                break;
            }
            combiners.push_back(combiner);
            idx++;
        }

        // Parse another expression.
        unique_ptr<expression> expr = parse_unary_expression(idx, end);
        if (!expr) {
            add_error(L"Missing argument at index %u", idx);
            if (!first) {
                // Clean up the dangling combiner, since it never got its right hand expression.
                combiners.pop_back();
            }
            break;
        }

        // Go to the end of this expression.
        idx = expr->range.end;
        subjects.push_back(move(expr));
        first = false;
    }

    if (subjects.empty()) {
        return NULL;  // no subjects
    }
    // Our new expression takes ownership of all expressions we created. The token we pass is
    // irrelevant.
    return make_unique<combining_expression>(test_combine_and, range_t(start, idx), move(subjects),
                                             move(combiners));
}

unique_ptr<expression> test_parser::parse_unary_primary(unsigned int start, unsigned int end) {
    // We need two arguments.
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }
    if (start + 1 >= end) {
        return error(L"Missing argument at index %u", start + 1);
    }

    // All our unary primaries are prefix, so the operator is at start.
    const token_info_t *info = token_for_string(arg(start));
    if (!(info->flags & UNARY_PRIMARY)) return NULL;

    return make_unique<unary_primary>(info->tok, range_t(start, start + 2), arg(start + 1));
}

unique_ptr<expression> test_parser::parse_just_a_string(unsigned int start, unsigned int end) {
    // Handle a string as a unary primary that is not a token of any other type. e.g. 'test foo -a
    // bar' should evaluate to true We handle this with a unary primary of test_string_n.

    // We need one argument.
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }

    const token_info_t *info = token_for_string(arg(start));
    if (info->tok != test_unknown) {
        return error(L"Unexpected argument type at index %u", start);
    }

    // This is hackish; a nicer way to implement this would be with a "just a string" expression
    // type.
    return make_unique<unary_primary>(test_string_n, range_t(start, start + 1), arg(start));
}

unique_ptr<expression> test_parser::parse_binary_primary(unsigned int start, unsigned int end) {
    // We need three arguments.
    for (unsigned int idx = start; idx < start + 3; idx++) {
        if (idx >= end) {
            return error(L"Missing argument at index %u", idx);
        }
    }

    // All our binary primaries are infix, so the operator is at start + 1.
    const token_info_t *info = token_for_string(arg(start + 1));
    if (!(info->flags & BINARY_PRIMARY)) return NULL;

    return make_unique<binary_primary>(info->tok, range_t(start, start + 3), arg(start),
                                       arg(start + 2));
}

unique_ptr<expression> test_parser::parse_parenthentical(unsigned int start, unsigned int end) {
    // We need at least three arguments: open paren, argument, close paren.
    if (start + 3 >= end) return NULL;

    // Must start with an open expression.
    const token_info_t *open_paren = token_for_string(arg(start));
    if (open_paren->tok != test_paren_open) return NULL;

    // Parse a subexpression.
    unique_ptr<expression> subexpr = parse_expression(start + 1, end);
    if (!subexpr) return NULL;

    // Parse a close paren.
    unsigned close_index = subexpr->range.end;
    assert(close_index <= end);
    if (close_index == end) {
        return error(L"Missing close paren at index %u", close_index);
    }
    const token_info_t *close_paren = token_for_string(arg(close_index));
    if (close_paren->tok != test_paren_close) {
        return error(L"Expected close paren at index %u", close_index);
    }

    // Success.
    return make_unique<parenthetical_expression>(test_paren_open, range_t(start, close_index + 1),
                                                 move(subexpr));
}

unique_ptr<expression> test_parser::parse_primary(unsigned int start, unsigned int end) {
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }

    unique_ptr<expression> expr = NULL;
    if (!expr) expr = parse_parenthentical(start, end);
    if (!expr) expr = parse_unary_primary(start, end);
    if (!expr) expr = parse_binary_primary(start, end);
    if (!expr) expr = parse_just_a_string(start, end);
    return expr;
}

// See IEEE 1003.1 breakdown of the behavior for different parameter counts.
unique_ptr<expression> test_parser::parse_3_arg_expression(unsigned int start, unsigned int end) {
    assert(end - start == 3);
    unique_ptr<expression> result = NULL;

    const token_info_t *center_token = token_for_string(arg(start + 1));
    if (center_token->flags & BINARY_PRIMARY) {
        result = parse_binary_primary(start, end);
    } else if (center_token->tok == test_combine_and || center_token->tok == test_combine_or) {
        unique_ptr<expression> left(parse_unary_expression(start, start + 1));
        unique_ptr<expression> right(parse_unary_expression(start + 2, start + 3));
        if (left.get() && right.get()) {
            // Transfer ownership to the vector of subjects.
            std::vector<token_t> combiners = {center_token->tok};
            std::vector<unique_ptr<expression>> subjects;
            subjects.push_back(move(left));
            subjects.push_back(move(right));
            result = make_unique<combining_expression>(center_token->tok, range_t(start, end),
                                                       move(subjects), move(combiners));
        }
    } else {
        result = parse_unary_expression(start, end);
    }
    return result;
}

unique_ptr<expression> test_parser::parse_4_arg_expression(unsigned int start, unsigned int end) {
    assert(end - start == 4);
    unique_ptr<expression> result = NULL;

    token_t first_token = token_for_string(arg(start))->tok;
    if (first_token == test_bang) {
        unique_ptr<expression> subject(parse_3_arg_expression(start + 1, end));
        if (subject.get()) {
            result = make_unique<unary_operator>(first_token, range_t(start, subject->range.end),
                                                 move(subject));
        }
    } else if (first_token == test_paren_open) {
        result = parse_parenthentical(start, end);
    } else {
        result = parse_combining_expression(start, end);
    }
    return result;
}

unique_ptr<expression> test_parser::parse_expression(unsigned int start, unsigned int end) {
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }

    unsigned int argc = end - start;
    switch (argc) {
        case 0: {
            DIE("argc should not be zero");  // should have been caught by the above test
            break;
        }
        case 1: {
            return error(L"Missing argument at index %u", start + 1);
        }
        case 2: {
            return parse_unary_expression(start, end);
        }
        case 3: {
            return parse_3_arg_expression(start, end);
        }
        case 4: {
            return parse_4_arg_expression(start, end);
        }
        default: { return parse_combining_expression(start, end); }
    }
}

unique_ptr<expression> test_parser::parse_args(const wcstring_list_t &args, wcstring &err,
                                               wchar_t *program_name) {
    // Empty list and one-arg list should be handled by caller.
    assert(args.size() > 1);

    test_parser parser(args);
    unique_ptr<expression> result = parser.parse_expression(0, (unsigned int)args.size());

    // Handle errors.
    // For now we only show the first error.
    if (!parser.errors.empty()) {
        err.append(program_name);
        err.append(L": ");
        err.append(parser.errors.at(0));
        err.push_back(L'\n');
    }

    if (result) {
        // It's also an error if there are any unused arguments. This is not detected by
        // parse_expression().
        assert(result->range.end <= args.size());
        if (result->range.end < args.size()) {
            if (err.empty()) {
                append_format(err, L"%ls: unexpected argument at index %lu: '%ls'\n", program_name,
                              (unsigned long)result->range.end, args.at(result->range.end).c_str());
            }
            result.reset(NULL);
        }
    }

    return result;
}

bool unary_primary::evaluate(wcstring_list_t &errors) {
    return unary_primary_evaluate(token, arg, errors);
}

bool binary_primary::evaluate(wcstring_list_t &errors) {
    return binary_primary_evaluate(token, arg_left, arg_right, errors);
}

bool unary_operator::evaluate(wcstring_list_t &errors) {
    if (token == test_bang) {
        assert(subject.get());
        return !subject->evaluate(errors);
    }

    errors.push_back(format_string(L"Unknown token type in %s", __func__));
    return false;
}

bool combining_expression::evaluate(wcstring_list_t &errors) {
    if (token == test_combine_and || token == test_combine_or) {
        assert(!subjects.empty());  //!OCLINT(multiple unary operator)
        assert(combiners.size() + 1 == subjects.size());

        // One-element case.
        if (subjects.size() == 1) return subjects.at(0)->evaluate(errors);

        // Evaluate our lists, remembering that AND has higher precedence than OR. We can
        // visualize this as a sequence of OR expressions of AND expressions.
        size_t idx = 0, max = subjects.size();
        bool or_result = false;
        while (idx < max) {
            if (or_result) {  // short circuit
                break;
            }

            // Evaluate a stream of AND starting at given subject index. It may only have one
            // element.
            bool and_result = true;
            for (; idx < max; idx++) {
                // Evaluate it, short-circuiting.
                and_result = and_result && subjects.at(idx)->evaluate(errors);

                // If the combiner at this index (which corresponding to how we combine with the
                // next subject) is not AND, then exit the loop.
                if (idx + 1 < max && combiners.at(idx) != test_combine_and) {
                    idx++;
                    break;
                }
            }

            // OR it in.
            or_result = or_result || and_result;
        }
        return or_result;
    }

    errors.push_back(format_string(L"Unknown token type in %s", __func__));
    return STATUS_INVALID_ARGS;
}

bool parenthetical_expression::evaluate(wcstring_list_t &errors) {
    return contents->evaluate(errors);
}

// Parse a double from arg. Return true on success, false on failure.
static bool parse_double(const wchar_t *arg, double *out_res) {
    // Consume leading spaces.
    while (arg && *arg != L'\0' && iswspace(*arg)) arg++;
    errno = 0;
    wchar_t *end = NULL;
    *out_res = wcstod_l(arg, &end, fish_c_locale());
    // Consume trailing spaces.
    while (end && *end != L'\0' && iswspace(*end)) end++;
    return errno == 0 && end > arg && *end == L'\0';
}

// IEEE 1003.1 says nothing about what it means for two strings to be "algebraically equal". For
// example, should we interpret 0x10 as 0, 10, or 16? Here we use only base 10 and use wcstoll,
// which allows for leading + and -, and whitespace. This is consistent, albeit a bit more lenient
// since we allow trailing whitespace, with other implementations such as bash.
static bool parse_number(const wcstring &arg, number_t *number, wcstring_list_t &errors) {
    const wchar_t *argcs = arg.c_str();
    double floating = 0;
    bool got_float = parse_double(argcs, &floating);

    errno = 0;
    long long integral = fish_wcstoll(argcs);
    bool got_int = (errno == 0);
    if (got_int) {
        // Here the value is just an integer; ignore the floating point parse because it may be
        // invalid (e.g. not a representable integer).
        *number = number_t{integral, 0.0};
        return true;
    } else if (got_float) {
        // Here we parsed an (in range) floating point value that could not be parsed as an integer.
        // Break the floating point value into base and delta. Ensure that base is <= the floating
        // point value.
        double intpart = std::floor(floating);
        double delta = floating - intpart;
        *number = number_t{static_cast<long long>(intpart), delta};
        return true;
    } else {
        // We could not parse a float or an int.
        errors.push_back(format_string(_(L"invalid number '%ls'"), arg.c_str()));
        return false;
    }
}

static bool binary_primary_evaluate(test_expressions::token_t token, const wcstring &left,
                                    const wcstring &right, wcstring_list_t &errors) {
    using namespace test_expressions;
    number_t ln, rn;
    switch (token) {
        case test_string_equal: {
            return left == right;
        }
        case test_string_not_equal: {
            return left != right;
        }
        case test_number_equal: {
            return parse_number(left, &ln, errors) && parse_number(right, &rn, errors) &&
                   ln.compare(rn) == 0;
        }
        case test_number_not_equal: {
            return parse_number(left, &ln, errors) && parse_number(right, &rn, errors) &&
                   ln.compare(rn) != 0;
        }
        case test_number_greater: {
            return parse_number(left, &ln, errors) && parse_number(right, &rn, errors) &&
                   ln.compare(rn) > 0;
        }
        case test_number_greater_equal: {
            return parse_number(left, &ln, errors) && parse_number(right, &rn, errors) &&
                   ln.compare(rn) >= 0;
        }
        case test_number_lesser: {
            return parse_number(left, &ln, errors) && parse_number(right, &rn, errors) &&
                   ln.compare(rn) < 0;
        }
        case test_number_lesser_equal: {
            return parse_number(left, &ln, errors) && parse_number(right, &rn, errors) &&
                   ln.compare(rn) <= 0;
        }
        default: {
            errors.push_back(format_string(L"Unknown token type in %s", __func__));
            return false;
        }
    }
}

static bool unary_primary_evaluate(test_expressions::token_t token, const wcstring &arg,
                                   wcstring_list_t &errors) {
    using namespace test_expressions;
    struct stat buf;
    switch (token) {
        case test_filetype_b: {  // "-b", for block special files
            return !wstat(arg, &buf) && S_ISBLK(buf.st_mode);
        }
        case test_filetype_c: {  // "-c", for character special files
            return !wstat(arg, &buf) && S_ISCHR(buf.st_mode);
        }
        case test_filetype_d: {  // "-d", for directories
            return !wstat(arg, &buf) && S_ISDIR(buf.st_mode);
        }
        case test_filetype_e: {  // "-e", for files that exist
            return !wstat(arg, &buf);
        }
        case test_filetype_f: {  // "-f", for for regular files
            return !wstat(arg, &buf) && S_ISREG(buf.st_mode);
        }
        case test_filetype_G: {  // "-G", for check effective group id
            return !wstat(arg, &buf) && getegid() == buf.st_gid;
        }
        case test_filetype_g: {  // "-g", for set-group-id
            return !wstat(arg, &buf) && (S_ISGID & buf.st_mode);
        }
        case test_filetype_h:    // "-h", for symbolic links
        case test_filetype_L: {  // "-L", same as -h
            return !lwstat(arg, &buf) && S_ISLNK(buf.st_mode);
        }
        case test_filetype_k: {  // "-k", for sticky bit
#ifdef S_ISVTX
            return !lwstat(arg, &buf) && buf.st_mode & S_ISVTX;
#else
            return false;
#endif
        }
        case test_filetype_O: {  // "-O", for check effective user id
            return !wstat(arg, &buf) && geteuid() == buf.st_uid;
        }
        case test_filetype_p: {  // "-p", for FIFO
            return !wstat(arg, &buf) && S_ISFIFO(buf.st_mode);
        }
        case test_filetype_S: {  // "-S", socket
            return !wstat(arg, &buf) && S_ISSOCK(buf.st_mode);
        }
        case test_filesize_s: {  // "-s", size greater than zero
            return !wstat(arg, &buf) && buf.st_size > 0;
        }
        case test_filedesc_t: {  // "-t", whether the fd is associated with a terminal
            number_t num;
            return parse_number(arg, &num, errors) && num.isatty();
        }
        case test_fileperm_r: {  // "-r", read permission
            return !waccess(arg, R_OK);
        }
        case test_fileperm_u: {  // "-u", whether file is setuid
            return !wstat(arg, &buf) && (S_ISUID & buf.st_mode);
        }
        case test_fileperm_w: {  // "-w", whether file write permission is allowed
            return !waccess(arg, W_OK);
        }
        case test_fileperm_x: {  // "-x", whether file execute/search is allowed
            return !waccess(arg, X_OK);
        }
        case test_string_n: {  // "-n", non-empty string
            return !arg.empty();
        }
        case test_string_z: {  // "-z", true if length of string is 0
            return arg.empty();
        }
        default: {
            errors.push_back(format_string(L"Unknown token type in %s", __func__));
            return false;
        }
    }
}
};  // namespace test_expressions
};  // anonymous namespace

/// Evaluate a conditional expression given the arguments. If fromtest is set, the caller is the
/// test or [ builtin; with the pointer giving the name of the command. for POSIX conformance this
/// supports a more limited range of functionality.
///
/// Return status is the final shell status, i.e. 0 for true, 1 for false and 2 for error.
int builtin_test(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    UNUSED(parser);
    using namespace test_expressions;

    // The first argument should be the name of the command ('test').
    if (!argv[0]) return STATUS_INVALID_ARGS;

    // Whether we are invoked with bracket '[' or not.
    wchar_t *program_name = argv[0];
    const bool is_bracket = !wcscmp(program_name, L"[");

    size_t argc = 0;
    while (argv[argc + 1]) argc++;

    // If we're bracket, the last argument ought to be ]; we ignore it. Note that argc is the number
    // of arguments after the command name; thus argv[argc] is the last argument.
    if (is_bracket) {
        if (!wcscmp(argv[argc], L"]")) {
            // Ignore the closing bracket from now on.
            argc--;
        } else {
            streams.err.append(L"[: the last argument must be ']'\n");
            return STATUS_INVALID_ARGS;
        }
    }

    // Collect the arguments into a list.
    const wcstring_list_t args(argv + 1, argv + 1 + argc);

    if (argc == 0) {
        return STATUS_CMD_ERROR;  // Per 1003.1, exit false.
    } else if (argc == 1) {
        // Per 1003.1, exit true if the arg is non-empty.
        return args.at(0).empty() ? STATUS_CMD_ERROR : STATUS_CMD_OK;
    }

    // Try parsing
    wcstring err;
    unique_ptr<expression> expr = test_parser::parse_args(args, err, program_name);
    if (!expr) {
#if 0
        streams.err.append(L"Oops! test was given args:\n");
        for (size_t i=0; i < argc; i++) {
            streams.err.append_format(L"\t%ls\n", args.at(i).c_str());
        }
        streams.err.append_format(L"and returned parse error: %ls\n", err.c_str());
#endif
        streams.err.append(err);
        return STATUS_CMD_ERROR;
    }

    wcstring_list_t eval_errors;
    bool result = expr->evaluate(eval_errors);
    if (!eval_errors.empty() && !should_suppress_stderr_for_tests()) {
        streams.err.append(L"test returned eval errors:\n");
        for (size_t i = 0; i < eval_errors.size(); i++) {
            streams.err.append_format(L"\t%ls\n", eval_errors.at(i).c_str());
        }
    }
    return result ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}
