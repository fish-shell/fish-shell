/*
 * TINYEXPR - Tiny recursive descent parser and evaluation engine in C
 *
 * Copyright (c) 2015, 2016 Lewis Van Winkle
 *
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgement in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

// This version has been altered and ported to C++ for inclusion in fish.
#include "tinyexpr.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <utility>

#include "fallback.h"  // IWYU pragma: keep
#include "wutil.h"

// TODO: It would be nice not to rely on a typedef for this, especially one that can only do
// functions with two args.
using te_fun2 = double (*)(double, double);
using te_fun1 = double (*)(double);
using te_fun0 = double (*)();

enum {
    TE_CONSTANT = 0,
    TE_FUNCTION0,
    TE_FUNCTION1,
    TE_FUNCTION2,
    TOK_NULL,
    TOK_ERROR,
    TOK_END,
    TOK_SEP,
    TOK_OPEN,
    TOK_CLOSE,
    TOK_NUMBER,
    TOK_INFIX
};

static int get_arity(const int type) {
    if (type == TE_FUNCTION2) return 2;
    if (type == TE_FUNCTION1) return 1;
    return 0;
}

struct te_expr_t {
    int type;
    union {
        double value;
        te_fun0 fun0;
        te_fun1 fun1;
        te_fun2 fun2;
    };
    te_expr_t *parameters[];
};

struct te_builtin {
    const wchar_t *name;
    void *address;
    int type;
};

struct state {
    explicit state(const wchar_t *expr) : start{expr}, next{expr} { next_token(); }
    te_expr_t *expr();

    union {
        double value;
        void *function;
    };
    const wchar_t *start;
    const wchar_t *next;
    int type;
    te_error_type_t error{TE_ERROR_NONE};

   private:
    void next_token();

    te_expr_t *power();
    te_expr_t *base();
    te_expr_t *factor();
    te_expr_t *term();
};

// TODO: That move there? Ouch. Replace with a proper class with a constructor.
#define NEW_EXPR(type, ...) new_expr((type), std::move((const te_expr_t *[]){__VA_ARGS__}))

static te_expr_t *new_expr(const int type, const te_expr_t *parameters[]) {
    const int arity = get_arity(type);
    const int psize = sizeof(te_expr_t *) * arity;
    const int size = sizeof(te_expr_t) + psize;
    auto ret = static_cast<te_expr_t *>(malloc(size));
    // This sets float to 0, which depends on the implementation.
    // We rely on IEEE-754 floats anyway, so it's okay.
    std::memset(ret, 0, size);
    if (arity && parameters) {
        std::memcpy(ret->parameters, parameters, psize);
    }
    ret->type = type;
    return ret;
}

/* Frees the expression. */
/* This is safe to call on NULL pointers. */
static void te_free(te_expr_t *n);

static void te_free_parameters(te_expr_t *n) {
    if (!n) return;
    int arity = get_arity(n->type);
    // Free all parameters from the back to the front.
    while (arity > 0) {
        te_free(n->parameters[arity - 1]);
        arity--;
    }
}

static void te_free(te_expr_t *n) {
    if (!n) return;
    te_free_parameters(n);
    free(n);
}

static constexpr double pi() { return M_PI; }
static constexpr double tau() { return 2 * M_PI; }
static constexpr double e() { return M_E; }

static double fac(double a) { /* simplest version of fac */
    if (a < 0.0) return NAN;
    if (a > UINT_MAX) return INFINITY;
    auto ua = static_cast<unsigned int>(a);
    unsigned long int result = 1, i;
    for (i = 1; i <= ua; i++) {
        if (i > ULONG_MAX / result) return INFINITY;
        result *= i;
    }
    return static_cast<double>(result);
}

static double ncr(double n, double r) {
    // Doing this for NAN takes ages - just return the result right away.
    if (std::isnan(n)) return INFINITY;
    if (n < 0.0 || r < 0.0 || n < r) return NAN;
    if (n > UINT_MAX || r > UINT_MAX) return INFINITY;
    unsigned long int un = static_cast<unsigned int>(n), ur = static_cast<unsigned int>(r), i;
    unsigned long int result = 1;
    if (ur > un / 2) ur = un - ur;
    for (i = 1; i <= ur; i++) {
        if (result > ULONG_MAX / (un - ur + i)) return INFINITY;
        result *= un - ur + i;
        result /= i;
    }
    return result;
}

static double npr(double n, double r) { return ncr(n, r) * fac(r); }

