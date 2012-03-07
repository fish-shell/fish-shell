/** \file builtin_test.cpp Functions defining the test builtin

Functions used for implementing the test builtin. 
Implemented from scratch (yes, really) by way of IEEE 1003.1 as reference.

*/

#include "config.h"
#include "common.h"
#include "builtin.h"
#include "wutil.h"
#include "proc.h"
#include <sys/stat.h>
#include <memory>


enum {
    BUILTIN_TEST_SUCCESS = STATUS_BUILTIN_OK,
    BUILTIN_TEST_FAIL = STATUS_BUILTIN_ERROR
};


int builtin_test( parser_t &parser, wchar_t **argv );

static const wchar_t * const condstr[] = {
    L"!", L"&&", L"||", L"==", L"!=", L"<", L">", L"-nt", L"-ot", L"-ef", L"-eq",
    L"-ne", L"-lt", L"-gt", L"-le", L"-ge", L"=~"
};

namespace test_expressions {
    
    enum token_t {
        test_unknown,               // arbitrary string
        
        test_bang,                  // "!", inverts sense
        
        test_filetype_b,            // "-b", for block special files
        test_filetype_c,            // "-c" for character special files
        test_filetype_d,            // "-d" for directories
        test_filetype_e,            // "-e" for files that exist
        test_filetype_f,            // "-f" for for regular files
        test_filetype_g,            // "-g" for set-group-id
        test_filetype_h,            // "-h" for symbolic links
        test_filetype_L,            // "-L", same as -h
        test_filetype_p,            // "-p", for FIFO
        test_filetype_S,            // "-S", socket
        
        test_filesize_s,            // "-s", size greater than zero
        
        test_filedesc_t,            // "-t", whether the fd is associated with a terminal
        
        test_fileperm_r,            // "-r", read permission
        test_fileperm_u,            // "-u", whether file is setuid
        test_fileperm_w,            // "-w", whether file write permission is allowed
        test_fileperm_x,            // "-x", whether file execute/search is allowed
        
        test_string_n,              // "-n", non-empty string
        test_string_z,              // "-z", true if length of string is 0
        test_string_equal,          // "=", true if strings are identical
        test_string_not_equal,      // "!=", true if strings are not identical
        
        test_number_equal,          // "-eq", true if numbers are equal
        test_number_not_equal,      // "-ne", true if numbers are not equal
        test_number_greater,        // "-gt", true if first number is larger than second
        test_number_greater_equal,  // "-ge", true if first number is at least second
        test_number_lesser,         // "-lt", true if first number is smaller than second
        test_number_lesser_equal,   // "-le", true if first number is at most second
        
        test_combine_and,            // "-a", true if left and right are both true
        test_combine_or             // "-o", true if either left or right is true
    };
    
    static bool binary_primary_evaluate(test_expressions::token_t token, const wcstring &left, const wcstring &right, wcstring_list_t &errors);
    static bool unary_primary_evaluate(test_expressions::token_t token, const wcstring &arg, wcstring_list_t &errors);

    
    enum {
        UNARY_PRIMARY = 1 << 0,
        BINARY_PRIMARY = 1 << 1
    };
    
    static const struct token_info_t { token_t tok; const wchar_t *string; unsigned int flags; } token_infos[] =
    {
        {test_unknown, L"", 0},
        {test_bang, L"!", 0},
        {test_filetype_b, L"-b", UNARY_PRIMARY},
        {test_filetype_c, L"-c", UNARY_PRIMARY},
        {test_filetype_d, L"-d", UNARY_PRIMARY},
        {test_filetype_e, L"-e", UNARY_PRIMARY},
        {test_filetype_f, L"-f", UNARY_PRIMARY},
        {test_filetype_g, L"-g", UNARY_PRIMARY},
        {test_filetype_h, L"-h", UNARY_PRIMARY},
        {test_filetype_L, L"-L", UNARY_PRIMARY},
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
        {test_combine_or, L"-o", 0}
    };
    
