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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <utility>

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
    TE_FUNCTION3,
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
    if (type == TE_FUNCTION3) return 3;
    if (type == TE_FUNCTION2) return 2;
    if (type == TE_FUNCTION1) return 1;
    return 0;
}

typedef struct te_expr {
    int type;
    union {
        double value;
        const void *function;
    };
    te_expr *parameters[];
} te_expr;

using te_builtin = struct {
    const char *name;
    const void *address;
    int type;
};

using state = struct {
    union {
        double value;
        const void *function;
    };
    const char *start;
    const char *next;
    int type;
    te_error_type_t error;
};

/* Parses the input expression. */
/* Returns NULL on error. */
te_expr *te_compile(const char *expression, te_error_t *error);

/* Evaluates the expression. */
double te_eval(const te_expr *n);

/* Frees the expression. */
/* This is safe to call on NULL pointers. */
void te_free(te_expr *n);

// TODO: That move there? Ouch. Replace with a proper class with a constructor.
#define NEW_EXPR(type, ...) new_expr((type), std::move((const te_expr *[]){__VA_ARGS__}))

static te_expr *new_expr(const int type, const te_expr *parameters[]) {
    const int arity = get_arity(type);
    const int psize = sizeof(te_expr *) * arity;
    const int size = sizeof(te_expr) + psize;
    auto ret = static_cast<te_expr *>(malloc(size));
    // This sets float to 0, which depends on the implementation.
    // We rely on IEEE-754 floats anyway, so it's okay.
    std::memset(ret, 0, size);
    if (arity && parameters) {
        std::memcpy(ret->parameters, parameters, psize);
    }
    ret->type = type;
    return ret;
}

static void te_free_parameters(te_expr *n) {
    if (!n) return;
    int arity = get_arity(n->type);
    // Free all parameters from the back to the front.
    while (arity > 0) {
        te_free(n->parameters[arity - 1]);
        arity--;
    }
}