static constexpr double bit_and(double a, double b) {
    return static_cast<double>(static_cast<long long>(a) & static_cast<long long>(b));
}

static constexpr double bit_or(double a, double b) {
    return static_cast<double>(static_cast<long long>(a) | static_cast<long long>(b));
}

static constexpr double bit_xor(double a, double b) {
    return static_cast<double>(static_cast<long long>(a) ^ static_cast<long long>(b));
}

static double max(double a, double b) {
    if (std::isnan(a)) return a;
    if (std::isnan(b)) return b;
    if (a == b) return std::signbit(a) ? b : a;  // treat +0 as larger than -0
    return a > b ? a : b;
}

static double min(double a, double b) {
    if (std::isnan(a)) return a;
    if (std::isnan(b)) return b;
    if (a == b) return std::signbit(a) ? a : b;  // treat -0 as smaller than +0
    return a < b ? a : b;
}

static const te_builtin functions[] = {
    /* must be in alphabetical order */
    {L"abs", reinterpret_cast<void *>(static_cast<te_fun1>(std::fabs)), TE_FUNCTION1},
    {L"acos", reinterpret_cast<void *>(static_cast<te_fun1>(std::acos)), TE_FUNCTION1},
    {L"asin", reinterpret_cast<void *>(static_cast<te_fun1>(std::asin)), TE_FUNCTION1},
    {L"atan", reinterpret_cast<void *>(static_cast<te_fun1>(std::atan)), TE_FUNCTION1},
    {L"atan2", reinterpret_cast<void *>(static_cast<te_fun2>(std::atan2)), TE_FUNCTION2},
    {L"bitand", reinterpret_cast<void *>(static_cast<te_fun2>(bit_and)), TE_FUNCTION2},
    {L"bitor", reinterpret_cast<void *>(static_cast<te_fun2>(bit_or)), TE_FUNCTION2},
    {L"bitxor", reinterpret_cast<void *>(static_cast<te_fun2>(bit_xor)), TE_FUNCTION2},
    {L"ceil", reinterpret_cast<void *>(static_cast<te_fun1>(std::ceil)), TE_FUNCTION1},
    {L"cos", reinterpret_cast<void *>(static_cast<te_fun1>(std::cos)), TE_FUNCTION1},
    {L"cosh", reinterpret_cast<void *>(static_cast<te_fun1>(std::cosh)), TE_FUNCTION1},
    {L"e", reinterpret_cast<void *>(static_cast<te_fun0>(e)), TE_FUNCTION0},
    {L"exp", reinterpret_cast<void *>(static_cast<te_fun1>(std::exp)), TE_FUNCTION1},
    {L"fac", reinterpret_cast<void *>(static_cast<te_fun1>(fac)), TE_FUNCTION1},
    {L"floor", reinterpret_cast<void *>(static_cast<te_fun1>(std::floor)), TE_FUNCTION1},
    {L"ln", reinterpret_cast<void *>(static_cast<te_fun1>(std::log)), TE_FUNCTION1},
    {L"log", reinterpret_cast<void *>(static_cast<te_fun1>(std::log10)), TE_FUNCTION1},
    {L"log10", reinterpret_cast<void *>(static_cast<te_fun1>(std::log10)), TE_FUNCTION1},
    {L"log2", reinterpret_cast<void *>(static_cast<te_fun1>(std::log2)), TE_FUNCTION1},
    {L"max", reinterpret_cast<void *>(static_cast<te_fun2>(max)), TE_FUNCTION2},
    {L"min", reinterpret_cast<void *>(static_cast<te_fun2>(min)), TE_FUNCTION2},
    {L"ncr", reinterpret_cast<void *>(static_cast<te_fun2>(ncr)), TE_FUNCTION2},
    {L"npr", reinterpret_cast<void *>(static_cast<te_fun2>(npr)), TE_FUNCTION2},
    {L"pi", reinterpret_cast<void *>(static_cast<te_fun0>(pi)), TE_FUNCTION0},
    {L"pow", reinterpret_cast<void *>(static_cast<te_fun2>(std::pow)), TE_FUNCTION2},
    {L"round", reinterpret_cast<void *>(static_cast<te_fun1>(std::round)), TE_FUNCTION1},
    {L"sin", reinterpret_cast<void *>(static_cast<te_fun1>(std::sin)), TE_FUNCTION1},
    {L"sinh", reinterpret_cast<void *>(static_cast<te_fun1>(std::sinh)), TE_FUNCTION1},
    {L"sqrt", reinterpret_cast<void *>(static_cast<te_fun1>(std::sqrt)), TE_FUNCTION1},
    {L"tan", reinterpret_cast<void *>(static_cast<te_fun1>(std::tan)), TE_FUNCTION1},
    {L"tanh", reinterpret_cast<void *>(static_cast<te_fun1>(std::tanh)), TE_FUNCTION1},
    {L"tau", reinterpret_cast<void *>(static_cast<te_fun0>(tau)), TE_FUNCTION0},
};