    const token_info_t *token_for_string(const wcstring &str) {
        for (size_t i=0; i < sizeof token_infos / sizeof *token_infos; i++) {
            if (str == token_infos[i].string) {
                return &token_infos[i];
            }
        }
        return &token_infos[0];
    }

    
    /* Grammar.
    
        <expr> = <combining_expr>
                 
        <combining_expr> = <unary_expr> and/or <combining_expr> |
                           <combining_expr>
        
        <unary_expr> = bang <unary_expr> |
                      <primary>
                     
        <primary> = <unary_primary> arg |
                    arg <binary_primary> arg
                
    */
    
    class expression;
    class test_parser {
        private:
        wcstring_list_t strings;
        wcstring_list_t errors;
        
        expression *error(const wchar_t *fmt, ...);
        void add_error(const wchar_t *fmt, ...);
        
        const wcstring &arg(unsigned int idx) { return strings.at(idx); }
        
        public:
        test_parser(const wcstring_list_t &val) : strings(val)
        { }
        
        expression *parse_expression(unsigned int start, unsigned int end);
        expression *parse_combining_expression(unsigned int start, unsigned int end);
        expression *parse_unary_expression(unsigned int start, unsigned int end);
        
        expression *parse_primary(unsigned int start, unsigned int end);
        expression *parse_unary_primary(unsigned int start, unsigned int end);
        expression *parse_binary_primary(unsigned int start, unsigned int end);
        
        static expression *parse_args(const wcstring_list_t &args, wcstring &err);
    };
    
    struct range_t {
        unsigned int start;
        unsigned int end;
        
        range_t(unsigned s, unsigned e) : start(s), end(e) { }
    };


    /* Base class for expressions */
    class expression {
        protected:
        expression(token_t what, range_t where) : token(what), range(where) { }
        
        public:
        const token_t token;
        range_t range;
        
        virtual ~expression() { }
        
        // evaluate returns true if the expression is true (i.e. BUILTIN_TEST_SUCCESS)
        virtual bool evaluate(wcstring_list_t &errors) = 0;
    };

    typedef std::auto_ptr<expression> expr_ref_t;

    /* Single argument like -n foo */
    class unary_primary : public expression {
        public:
        wcstring arg;
        unary_primary(token_t tok, range_t where, const wcstring &what) : expression(tok, where), arg(what) { }
        bool evaluate(wcstring_list_t &errors);
    };

    /* Two argument primary like foo != bar */
    class binary_primary : public expression {
        public:
        wcstring arg_left;
        wcstring arg_right;
        
        binary_primary(token_t tok, range_t where, const wcstring &left, const wcstring &right) : expression(tok, where), arg_left(left), arg_right(right)
        { }
        bool evaluate(wcstring_list_t &errors);
    };
    
    /* Unary operator like bang */
    class unary_operator : public expression {
        public:
        expr_ref_t subject;        
        unary_operator(token_t tok, range_t where, expr_ref_t &exp) : expression(tok, where), subject(exp) { }
        bool evaluate(wcstring_list_t &errors);
    };
    
    /* Combining expression. Contains a list of AND or OR expressions. It takes more than two so that we don't have to worry about precedence in the parser. */
    class combining_expression : public expression {
        public:
        const std::vector<expression *> subjects;
        const std::vector<token_t> combiners;
        
        combining_expression(token_t tok, range_t where, const std::vector<expression *> &exprs, const std::vector<token_t> &combs) : expression(tok, where), subjects(exprs), combiners(combs)
        {
            /* We should have one more subject than combiner */
            assert(subjects.size() == combiners.size() + 1);
        }
        
        /* We are responsible for destroying our expressions */
        virtual ~combining_expression() {
            for (size_t i=0; i < subjects.size(); i++) {
                delete subjects[i];
            }
        }
        
