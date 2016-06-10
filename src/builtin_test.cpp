// Functions used for implementing the test builtin.
//
// Implemented from scratch (yes, really) by way of IEEE 1003.1 as reference.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <memory>
#include <string>

#include "builtin.h"
#include "common.h"
#include "io.h"
#include "proc.h"
#include "wutil.h"  // IWYU pragma: keep

enum { BUILTIN_TEST_SUCCESS = STATUS_BUILTIN_OK, BUILTIN_TEST_FAIL = STATUS_BUILTIN_ERROR };

int builtin_test(parser_t &parser, io_streams_t &streams, wchar_t **argv);

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

static bool binary_primary_evaluate(test_expressions::token_t token, const wcstring &left,
                                    const wcstring &right, wcstring_list_t &errors);
static bool unary_primary_evaluate(test_expressions::token_t token, const wcstring &arg,
                                   wcstring_list_t &errors);

enum { UNARY_PRIMARY = 1 << 0, BINARY_PRIMARY = 1 << 1 };

static const struct token_info_t {
    token_t tok;
    const wchar_t *string;
    unsigned int flags;
} token_infos[] = {{test_unknown, L"", 0},
                   {test_bang, L"!", 0},
                   {test_filetype_b, L"-b", UNARY_PRIMARY},
                   {test_filetype_c, L"-c", UNARY_PRIMARY},
                   {test_filetype_d, L"-d", UNARY_PRIMARY},
                   {test_filetype_e, L"-e", UNARY_PRIMARY},
                   {test_filetype_f, L"-f", UNARY_PRIMARY},
                   {test_filetype_G, L"-G", UNARY_PRIMARY},
                   {test_filetype_g, L"-g", UNARY_PRIMARY},
                   {test_filetype_h, L"-h", UNARY_PRIMARY},
                   {test_filetype_L, L"-L", UNARY_PRIMARY},
                   {test_filetype_O, L"-O", UNARY_PRIMARY},
                   {test_filetype_p, L"-p", UNARY_PRIMARY},
                   {test_filetype_S, L"-S", UNARY_PRIMARY},
                   {test_filesize_s, L"-s", UNARY_PRIMARY},
                   {test_filedesc_t, L"-t", UNARY_PRIMARY},
                   {test_fileperm_r, L"-r", UNARY_PRIMARY},
                   {test_fileperm_u, L"-u", UNARY_PRIMARY},
                   {test_fileperm_w, L"-w", UNARY_PRIMARY},
                   {test_fileperm_x, L"-x", UNARY_PRIMARY},
                   {test_string_n, L"-n", UNARY_PRIMARY},
                   {test_string_z, L"-z", UNARY_PRIMARY},
                   {test_string_equal, L"=", BINARY_PRIMARY},
                   {test_string_not_equal, L"!=", BINARY_PRIMARY},
                   {test_number_equal, L"-eq", BINARY_PRIMARY},
                   {test_number_not_equal, L"-ne", BINARY_PRIMARY},
                   {test_number_greater, L"-gt", BINARY_PRIMARY},
                   {test_number_greater_equal, L"-ge", BINARY_PRIMARY},
                   {test_number_lesser, L"-lt", BINARY_PRIMARY},
                   {test_number_lesser_equal, L"-le", BINARY_PRIMARY},
                   {test_combine_and, L"-a", 0},
                   {test_combine_or, L"-o", 0},
                   {test_paren_open, L"(", 0},
                   {test_paren_close, L")", 0}};