void te_free(te_expr *n) {
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

static const te_builtin functions[] = {
    /* must be in alphabetical order */
    {"abs", reinterpret_cast<const void *>(static_cast<te_fun1>(fabs)), TE_FUNCTION1},
    {"acos", reinterpret_cast<const void *>(static_cast<te_fun1>(acos)), TE_FUNCTION1},
    {"asin", reinterpret_cast<const void *>(static_cast<te_fun1>(asin)), TE_FUNCTION1},
    {"atan", reinterpret_cast<const void *>(static_cast<te_fun1>(atan)), TE_FUNCTION1},
    {"atan2", reinterpret_cast<const void *>(static_cast<te_fun2>(atan2)), TE_FUNCTION2},
    {"bitand", reinterpret_cast<const void *>(static_cast<te_fun2>(bit_and)), TE_FUNCTION2},
    {"bitor", reinterpret_cast<const void *>(static_cast<te_fun2>(bit_or)), TE_FUNCTION2},
    {"bitxor", reinterpret_cast<const void *>(static_cast<te_fun2>(bit_xor)), TE_FUNCTION2},
    {"ceil", reinterpret_cast<const void *>(static_cast<te_fun1>(ceil)), TE_FUNCTION1},
    {"cos", reinterpret_cast<const void *>(static_cast<te_fun1>(cos)), TE_FUNCTION1},
    {"cosh", reinterpret_cast<const void *>(static_cast<te_fun1>(cosh)), TE_FUNCTION1},
    {"e", reinterpret_cast<const void *>(static_cast<te_fun0>(e)), TE_FUNCTION0},
    {"exp", reinterpret_cast<const void *>(static_cast<te_fun1>(exp)), TE_FUNCTION1},
    {"fac", reinterpret_cast<const void *>(static_cast<te_fun1>(fac)), TE_FUNCTION1},
    {"floor", reinterpret_cast<const void *>(static_cast<te_fun1>(floor)), TE_FUNCTION1},
    {"ln", reinterpret_cast<const void *>(static_cast<te_fun1>(log)), TE_FUNCTION1},
    {"log", reinterpret_cast<const void *>(static_cast<te_fun1>(log10)), TE_FUNCTION1},
    {"log10", reinterpret_cast<const void *>(static_cast<te_fun1>(log10)), TE_FUNCTION1},
    {"ncr", reinterpret_cast<const void *>(static_cast<te_fun2>(ncr)), TE_FUNCTION2},
    {"npr", reinterpret_cast<const void *>(static_cast<te_fun2>(npr)), TE_FUNCTION2},
    {"pi", reinterpret_cast<const void *>(static_cast<te_fun0>(pi)), TE_FUNCTION0},
    {"pow", reinterpret_cast<const void *>(static_cast<te_fun2>(pow)), TE_FUNCTION2},
    {"round", reinterpret_cast<const void *>(static_cast<te_fun1>(round)), TE_FUNCTION1},
    {"sin", reinterpret_cast<const void *>(static_cast<te_fun1>(sin)), TE_FUNCTION1},
    {"sinh", reinterpret_cast<const void *>(static_cast<te_fun1>(sinh)), TE_FUNCTION1},
    {"sqrt", reinterpret_cast<const void *>(static_cast<te_fun1>(sqrt)), TE_FUNCTION1},
    {"tan", reinterpret_cast<const void *>(static_cast<te_fun1>(tan)), TE_FUNCTION1},
    {"tanh", reinterpret_cast<const void *>(static_cast<te_fun1>(tanh)), TE_FUNCTION1},
    {"tau", reinterpret_cast<const void *>(static_cast<te_fun0>(tau)), TE_FUNCTION0},
};

static const te_builtin *find_builtin(const char *name, int len) {
    const auto end = std::end(functions);
    const te_builtin *found = std::lower_bound(std::begin(functions), end, name,
                                               [len](const te_builtin &lhs, const char *rhs) {
                                                   // The length is important because that's where
                                                   // the parens start
                                                   return std::strncmp(lhs.name, rhs, len) < 0;
                                               });
    // We need to compare again because we might have gotten the first "larger" element.
    if (found != end && std::strncmp(found->name, name, len) == 0) return found;
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

static void next_token(state *s) {
    s->type = TOK_NULL;

    do {
        if (!*s->next) {
            s->type = TOK_END;
            return;
        }

        /* Try reading a number. */
        if ((s->next[0] >= '0' && s->next[0] <= '9') || s->next[0] == '.') {
            s->value = strtod(s->next, const_cast<char **>(&s->next));
            s->type = TOK_NUMBER;
        } else {
            /* Look for a function call. */
            // But not when it's an "x" followed by whitespace
            // - that's the alternative multiplication operator.
            if (s->next[0] >= 'a' && s->next[0] <= 'z' &&
                !(s->next[0] == 'x' && isspace(s->next[1]))) {
                const char *start;
                start = s->next;
                while ((s->next[0] >= 'a' && s->next[0] <= 'z') ||
                       (s->next[0] >= '0' && s->next[0] <= '9') || (s->next[0] == '_'))
                    s->next++;

                const te_builtin *var = find_builtin(start, s->next - start);

                if (var) {
                    switch (var->type) {
                        case TE_FUNCTION0:
                        case TE_FUNCTION1:
                        case TE_FUNCTION2:
                        case TE_FUNCTION3:
                            s->type = var->type;
                            s->function = var->address;
                            break;
                    }
                } else if (s->type != TOK_ERROR || s->error == TE_ERROR_UNKNOWN) {
                    // Our error is more specific, so it takes precedence.
                    s->type = TOK_ERROR;
                    s->error = TE_ERROR_UNKNOWN_FUNCTION;
                }
            } else {
                /* Look for an operator or special character. */
                switch (s->next++[0]) {
                    // The "te_fun2" casts are necessary to pick the right overload.
                    case '+':
                        s->type = TOK_INFIX;
                        s->function = reinterpret_cast<const void *>(static_cast<te_fun2>(add));
                        break;
                    case '-':
                        s->type = TOK_INFIX;
                        s->function = reinterpret_cast<const void *>(static_cast<te_fun2>(sub));
                        break;
                    case 'x':
                    case '*':
                        // We've already checked for whitespace above.
                        s->type = TOK_INFIX;
                        s->function = reinterpret_cast<const void *>(static_cast<te_fun2>(mul));
                        break;
                    case '/':
                        s->type = TOK_INFIX;
                        s->function = reinterpret_cast<const void *>(static_cast<te_fun2>(divide));
                        break;
                    case '^':
                        s->type = TOK_INFIX;
                        s->function = reinterpret_cast<const void *>(static_cast<te_fun2>(pow));
                        break;
                    case '%':
                        s->type = TOK_INFIX;
                        s->function = reinterpret_cast<const void *>(static_cast<te_fun2>(fmod));
                        break;
                    case '(':
                        s->type = TOK_OPEN;
                        break;
                    case ')':
                        s->type = TOK_CLOSE;
                        break;
                    case ',':
                        s->type = TOK_SEP;
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
                        s->type = TOK_ERROR;
                        s->error = TE_ERROR_LOGICAL_OPERATOR;
                        break;
                    default:
                        s->type = TOK_ERROR;
                        s->error = TE_ERROR_MISSING_OPERATOR;
                        break;
                }
            }
        }
    } while (s->type == TOK_NULL);
}

static te_expr *expr(state *s);
static te_expr *power(state *s);

static te_expr *base(state *s) {
    /* <base>      =    <constant> | <function-0> {"(" ")"} | <function-1> <power> |
     * <function-X> "(" <expr> {"," <expr>} ")" | "(" <list> ")" */
    te_expr *ret;
    int arity;

    switch (s->type) {
        case TOK_NUMBER:
            ret = new_expr(TE_CONSTANT, nullptr);
            ret->value = s->value;
            next_token(s);
            break;

        case TE_FUNCTION0:
            ret = new_expr(s->type, nullptr);
            ret->function = s->function;
            next_token(s);
            if (s->type == TOK_OPEN) {
                next_token(s);
                if (s->type == TOK_CLOSE) {
                    next_token(s);
                } else if (s->type != TOK_ERROR || s->error == TE_ERROR_UNKNOWN) {
                    s->type = TOK_ERROR;
                    s->error = TE_ERROR_MISSING_CLOSING_PAREN;
                }
            }
            break;

        case TE_FUNCTION1:
        case TE_FUNCTION2:
        case TE_FUNCTION3:
            arity = get_arity(s->type);

            ret = new_expr(s->type, nullptr);
            ret->function = s->function;
            next_token(s);

            if (s->type == TOK_OPEN) {
                int i;
                for (i = 0; i < arity; i++) {
                    next_token(s);
                    ret->parameters[i] = expr(s);
                    if (s->type != TOK_SEP) {
                        break;
                    }
                }
                if (s->type == TOK_CLOSE && i == arity - 1) {
                    next_token(s);
                } else if (s->type != TOK_ERROR || s->error == TE_ERROR_UNEXPECTED_TOKEN) {
                    // If we had the right number of arguments, we're missing a closing paren.
                    if (i == arity - 1 && s->type != TOK_ERROR) {
                        s->error = TE_ERROR_MISSING_CLOSING_PAREN;
                    } else {
                        // Otherwise we complain about the number of arguments *first*,
                        // a closing parenthesis should be more obvious.
                        s->error = i < arity ? TE_ERROR_TOO_FEW_ARGS : TE_ERROR_TOO_MANY_ARGS;
                    }
                    s->type = TOK_ERROR;
                }
            } else if (s->type != TOK_ERROR || s->error == TE_ERROR_UNKNOWN) {
                s->type = TOK_ERROR;
                s->error = TE_ERROR_MISSING_OPENING_PAREN;
            }

            break;

        case TOK_OPEN:
            next_token(s);
            ret = expr(s);
            if (s->type == TOK_CLOSE) {
                next_token(s);
            } else if (s->type != TOK_ERROR || s->error == TE_ERROR_UNKNOWN) {
                s->type = TOK_ERROR;
                s->error = TE_ERROR_MISSING_CLOSING_PAREN;
            }
            break;

        case TOK_END:
            // The expression ended before we expected it.
            // e.g. `2 - `.
            // This means we have too few things.
            // Instead of introducing another error, just call it
            // "too few args".
            ret = new_expr(0, nullptr);
            s->type = TOK_ERROR;
            s->error = TE_ERROR_TOO_FEW_ARGS;
            ret->value = NAN;
            break;
        default:
            ret = new_expr(0, nullptr);
            s->type = TOK_ERROR;
            s->error = TE_ERROR_UNEXPECTED_TOKEN;
            ret->value = NAN;
            break;
    }

    return ret;
}

static te_expr *power(state *s) {
    /* <power>     =    {("-" | "+")} <base> */
    int sign = 1;
    while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
        if (s->function == sub) sign = -sign;
        next_token(s);
    }

    te_expr *ret;

    if (sign == 1) {
        ret = base(s);
    } else {
        ret = NEW_EXPR(TE_FUNCTION1, base(s));
        ret->function = reinterpret_cast<const void *>(negate);
    }

    return ret;
}

static te_expr *factor(state *s) {
    /* <factor>    =    <power> {"^" <power>} */
    te_expr *ret = power(s);

    te_expr *insertion = nullptr;

    while (s->type == TOK_INFIX &&
           (s->function == reinterpret_cast<const void *>(static_cast<te_fun2>(pow)))) {
        auto t = reinterpret_cast<te_fun2>(const_cast<void *>(s->function));
        next_token(s);

        if (insertion) {
            /* Make exponentiation go right-to-left. */
            te_expr *insert = NEW_EXPR(TE_FUNCTION2, insertion->parameters[1], power(s));
            insert->function = reinterpret_cast<const void *>(t);
            insertion->parameters[1] = insert;
            insertion = insert;
        } else {
            ret = NEW_EXPR(TE_FUNCTION2, ret, power(s));
            ret->function = reinterpret_cast<const void *>(t);
            insertion = ret;
        }
    }

    return ret;
}

static te_expr *term(state *s) {
    /* <term>      =    <factor> {("*" | "/" | "%") <factor>} */
    te_expr *ret = factor(s);

    while (s->type == TOK_INFIX &&
           (s->function == reinterpret_cast<const void *>(static_cast<te_fun2>(mul)) ||
            s->function == reinterpret_cast<const void *>(static_cast<te_fun2>(divide)) ||
            s->function == reinterpret_cast<const void *>(static_cast<te_fun2>(fmod)))) {
        auto t = reinterpret_cast<te_fun2>(const_cast<void *>(s->function));
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2, ret, factor(s));
        ret->function = reinterpret_cast<const void *>(t);
    }

    return ret;
}