        bool evaluate(wcstring_list_t &errors);
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
            } else {
                return NULL;
            }
        } else {
            return parse_primary(start, end);
        }
    }
    
    /* Parse a combining expression (AND, OR) */
    expression *test_parser::parse_combining_expression(unsigned int start, unsigned int end) {
        if (start >= end)
            return NULL;
        
        std::vector<expression *> subjects;
        std::vector<token_t> combiners;
        unsigned int idx = start;
        
        while (idx < end) {
            
            if (! subjects.empty()) {
                /* This is not the first expression, so we need a combiner. */
                token_t combiner = token_for_string(arg(idx))->tok;
                if (combiner != test_combine_and && combiner != test_combine_or) {
                    add_error(L"Expected combining argument at index %u", idx);
                    break;
                }
                combiners.push_back(combiner);
                idx++;
            }
            
            /* Parse another expression */
            expression *expr = parse_unary_expression(idx, end);
            if (! expr) {
                add_error(L"Missing argument at index %u", idx);
                break;
            }
            
            /* Go to the end of this expression */
            idx = expr->range.end;
            subjects.push_back(expr);
        }
        
        if (idx >= end) {
            /* We succeeded. Our new expression takes ownership of all expressions we created. The token we pass is irrelevant. */
            return new combining_expression(test_combine_and, range_t(start, idx), subjects, combiners);
        } else {
            /* Failure; we must delete our expressions since we own them */
            while (! subjects.empty()) {
                delete subjects.back();
                subjects.pop_back();
            }
            return NULL;
        }
    }
    
    expression *test_parser::parse_unary_primary(unsigned int start, unsigned int end) {
        /* We need two arguments */
        if (start >= end) {
            return error(L"Missing argument at index %u", start);
        }
        if (start + 1 >= end) {
            return error(L"Missing argument at index %u", start + 1);
        }
        
        /* All our unary primaries are prefix, so the operator is at start. */
        const token_info_t *info = token_for_string(arg(start));
        if (! (info->flags & UNARY_PRIMARY))
            return NULL;
        
        return new unary_primary(info->tok, range_t(start, start + 2), arg(start + 1));
    }
    
    expression *test_parser::parse_binary_primary(unsigned int start, unsigned int end) {
        /* We need three arguments */
        for (unsigned int idx = start; idx < start + 3; idx++) {
            if (idx >= end) {
                return error(L"Missing argument at index %u", idx);
            }
        }
        
        /* All our binary primaries are infix, so the operator is at start + 1. */
        const token_info_t *info = token_for_string(arg(start + 1));
        if (! (info->flags & BINARY_PRIMARY))
            return NULL;
        
        return new binary_primary(info->tok, range_t(start, start + 3), arg(start), arg(start + 2));
    }

    expression *test_parser::parse_primary(unsigned int start, unsigned int end) {
        if (start >= end) {
            return error(L"Missing argument at index %u", start);
        }
        
        expression *expr = parse_unary_primary(start, end);
        if (! expr) expr = parse_binary_primary(start, end);
        return expr;
    }

    expression *test_parser::parse_expression(unsigned int start, unsigned int end) {
        if (start >= end) {
            return error(L"Missing argument at index %u", start);
        }
        
        return parse_combining_expression(start, end);
    }
    
    
    expression *test_parser::parse_args(const wcstring_list_t &args, wcstring &err) {
        /* Empty list and one-arg list should be handled by caller */
        assert(args.size() > 1);
        
        test_parser parser(args);
        expression *result = parser.parse_expression(0, (unsigned int)args.size());
        /* Handle errors */
        for (size_t i = 0; i < parser.errors.size(); i++) {
            if (i > 0)
                err.push_back(L'\n');
            err.append(parser.errors.at(i));
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
            case test_bang:
                assert(subject.get());
                return ! subject->evaluate(errors);
            default:
                errors.push_back(format_string(L"Unknown token type in %s", __func__));
                return false;

        }
    }
    
    bool combining_expression::evaluate(wcstring_list_t &errors) {
        switch (token) {
            case test_combine_and:
            case test_combine_or:
            {
                /* One-element case */
                if (subjects.size() == 1)
                    return subjects.at(0)->evaluate(errors);
                
                /* Evaluate our lists, remembering that AND has higher precedence than OR. We can visualize this as a sequence of OR expressions of AND expressions. */
                assert(combiners.size() + 1 == subjects.size());
                assert(! subjects.empty());
                
                size_t idx = 0, max = combiners.size();
                bool or_result = false;
                while (idx < max) {
                    if (or_result) {
                        /* Short circuit */
                        break;
                    }
                    
                    /* Evaluate a stream of AND starting at given subject index. It may only have one element.  */
                    bool and_result = true;
                    for (; idx < max; idx++) {
                        /* Evaluate it, short-circuiting */
                        and_result = and_result && subjects.at(idx)->evaluate(errors);
                        
                        /* If the combiner at this index (which corresponding to how we combine with the next subject) is not AND, then exit the loop */
                        if (idx + 1 < max && combiners.at(idx) != test_combine_and) {
                            idx++;
                            break;
                        }
                    }
                    
                    /* OR it in */
                    or_result = or_result || and_result;
                }
                return or_result;
            }
            
            default:
                errors.push_back(format_string(L"Unknown token type in %s", __func__));
                return BUILTIN_TEST_FAIL;

        }
    }

    /* IEEE 1003.1 says nothing about what it means for two strings to be "algebraically equal". For example, should we interpret 0x10 as 0, 10, or 16? Here we use only base 10 and use wcstoll, which allows for leading + and -, and leading whitespace. This matches bash. */
    static bool parse_number(const wcstring &arg, long long *out) {
        const wchar_t *str = arg.c_str();
        wchar_t *endptr = NULL;
        *out = wcstoll(str, &endptr, 10);
        return endptr && *endptr == L'\0';
    }

    static bool binary_primary_evaluate(test_expressions::token_t token, const wcstring &left, const wcstring &right, wcstring_list_t &errors) {
        using namespace test_expressions;
        long long left_num, right_num;
        switch (token) {
            case test_string_equal:
                return left == right;
            
            case test_string_not_equal:
                return left != right;
            
            case test_number_equal:
                return parse_number(left, &left_num) && parse_number(right, &right_num) && left_num == right_num;
                
            case test_number_not_equal:
                return parse_number(left, &left_num) && parse_number(right, &right_num) && left_num != right_num;
            
            case test_number_greater:
                return parse_number(left, &left_num) && parse_number(right, &right_num) && left_num > right_num;
                
            case test_number_greater_equal:
                return parse_number(left, &left_num) && parse_number(right, &right_num) && left_num >= right_num;
                
            case test_number_lesser:
                return parse_number(left, &left_num) && parse_number(right, &right_num) && left_num < right_num;
                
            case test_number_lesser_equal:
                return parse_number(left, &left_num) && parse_number(right, &right_num) && left_num <= right_num;
                
            default:
                errors.push_back(format_string(L"Unknown token type in %s", __func__));
                return false;
        }
    }


    static bool unary_primary_evaluate(test_expressions::token_t token, const wcstring &arg, wcstring_list_t &errors) {
        using namespace test_expressions;
        struct stat buf;
        long long num;
        switch (token) {
            case test_filetype_b:            // "-b", for block special files
                return !wstat(arg, &buf) && S_ISBLK(buf.st_mode);
                
            case test_filetype_c:            // "-c" for character special files
                return !wstat(arg, &buf) && S_ISCHR(buf.st_mode);
                
            case test_filetype_d:            // "-d" for directories
                return !wstat(arg, &buf) && S_ISDIR(buf.st_mode);
                
            case test_filetype_e:            // "-e" for files that exist
                return !wstat(arg, &buf);
                
            case test_filetype_f:            // "-f" for for regular files
                return !wstat(arg, &buf) && S_ISREG(buf.st_mode);
            
            case test_filetype_g:            // "-g" for set-group-id
                return !wstat(arg, &buf) && (S_ISGID & buf.st_mode);
                
            case test_filetype_h:            // "-h" for symbolic links
            case test_filetype_L:            // "-L", same as -h
                return !lwstat(arg, &buf) && S_ISLNK(buf.st_mode);

            case test_filetype_p:            // "-p", for FIFO
                return !wstat(arg, &buf) && S_ISFIFO(buf.st_mode);
                
            case test_filetype_S:            // "-S", socket
                return !wstat(arg, &buf) && S_ISSOCK(buf.st_mode);
            
            case test_filesize_s:            // "-s", size greater than zero
                return !wstat(arg, &buf) && buf.st_size > 0;
                
            case test_filedesc_t:            // "-t", whether the fd is associated with a terminal
                return parse_number(arg, &num) && num == (int)num && isatty((int)num);
            
            case test_fileperm_r:            // "-r", read permission
                return !waccess(arg, R_OK);
                
            case test_fileperm_u:            // "-u", whether file is setuid
                return !wstat(arg, &buf) && (S_ISUID & buf.st_mode);
                
            case test_fileperm_w:            // "-w", whether file write permission is allowed
                return !waccess(arg, W_OK);
                
            case test_fileperm_x:            // "-x", whether file execute/search is allowed
                return !waccess(arg, X_OK);
            
            case test_string_n:              // "-n", non-empty string
                return ! arg.empty();
                
            case test_string_z:              // "-z", true if length of string is 0
                return arg.empty();
            
            default:
                errors.push_back(format_string(L"Unknown token type in %s", __func__));
                return false;
        }        
    }

};