static const te_builtin *find_builtin(const wchar_t *name, int len) {
    const auto end = std::end(functions);
    const te_builtin *found = std::lower_bound(std::begin(functions), end, name,
                                               [len](const te_builtin &lhs, const wchar_t *rhs) {
                                                   // The length is important because that's where
                                                   // the parens start
                                                   return std::wcsncmp(lhs.name, rhs, len) < 0;
                                               });
    // We need to compare again because we might have gotten the first "larger" element.
    if (found != end && std::wcsncmp(found->name, name, len) == 0 && found->name[len] == 0)
        return found;
    return nullptr;
}

static constexpr double add(double a, double b) { return a + b; }
static constexpr double sub(double a, double b) { return a - b; }
static constexpr double mul(double a, double b) { return a * b; }
static constexpr double divide(double a, double b) {
    // If b isn't zero, divide.
    // If a isn't zero, return signed INFINITY.
    // Else, return NAN.
    return b ? a / b : a ? copysign(1, a) * copysign(1, b) * INFINITY : NAN;
}

static constexpr double negate(double a) { return -a; }

void state::next_token() {
    type = TOK_NULL;

    do {
        if (!*next) {
            type = TOK_END;
            return;
        }

        /* Try reading a number. */
        if ((next[0] >= '0' && next[0] <= '9') || next[0] == '.') {
            value = fish_wcstod(next, const_cast<wchar_t **>(&next));
            type = TOK_NUMBER;
        } else {
            /* Look for a function call. */
            // But not when it's an "x" followed by whitespace
            // - that's the alternative multiplication operator.
            if (next[0] >= 'a' && next[0] <= 'z' && !(next[0] == 'x' && isspace(next[1]))) {
                const wchar_t *start;
                start = next;
                while ((next[0] >= 'a' && next[0] <= 'z') || (next[0] >= '0' && next[0] <= '9') ||
                       (next[0] == '_'))
                    next++;

                const te_builtin *var = find_builtin(start, next - start);

                if (var) {
                    switch (var->type) {
                        case TE_FUNCTION0:
                        case TE_FUNCTION1:
                        case TE_FUNCTION2:
                            type = var->type;
                            function = var->address;
                            break;
                    }
                } else if (type != TOK_ERROR || error == TE_ERROR_UNKNOWN) {
                    // Our error is more specific, so it takes precedence.
                    type = TOK_ERROR;
                    error = TE_ERROR_UNKNOWN_FUNCTION;
                }
            } else {
                /* Look for an operator or special character. */
                switch (next++[0]) {
                    // The "te_fun2" casts are necessary to pick the right overload.
                    case '+':
                        type = TOK_INFIX;
                        function = reinterpret_cast<void *>(static_cast<te_fun2>(add));
                        break;
                    case '-':
                        type = TOK_INFIX;
                        function = reinterpret_cast<void *>(static_cast<te_fun2>(sub));
                        break;
                    case 'x':
                    case '*':
                        // We've already checked for whitespace above.
                        type = TOK_INFIX;
                        function = reinterpret_cast<void *>(static_cast<te_fun2>(mul));
                        break;
                    case '/':
                        type = TOK_INFIX;
                        function = reinterpret_cast<void *>(static_cast<te_fun2>(divide));
                        break;
                    case '^':
                        type = TOK_INFIX;
                        function = reinterpret_cast<void *>(static_cast<te_fun2>(pow));
                        break;
                    case '%':
                        type = TOK_INFIX;
                        function = reinterpret_cast<void *>(static_cast<te_fun2>(fmod));
                        break;
                    case '(':
                        type = TOK_OPEN;
                        break;
                    case ')':
                        type = TOK_CLOSE;
                        break;
                    case ',':
                        type = TOK_SEP;
                        break;
                    case ' ':
                    case '\t':
                    case '\n':
                    case '\r':
                        break;
                    case '=':
                    case '>':
                    case '<':
                    case '&':
                    case '|':
                    case '!':
                        type = TOK_ERROR;
                        error = TE_ERROR_LOGICAL_OPERATOR;
                        break;
                    default:
                        type = TOK_ERROR;
                        error = TE_ERROR_MISSING_OPERATOR;
                        break;
                }
            }
        }
    } while (type == TOK_NULL);
}