const token_info_t *token_for_string(const wcstring &str) {
    for (size_t i = 0; i < sizeof token_infos / sizeof *token_infos; i++) {
        if (str == token_infos[i].string) {
            return &token_infos[i];
        }
    }
    return &token_infos[0];  // unknown
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

    expression *error(const wchar_t *fmt, ...);
    void add_error(const wchar_t *fmt, ...);

    const wcstring &arg(unsigned int idx) { return strings.at(idx); }

   public:
    explicit test_parser(const wcstring_list_t &val) : strings(val) {}

    expression *parse_expression(unsigned int start, unsigned int end);
    expression *parse_3_arg_expression(unsigned int start, unsigned int end);
    expression *parse_4_arg_expression(unsigned int start, unsigned int end);
    expression *parse_combining_expression(unsigned int start, unsigned int end);
    expression *parse_unary_expression(unsigned int start, unsigned int end);

    expression *parse_primary(unsigned int start, unsigned int end);
    expression *parse_parenthentical(unsigned int start, unsigned int end);
    expression *parse_unary_primary(unsigned int start, unsigned int end);
    expression *parse_binary_primary(unsigned int start, unsigned int end);
    expression *parse_just_a_string(unsigned int start, unsigned int end);

    static expression *parse_args(const wcstring_list_t &args, wcstring &err);
};

struct range_t {
    unsigned int start;
    unsigned int end;

    range_t(unsigned s, unsigned e) : start(s), end(e) {}
};

/// Base class for expressions.
class expression {
   protected:
    expression(token_t what, range_t where) : token(what), range(where) {}

   public:
    const token_t token;
    range_t range;

    virtual ~expression() {}

    /// Evaluate returns true if the expression is true (i.e. BUILTIN_TEST_SUCCESS).
    virtual bool evaluate(wcstring_list_t &errors) = 0;
};

typedef std::auto_ptr<expression> expr_ref_t;

/// Single argument like -n foo or "just a string".
class unary_primary : public expression {
   public:
    wcstring arg;
    unary_primary(token_t tok, range_t where, const wcstring &what)
        : expression(tok, where), arg(what) {}
    bool evaluate(wcstring_list_t &errors);
};

/// Two argument primary like foo != bar.
class binary_primary : public expression {
   public:
    wcstring arg_left;
    wcstring arg_right;

    binary_primary(token_t tok, range_t where, const wcstring &left, const wcstring &right)
        : expression(tok, where), arg_left(left), arg_right(right) {}
    bool evaluate(wcstring_list_t &errors);
};

/// Unary operator like bang.
class unary_operator : public expression {
   public:
    expr_ref_t subject;
    unary_operator(token_t tok, range_t where, expr_ref_t &exp)
        : expression(tok, where), subject(exp) {}
    bool evaluate(wcstring_list_t &errors);
};

/// Combining expression. Contains a list of AND or OR expressions. It takes more than two so that
/// we don't have to worry about precedence in the parser.
class combining_expression : public expression {
   public:
    const std::vector<expression *> subjects;
    const std::vector<token_t> combiners;

    combining_expression(token_t tok, range_t where, const std::vector<expression *> &exprs,
                         const std::vector<token_t> &combs)
        : expression(tok, where), subjects(exprs), combiners(combs) {
        // We should have one more subject than combiner.
        assert(subjects.size() == combiners.size() + 1);
    }

    // We are responsible for destroying our expressions.
    virtual ~combining_expression() {
        for (size_t i = 0; i < subjects.size(); i++) {
            delete subjects[i];
        }
    }

    bool evaluate(wcstring_list_t &errors);
};

/// Parenthetical expression.
class parenthetical_expression : public expression {
   public:
    expr_ref_t contents;
    parenthetical_expression(token_t tok, range_t where, expr_ref_t &expr)
        : expression(tok, where), contents(expr) {}

    virtual bool evaluate(wcstring_list_t &errors);
};

void test_parser::add_error(const wchar_t *fmt, ...) {
    assert(fmt != NULL);
    va_list va;
    va_start(va, fmt);
    this->errors.push_back(vformat_string(fmt, va));
    va_end(va);
}

expression *test_parser::error(const wchar_t *fmt, ...) {
    assert(fmt != NULL);
    va_list va;
    va_start(va, fmt);
    this->errors.push_back(vformat_string(fmt, va));
    va_end(va);
    return NULL;
}

expression *test_parser::parse_unary_expression(unsigned int start, unsigned int end) {
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }
    token_t tok = token_for_string(arg(start))->tok;
    if (tok == test_bang) {
        expr_ref_t subject(parse_unary_expression(start + 1, end));
        if (subject.get()) {
            return new unary_operator(tok, range_t(start, subject->range.end), subject);
        }
        return NULL;
    }
    return parse_primary(start, end);
}