/*
 * Evaluate a conditional expression given the arguments.
 * If fromtest is set, the caller is the test or [ builtin;
 * with the pointer giving the name of the command.
 * for POSIX conformance this supports a more limited range
 * of functionality.
 *
 * Return status is the final shell status, i.e. 0 for true,
 * 1 for false and 2 for error.
 */
int builtin_test( parser_t &parser, wchar_t **argv )
{
    using namespace test_expressions;
    
    /* The first argument should be the name of the command ('test') */
    if (! argv[0])
        return BUILTIN_TEST_FAIL;
    
    size_t argc = 0;
    while (argv[argc + 1])
        argc++;
    const wcstring_list_t args(argv + 1, argv + 1 + argc);

    if (argc == 0) {
        // Per 1003.1, exit false
        return BUILTIN_TEST_FAIL;
    } else if (argc == 1) {
        // Per 1003.1, exit true if the arg is non-empty
        return args.at(0).empty() ? BUILTIN_TEST_FAIL : BUILTIN_TEST_SUCCESS;
    } else {
        // Try parsing. If expr is not nil, we are responsible for deleting it.
        wcstring err;
        expression *expr = test_parser::parse_args(args, err);
        if (! expr) {
#if 0
            printf("Oops! test was given args:\n");
            for (size_t i=0; i < argc; i++) {
                printf("\t%ls\n", args.at(i).c_str());
            }
            printf("and returned parse error: %ls\n", err.c_str());
#endif
            builtin_show_error(err);
            return BUILTIN_TEST_FAIL;
        } else {
            wcstring_list_t eval_errors;
            bool result = expr->evaluate(eval_errors);
            if (! eval_errors.empty()) {
                printf("test returned eval errors:\n");
                for (size_t i=0; i < eval_errors.size(); i++) {
                    printf("\t%ls\n", eval_errors.at(i).c_str());
                }
            }
            delete expr;
            return result ? BUILTIN_TEST_SUCCESS : BUILTIN_TEST_FAIL;
        }
    }
    return 1;
}