te_expr_t *state::base() {
    /* <base>      =    <constant> | <function-0> {"(" ")"} | <function-1> <power> |
     * <function-X> "(" <expr> {"," <expr>} ")" | "(" <list> ")" */
    te_expr_t *ret;
    int arity;

    auto previous = start;
    auto next = this->next;
    switch (type) {
        case TOK_NUMBER:
            ret = new_expr(TE_CONSTANT, nullptr);
            ret->value = value;
            next_token();
            if (type == TOK_NUMBER || type == TE_FUNCTION0) {
                // Two numbers after each other:
                // math '5 2'
                // math '3 pi'
                // (of course 3 pi could also be interpreted as 3 x pi)
                type = TOK_ERROR;
                error = TE_ERROR_MISSING_OPERATOR;
                // The error should be given *between*
                // the last two tokens.
                // Since these are two separate numbers there is at least
                // one space between.
                start = previous;
                this->next = next + 1;
            }
            break;

        case TE_FUNCTION0:
            ret = new_expr(type, nullptr);
            ret->fun0 = reinterpret_cast<te_fun0>(function);
            next_token();
            if (type == TOK_OPEN) {
                next_token();
                if (type == TOK_CLOSE) {
                    next_token();
                } else if (type != TOK_ERROR || error == TE_ERROR_UNKNOWN) {
                    type = TOK_ERROR;
                    error = TE_ERROR_MISSING_CLOSING_PAREN;
                }
            }
            break;

        case TE_FUNCTION1:
        case TE_FUNCTION2: {
            arity = get_arity(type);

            ret = new_expr(type, nullptr);
            ret->fun0 = reinterpret_cast<te_fun0>(function);
            next_token();

            bool have_open = false;
            if (type == TOK_OPEN) {
                // If we *have* an opening parenthesis,
                // we need to consume it and
                // expect a closing one.
                have_open = true;
                next_token();
            }

            int i;
            for (i = 0; i < arity; i++) {
                ret->parameters[i] = expr();
                if (type != TOK_SEP) {
                    break;
                }
                next_token();
            }

            if (!have_open && i == arity - 1) {
                break;
            }

            if (have_open && type == TOK_CLOSE && i == arity - 1) {
                // We have an opening and a closing paren, consume the closing one and done.
                next_token();
            } else if (type != TOK_ERROR || error == TE_ERROR_UNEXPECTED_TOKEN) {
                // If we had the right number of arguments, we're missing a closing paren.
                if (have_open && i == arity - 1 && type != TOK_ERROR) {
                    error = TE_ERROR_MISSING_CLOSING_PAREN;
                } else {
                    // Otherwise we complain about the number of arguments *first*,
                    // a closing parenthesis should be more obvious.
                    error = i < arity ? TE_ERROR_TOO_FEW_ARGS : TE_ERROR_TOO_MANY_ARGS;
                }
                type = TOK_ERROR;
            }

            break;
        }

        case TOK_OPEN:
            next_token();
            ret = expr();
            if (type == TOK_CLOSE) {
                next_token();
            } else if (type != TOK_ERROR && type != TOK_END && error == TE_ERROR_NONE) {
                type = TOK_ERROR;
                error = TE_ERROR_TOO_MANY_ARGS;
            } else if (type != TOK_ERROR || error == TE_ERROR_UNKNOWN) {
                type = TOK_ERROR;
                error = TE_ERROR_MISSING_CLOSING_PAREN;
            }
            break;

        case TOK_END:
            // The expression ended before we expected it.
            // e.g. `2 - `.
            // This means we have too few things.
            // Instead of introducing another error, just call it
            // "too few args".
            ret = new_expr(0, nullptr);
            type = TOK_ERROR;
            error = TE_ERROR_TOO_FEW_ARGS;
            ret->value = NAN;
            break;
        default:
            ret = new_expr(0, nullptr);
            if (type != TOK_ERROR || error == TE_ERROR_UNKNOWN) {
                type = TOK_ERROR;
                error = TE_ERROR_UNEXPECTED_TOKEN;
            }
            ret->value = NAN;
            break;
    }

    return ret;
}