static te_expr *expr(state *s) {
    /* <expr>      =    <term> {("+" | "-") <term>} */
    te_expr *ret = term(s);

    while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
        auto t = reinterpret_cast<te_fun2>(const_cast<void *>(s->function));
        next_token(s);
        ret = NEW_EXPR(TE_FUNCTION2, ret, term(s));
        ret->function = reinterpret_cast<const void *>(t);
    }

    return ret;
}

#define TE_FUN(...) ((double (*)(__VA_ARGS__))n->function)
#define M(e) te_eval(n->parameters[e])

double te_eval(const te_expr *n) {
    if (!n) return NAN;

    switch (n->type) {
        case TE_CONSTANT:
            return n->value;
        case TE_FUNCTION0:
            return TE_FUN(void)();
        case TE_FUNCTION1:
            return TE_FUN(double)(M(0));
        case TE_FUNCTION2:
            return TE_FUN(double, double)(M(0), M(1));
        case TE_FUNCTION3:
            return TE_FUN(double, double, double)(M(0), M(1), M(2));
        default:
            return NAN;
    }
}

#undef TE_FUN
#undef M

static void optimize(te_expr *n) {
    /* Evaluates as much as possible. */
    if (n->type == TE_CONSTANT) return;

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

te_expr *te_compile(const char *expression, te_error_t *error) {
    state s;
    s.start = s.next = expression;
    s.error = TE_ERROR_NONE;

    next_token(&s);
    te_expr *root = expr(&s);

    if (s.type != TOK_END) {
        te_free(root);
        if (error) {
            error->position = (s.next - s.start) + 1;
            if (s.error != TE_ERROR_NONE) {
                error->type = s.error;
            } else {
                // If we're not at the end but there's no error, then that means we have a
                // superfluous token that we have no idea what to do with. This occurs in e.g. `2 +
                // 2 4` - the "4" is just not part of the expression. We can report either "too many
                // arguments" or "expected operator", but the operator should be reported between
                // the "2" and the "4". So we report TOO_MANY_ARGS on the "4".
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

double te_interp(const char *expression, te_error_t *error) {
    te_expr *n = te_compile(expression, error);
    double ret;
    if (n) {
        ret = te_eval(n);
        te_free(n);
    } else {
        ret = NAN;
    }
    return ret;
}