/// Parse a combining expression (AND, OR).
expression *test_parser::parse_combining_expression(unsigned int start, unsigned int end) {
    if (start >= end) return NULL;

    std::vector<expression *> subjects;
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
        expression *expr = parse_unary_expression(idx, end);
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
        subjects.push_back(expr);
        first = false;
    }

    if (subjects.empty()) {
        return NULL;  // no subjects
    }
    // Our new expression takes ownership of all expressions we created. The token we pass is
    // irrelevant.
    return new combining_expression(test_combine_and, range_t(start, idx), subjects, combiners);
}

expression *test_parser::parse_unary_primary(unsigned int start, unsigned int end) {
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

    return new unary_primary(info->tok, range_t(start, start + 2), arg(start + 1));
}

expression *test_parser::parse_just_a_string(unsigned int start, unsigned int end) {
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
    return new unary_primary(test_string_n, range_t(start, start + 1), arg(start));
}

#if 0
expression *test_parser::parse_unary_primary(unsigned int start, unsigned int end)
{
    // We need either one or two arguments.
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }

    // The index of the argument to the unary primary.
    unsigned int arg_idx;

    // All our unary primaries are prefix, so any operator is at start. But it also may just be a
    // string, with no operator.
    const token_info_t *info = token_for_string(arg(start));
    if (info->flags & UNARY_PRIMARY) {
        /* We have an operator. Skip the operator argument */
        arg_idx = start + 1;

        // We have some freedom here...do we allow other tokens for the argument to operate on? For
        // example, should 'test -n =' work? I say yes. So no typechecking on the next token.
    } else if (info->tok == test_unknown) {
        // "Just a string.
        arg_idx = start;
    } else {
        // Here we don't allow arbitrary tokens as "just a string." I.e. 'test = -a =' should have a
        // parse error. We could relax this at some point.
        return error(L"Parse error at argument index %u", start);
    }

    // Verify we have the argument we want, i.e. test -n should fail to parse.
    if (arg_idx >= end) {
        return error(L"Missing argument at index %u", arg_idx);
    }

    return new unary_primary(info->tok, range_t(start, arg_idx + 1), arg(arg_idx));
}
#endif

expression *test_parser::parse_binary_primary(unsigned int start, unsigned int end) {
    // We need three arguments.
    for (unsigned int idx = start; idx < start + 3; idx++) {
        if (idx >= end) {
            return error(L"Missing argument at index %u", idx);
        }
    }

    // All our binary primaries are infix, so the operator is at start + 1.
    const token_info_t *info = token_for_string(arg(start + 1));
    if (!(info->flags & BINARY_PRIMARY)) return NULL;

    return new binary_primary(info->tok, range_t(start, start + 3), arg(start), arg(start + 2));
}

expression *test_parser::parse_parenthentical(unsigned int start, unsigned int end) {
    // We need at least three arguments: open paren, argument, close paren.
    if (start + 3 >= end) return NULL;

    // Must start with an open expression.
    const token_info_t *open_paren = token_for_string(arg(start));
    if (open_paren->tok != test_paren_open) return NULL;

    // Parse a subexpression.
    expression *subexr_ptr = parse_expression(start + 1, end);
    if (!subexr_ptr) return NULL;
    expr_ref_t subexpr(subexr_ptr);

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
    return new parenthetical_expression(test_paren_open, range_t(start, close_index + 1), subexpr);
}

expression *test_parser::parse_primary(unsigned int start, unsigned int end) {
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }

    expression *expr = NULL;
    if (!expr) expr = parse_parenthentical(start, end);
    if (!expr) expr = parse_unary_primary(start, end);
    if (!expr) expr = parse_binary_primary(start, end);
    if (!expr) expr = parse_just_a_string(start, end);
    return expr;
}