te_expr_t *state::power() {
    /* <power>     =    {("-" | "+")} <base> */
    int sign = 1;
    while (type == TOK_INFIX && (function == add || function == sub)) {
        if (function == sub) sign = -sign;
        next_token();
    }

    te_expr_t *ret;

    if (sign == 1) {
        ret = base();
    } else {
        ret = NEW_EXPR(TE_FUNCTION1, base());
        ret->fun1 = negate;
    }

    return ret;
}

te_expr_t *state::factor() {
    /* <factor>    =    <power> {"^" <power>} */
    te_expr_t *ret = power();

    te_expr_t *insertion = nullptr;

    while (type == TOK_INFIX && (function == reinterpret_cast<void *>(static_cast<te_fun2>(pow)))) {
        auto t = reinterpret_cast<te_fun2>(function);
        next_token();

        if (insertion) {
            /* Make exponentiation go right-to-left. */
            te_expr_t *insert = NEW_EXPR(TE_FUNCTION2, insertion->parameters[1], power());
            insert->fun2 = t;
            insertion->parameters[1] = insert;
            insertion = insert;
        } else {
            ret = NEW_EXPR(TE_FUNCTION2, ret, power());
            ret->fun2 = t;
            insertion = ret;
        }
    }

    return ret;
}

te_expr_t *state::term() {
    /* <term>      =    <factor> {("*" | "/" | "%") <factor>} */
    te_expr_t *ret = factor();

    while (type == TOK_INFIX &&
           (function == reinterpret_cast<void *>(static_cast<te_fun2>(mul)) ||
            function == reinterpret_cast<void *>(static_cast<te_fun2>(divide)) ||
            function == reinterpret_cast<void *>(static_cast<te_fun2>(fmod)))) {
        auto t = reinterpret_cast<te_fun2>(function);
        next_token();
        ret = NEW_EXPR(TE_FUNCTION2, ret, factor());
        ret->fun2 = t;
    }

    return ret;
}

te_expr_t *state::expr() {
    /* <expr>      =    <term> {("+" | "-") <term>} */
    te_expr_t *ret = term();

    while (type == TOK_INFIX && (function == add || function == sub)) {
        auto t = reinterpret_cast<te_fun2>(function);
        next_token();
        ret = NEW_EXPR(TE_FUNCTION2, ret, term());
        ret->fun2 = t;
    }

    return ret;
}

#define M(e) te_eval(n->parameters[e])

/* Evaluates the expression. */
static double te_eval(const te_expr_t *n) {
    if (!n) return NAN;

    switch (n->type) {
        case TE_CONSTANT:
            return n->value;
        case TE_FUNCTION0:
            return n->fun0();
        case TE_FUNCTION1:
            return n->fun1(M(0));
        case TE_FUNCTION2:
            return n->fun2(M(0), M(1));
        default:
            return NAN;
    }
}

#undef M

static void optimize(te_expr_t *n) {
    /* Evaluates as much as possible. */
    if (!n || n->type == TE_CONSTANT) return;

    const int arity = get_arity(n->type);
    bool known = true;
    for (int i = 0; i < arity; ++i) {
        optimize(n->parameters[i]);
        if ((n->parameters[i])->type != TE_CONSTANT) {
            known = false;
        }
    }
    if (known) {
        const double value = te_eval(n);
        te_free_parameters(n);
        n->type = TE_CONSTANT;
        n->value = value;
    }
}

/* Parses the input expression. */
/* Returns NULL on error. */
static te_expr_t *te_compile(const wchar_t *expression, te_error_t *error) {
    state s{expression};

    te_expr_t *root = s.expr();

    if (s.type != TOK_END) {
        te_free(root);
        if (error) {
            error->position = (s.next - s.start) + 1;
            if (s.error != TE_ERROR_NONE) {
                error->type = s.error;
            } else {
                // If we're not at the end but there's no error, then that means we have a
                // superfluous token that we have no idea what to do with.
                error->type = TE_ERROR_TOO_MANY_ARGS;
            }
        }
        return nullptr;
    } else {
        optimize(root);
        if (error) error->position = 0;
        return root;
    }
}

double te_interp(const wchar_t *expression, te_error_t *error) {
    te_expr_t *n = te_compile(expression, error);
    double ret;
    if (n) {
        ret = te_eval(n);
        te_free(n);
    } else {
        ret = NAN;
    }
    return ret;
}