// See IEEE 1003.1 breakdown of the behavior for different parameter counts.
expression *test_parser::parse_3_arg_expression(unsigned int start, unsigned int end) {
    assert(end - start == 3);
    expression *result = NULL;

    const token_info_t *center_token = token_for_string(arg(start + 1));
    if (center_token->flags & BINARY_PRIMARY) {
        result = parse_binary_primary(start, end);
    } else if (center_token->tok == test_combine_and || center_token->tok == test_combine_or) {
        expr_ref_t left(parse_unary_expression(start, start + 1));
        expr_ref_t right(parse_unary_expression(start + 2, start + 3));
        if (left.get() && right.get()) {
            // Transfer ownership to the vector of subjects.
            std::vector<token_t> combiners(1, center_token->tok);
            std::vector<expression *> subjects;
            subjects.push_back(left.release());
            subjects.push_back(right.release());
            result = new combining_expression(center_token->tok, range_t(start, end), subjects,
                                              combiners);
        }
    } else {
        result = parse_unary_expression(start, end);
    }
    return result;
}

expression *test_parser::parse_4_arg_expression(unsigned int start, unsigned int end) {
    assert(end - start == 4);
    expression *result = NULL;

    token_t first_token = token_for_string(arg(start))->tok;
    if (first_token == test_bang) {
        expr_ref_t subject(parse_3_arg_expression(start + 1, end));
        if (subject.get()) {
            result = new unary_operator(first_token, range_t(start, subject->range.end), subject);
        }
    } else if (first_token == test_paren_open) {
        result = parse_parenthentical(start, end);
    } else {
        result = parse_combining_expression(start, end);
    }
    return result;
}

expression *test_parser::parse_expression(unsigned int start, unsigned int end) {
    if (start >= end) {
        return error(L"Missing argument at index %u", start);
    }

    unsigned int argc = end - start;
    switch (argc) {
        case 0: {
            assert(0);  // should have been caught by the above test
            return NULL;
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

expression *test_parser::parse_args(const wcstring_list_t &args, wcstring &err) {
    // Empty list and one-arg list should be handled by caller.
    assert(args.size() > 1);

    test_parser parser(args);
    expression *result = parser.parse_expression(0, (unsigned int)args.size());

    // Handle errors.
    for (size_t i = 0; i < parser.errors.size(); i++) {
        err.append(L"test: ");
        err.append(parser.errors.at(i));
        err.push_back(L'\n');
        // For now we only show the first error.
        break;
    }

    if (result) {
        // It's also an error if there are any unused arguments. This is not detected by
        // parse_expression().
        assert(result->range.end <= args.size());
        if (result->range.end < args.size()) {
            if (err.empty()) {
                append_format(err, L"test: unexpected argument at index %lu: '%ls'\n",
                              (unsigned long)result->range.end, args.at(result->range.end).c_str());
            }

            delete result;
            result = NULL;
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
    switch (token) {
        case test_bang: {
            assert(subject.get());
            return !subject->evaluate(errors);
        }
        default: {
            errors.push_back(format_string(L"Unknown token type in %s", __func__));
            return false;
        }
    }
}

bool combining_expression::evaluate(wcstring_list_t &errors) {
    switch (token) {
        case test_combine_and:
        case test_combine_or: {
            // One-element case.
            if (subjects.size() == 1) return subjects.at(0)->evaluate(errors);

            // Evaluate our lists, remembering that AND has higher precedence than OR. We can
            // visualize this as a sequence of OR expressions of AND expressions.
            assert(combiners.size() + 1 == subjects.size());
            assert(!subjects.empty());

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
        default: {
            errors.push_back(format_string(L"Unknown token type in %s", __func__));
            return BUILTIN_TEST_FAIL;
        }
    }
}

bool parenthetical_expression::evaluate(wcstring_list_t &errors) {
    return contents->evaluate(errors);
}

// IEEE 1003.1 says nothing about what it means for two strings to be "algebraically equal". For
// example, should we interpret 0x10 as 0, 10, or 16? Here we use only base 10 and use wcstoll,
// which allows for leading + and -, and leading whitespace. This matches bash.
static bool parse_number(const wcstring &arg, long long *out) {
    const wchar_t *str = arg.c_str();
    wchar_t *endptr = NULL;
    *out = wcstoll(str, &endptr, 10);
    return endptr && *endptr == L'\0';
}

static bool binary_primary_evaluate(test_expressions::token_t token, const wcstring &left,
                                    const wcstring &right, wcstring_list_t &errors) {
    using namespace test_expressions;
    long long left_num, right_num;
    switch (token) {
        case test_string_equal: {
            return left == right;
        }
        case test_string_not_equal: {
            return left != right;
        }
        case test_number_equal: {
            return parse_number(left, &left_num) && parse_number(right, &right_num) &&
                   left_num == right_num;
        }
        case test_number_not_equal: {
            return parse_number(left, &left_num) && parse_number(right, &right_num) &&
                   left_num != right_num;
        }
        case test_number_greater: {
            return parse_number(left, &left_num) && parse_number(right, &right_num) &&
                   left_num > right_num;
        }
        case test_number_greater_equal: {
            return parse_number(left, &left_num) && parse_number(right, &right_num) &&
                   left_num >= right_num;
        }
        case test_number_lesser: {
            return parse_number(left, &left_num) && parse_number(right, &right_num) &&
                   left_num < right_num;
        }
        case test_number_lesser_equal: {
            return parse_number(left, &left_num) && parse_number(right, &right_num) &&
                   left_num <= right_num;
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
    long long num;
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
            return parse_number(arg, &num) && num == (int)num && isatty((int)num);
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
};

/// Evaluate a conditional expression given the arguments. If fromtest is set, the caller is the
/// test or [ builtin; with the pointer giving the name of the command. for POSIX conformance this
/// supports a more limited range of functionality.
///
/// Return status is the final shell status, i.e. 0 for true, 1 for false and 2 for error.
int builtin_test(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    using namespace test_expressions;

    // The first argument should be the name of the command ('test').
    if (!argv[0]) return BUILTIN_TEST_FAIL;

    // Whether we are invoked with bracket '[' or not.
    const bool is_bracket = !wcscmp(argv[0], L"[");

    size_t argc = 0;
    while (argv[argc + 1]) argc++;

    // If we're bracket, the last argument ought to be ]; we ignore it. Note that argc is the number
    // of arguments after the command name; thus argv[argc] is the last argument.
    if (is_bracket) {
        if (!wcscmp(argv[argc], L"]")) {
            // Ignore the closing bracketp.
            argc--;
        } else {
            streams.err.append(L"[: the last argument must be ']'\n");
            return BUILTIN_TEST_FAIL;
        }
    }

    // Collect the arguments into a list.
    const wcstring_list_t args(argv + 1, argv + 1 + argc);

    switch (argc) {
        case 0: {
            // Per 1003.1, exit false.
            return BUILTIN_TEST_FAIL;
        }
        case 1: {
            // Per 1003.1, exit true if the arg is non-empty.
            return args.at(0).empty() ? BUILTIN_TEST_FAIL : BUILTIN_TEST_SUCCESS;
        }
        default: {
            // Try parsing. If expr is not nil, we are responsible for deleting it.
            wcstring err;
            expression *expr = test_parser::parse_args(args, err);
            if (!expr) {
#if 0
                printf("Oops! test was given args:\n");
                for (size_t i=0; i < argc; i++) {
                    printf("\t%ls\n", args.at(i).c_str());
                }
                printf("and returned parse error: %ls\n", err.c_str());
#endif
                streams.err.append(err);
                return BUILTIN_TEST_FAIL;
            }

            wcstring_list_t eval_errors;
            bool result = expr->evaluate(eval_errors);
            if (!eval_errors.empty()) {
                printf("test returned eval errors:\n");
                for (size_t i = 0; i < eval_errors.size(); i++) {
                    printf("\t%ls\n", eval_errors.at(i).c_str());
                }
            }
            delete expr;
            return result ? BUILTIN_TEST_SUCCESS : BUILTIN_TEST_FAIL;
        }
    }
    return 1;
}
