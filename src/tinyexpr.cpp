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
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

// TODO: It would be nice not to rely on a typedef for this, especially one that can only do functions with two args.
typedef double (*te_fun2)(double, double);
typedef double (*te_fun1)(double);
typedef double (*te_fun0)();

enum {
      TE_CONSTANT = 0,
      TE_FUNCTION0, TE_FUNCTION1, TE_FUNCTION2, TE_FUNCTION3,
      TOK_NULL, TOK_ERROR, TOK_END, TOK_SEP,
      TOK_OPEN, TOK_CLOSE, TOK_NUMBER, TOK_INFIX
};

class te_expr {
public:
    int type;
    double value;
    const void *function;
    int arity = 0;
    std::vector<te_expr> parameters;
    te_expr(int t, const void *fun, te_expr *params) : type(t), value(0), function(fun)
    {
        // We fill in the arity automatically, because we need it here anyway.
        switch (t) {
            case TE_FUNCTION0: arity = 0; break;
            case TE_FUNCTION1: arity = 1; break;
            case TE_FUNCTION2: arity = 2; break;
            case TE_FUNCTION3: arity = 3; break;
            default: arity = 0;
        }

        // We allow filling the parameters later.
        if (params) {
            for (int i = 0; i < arity; i++) parameters.push_back(params[i]);
        }
    }

    te_expr(int t, double val) : type(t), value(val), function(NULL) {}
};

// TODO: Rename since variables have been removed.
typedef struct te_builtin {
    const char *name;
    const void *address;
    int type;
} te_builtin;

typedef struct state {
    const char *start;
    const char *next;
    int type;
    union {double value; const void *function;};

    te_error_type_t error;
} state;

/* Parses the input expression and binds variables. */
/* Returns NULL on error. */
te_expr te_compile(const char *expression, te_error_t *error);

/* Evaluates the expression. */
double te_eval(const te_expr &n);

static double pi() {return 3.14159265358979323846;}
static double e() {return 2.71828182845904523536;}
static double fac(double a) {/* simplest version of fac */
    if (a < 0.0)
        return NAN;
    if (a > UINT_MAX)
        return INFINITY;
    unsigned int ua = (unsigned int)(a);
    unsigned long int result = 1, i;
    for (i = 1; i <= ua; i++) {
        if (i > ULONG_MAX / result)
            return INFINITY;
        result *= i;
    }
    return (double)result;
}

static double ncr(double n, double r) {
    if (n < 0.0 || r < 0.0 || n < r) return NAN;
    if (n > UINT_MAX || r > UINT_MAX) return INFINITY;
    unsigned long int un = (unsigned int)(n), ur = (unsigned int)(r), i;
    unsigned long int result = 1;
    if (ur > un / 2) ur = un - ur;
    for (i = 1; i <= ur; i++) {
        if (result > ULONG_MAX / (un - ur + i))
            return INFINITY;
        result *= un - ur + i;
        result /= i;
    }
    return result;
}

static double npr(double n, double r) {return ncr(n, r) * fac(r);}

static const te_builtin functions[] = {
    /* must be in alphabetical order */
    {"abs", (const void *)(te_fun1)fabs,     TE_FUNCTION1},
    {"acos", (const void *)(te_fun1)acos,    TE_FUNCTION1},
    {"asin", (const void *)(te_fun1)asin,    TE_FUNCTION1},
    {"atan", (const void *)(te_fun1)atan,    TE_FUNCTION1},
    {"atan2", (const void *)(te_fun2)atan2,  TE_FUNCTION2},
    {"ceil", (const void *)(te_fun1)ceil,    TE_FUNCTION1},
    {"cos", (const void *)(te_fun1)cos,      TE_FUNCTION1},
    {"cosh", (const void *)(te_fun1)cosh,    TE_FUNCTION1},
    {"e", (const void *)(te_fun0)e,          TE_FUNCTION0},
    {"exp", (const void *)(te_fun1)exp,      TE_FUNCTION1},
    {"fac", (const void *)(te_fun1)fac,      TE_FUNCTION1},
    {"floor", (const void *)(te_fun1)floor,  TE_FUNCTION1},
    {"ln", (const void *)(te_fun1)log,       TE_FUNCTION1},
    {"log", (const void *)(te_fun1)log10,    TE_FUNCTION1},
    {"log10", (const void *)(te_fun1)log10,  TE_FUNCTION1},
    {"ncr", (const void *)(te_fun2)ncr,      TE_FUNCTION2},
    {"npr", (const void *)(te_fun2)npr,      TE_FUNCTION2},
    {"pi", (const void *)(te_fun1)pi,        TE_FUNCTION0},
    {"pow", (const void *)(te_fun2)pow,      TE_FUNCTION2},
    {"round", (const void *)(te_fun1)round,  TE_FUNCTION1},
    {"sin", (const void *)(te_fun1)sin,      TE_FUNCTION1},
    {"sinh", (const void *)(te_fun1)sinh,    TE_FUNCTION1},
    {"sqrt", (const void *)(te_fun1)sqrt,    TE_FUNCTION1},
    {"tan", (const void *)(te_fun1)tan,      TE_FUNCTION1},
    {"tanh", (const void *)(te_fun1)tanh,    TE_FUNCTION1}
};

static const te_builtin *find_builtin(const char *name, int len) {
    const auto end = std::end(functions);
    const te_builtin *found = std::lower_bound(std::begin(functions), end, name,
                                                [len](const te_builtin &lhs, const char *rhs) {
                                                    // The length is important because that's where the parens start
                                                    return strncmp(lhs.name, rhs, len) < 0;
                                                });
    // We need to compare again because we might have gotten the first "larger" element.
    if (found != end && strncmp(found->name, name, len) == 0) return found;
    return NULL;
}

static double add(double a, double b) {return a + b;}
static double sub(double a, double b) {return a - b;}
static double mul(double a, double b) {return a * b;}
static double divide(double a, double b) {return a / b;}
static double negate(double a) {return -a;}

void next_token(state *s) {
    s->type = TOK_NULL;

    do {
        if (!*s->next){
            s->type = TOK_END;
            return;
        }

        /* Try reading a number. */
        if ((s->next[0] >= '0' && s->next[0] <= '9') || s->next[0] == '.') {
            s->value = strtod(s->next, (char**)&s->next);
            s->type = TOK_NUMBER;
        } else {
            /* Look for a variable or builtin function call. */
            if (s->next[0] >= 'a' && s->next[0] <= 'z') {
                const char *start;
                start = s->next;
                while ((s->next[0] >= 'a' && s->next[0] <= 'z') || (s->next[0] >= '0' && s->next[0] <= '9') || (s->next[0] == '_')) s->next++;

                const te_builtin *var = find_builtin(start, s->next - start);

                if (var) {
                    switch(var->type) {
                        case TE_FUNCTION0: case TE_FUNCTION1: case TE_FUNCTION2: case TE_FUNCTION3:
                            s->type = var->type;
                            s->function = var->address;
                            break;
                    }
                } else if (s->type != TOK_ERROR
                           || s->error == TE_ERROR_UNKNOWN) {
                    // Our error is more specific, so it takes precedence.
                    s->type = TOK_ERROR;
                    s->error = TE_ERROR_UNKNOWN_VARIABLE;
                }
            } else {
                /* Look for an operator or special character. */
                switch (s->next++[0]) {
                    // The "te_fun2" casts are necessary to pick the right overload.
                    case '+': s->type = TOK_INFIX; s->function = (const void *)(te_fun2) add; break;
                    case '-': s->type = TOK_INFIX; s->function = (const void *)(te_fun2) sub; break;
                    case '*': s->type = TOK_INFIX; s->function = (const void *)(te_fun2) mul; break;
                    case '/': s->type = TOK_INFIX; s->function = (const void *)(te_fun2) divide; break;
                    case '^': s->type = TOK_INFIX; s->function = (const void *)(te_fun2) pow; break;
                    case '%': s->type = TOK_INFIX; s->function = (const void *)(te_fun2) fmod; break;
                    case '(': s->type = TOK_OPEN; break;
                    case ')': s->type = TOK_CLOSE; break;
                    case ',': s->type = TOK_SEP; break;
                    case ' ': case '\t': case '\n': case '\r': break;
                    default: s->type = TOK_ERROR; s->error = TE_ERROR_MISSING_OPERATOR; break;
                }
            }
        }
    } while (s->type == TOK_NULL);
}


static te_expr expr(state *s);
static te_expr power(state *s);

static te_expr base(state *s) {
    /* <base>      =    <constant> | <variable> | <function-0> {"(" ")"} | <function-1> <power> | <function-X> "(" <expr> {"," <expr>} ")" | "(" <list> ")" */
    te_expr ret(0, NULL, NULL);

    switch (s->type) {
        case TOK_NUMBER:
            ret = te_expr(TE_CONSTANT, s->value);
            next_token(s);
            break;

        case TE_FUNCTION0:
            ret = te_expr(s->type, s->function, NULL);
            next_token(s);
            if (s->type == TOK_OPEN) {
                next_token(s);
                if (s->type == TOK_CLOSE) {
                    next_token(s);
                } else if (s->type != TOK_ERROR
                           || s->error == TE_ERROR_UNKNOWN) {
                    s->type = TOK_ERROR;
                    s->error = TE_ERROR_MISSING_CLOSING_PAREN;
                }
            }
            break;

        case TE_FUNCTION1:
        case TE_FUNCTION2: case TE_FUNCTION3:
            ret = te_expr(s->type, s->function, NULL);
            next_token(s);

            if (s->type == TOK_OPEN) {
                int i;
                for(i = 0; i < ret.arity; i++) {
                    next_token(s);
                    ret.parameters.push_back(expr(s));
                    if(s->type != TOK_SEP) {
                        break;
                    }
                }
                if(s->type == TOK_CLOSE && i == ret.arity - 1) {
                    next_token(s);
                } else if (s->type != TOK_ERROR
                           || s->error == TE_ERROR_UNKNOWN) {
                    s->type = TOK_ERROR;
                    s->error = i < ret.arity ? TE_ERROR_TOO_FEW_ARGS
                        : TE_ERROR_TOO_MANY_ARGS;
                }
            } else if (s->type != TOK_ERROR
                       || s->error == TE_ERROR_UNKNOWN) {
                s->type = TOK_ERROR;
                s->error = TE_ERROR_MISSING_OPENING_PAREN;
            }

            break;

        case TOK_OPEN:
            next_token(s);
            ret = expr(s);
            if (s->type == TOK_CLOSE) {
                next_token(s);
            } else if (s->type != TOK_ERROR
                       || s->error == TE_ERROR_UNKNOWN) {
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
            ret = te_expr(TE_CONSTANT, NAN);
            s->type = TOK_ERROR;
            s->error = TE_ERROR_TOO_FEW_ARGS;
            break;
        default:
            ret = te_expr(TE_CONSTANT, NAN);
            s->type = TOK_ERROR;
            s->error = TE_ERROR_UNKNOWN;
            break;
    }

    return ret;
}


static te_expr power(state *s) {
    /* <power>     =    {("-" | "+")} <base> */
    int sign = 1;
    while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
        if (s->function == sub) sign = -sign;
        next_token(s);
    }

    if (sign == 1) {
        te_expr ret = base(s);
        return ret;
    } else {
        te_expr params[1] = { base(s) };
        te_expr ret = te_expr(TE_FUNCTION1, (const void *) negate, params);
        return ret;
    }
}

static te_expr factor(state *s) {
    /* <factor>    =    <power> {"^" <power>} */
    te_expr ret = power(s);

    while (s->type == TOK_INFIX && (s->function == (const void*)(te_fun2)pow)) {
        te_fun2 t = (te_fun2) s->function;
        next_token(s);

        te_expr param[2] = { ret, power(s) };
        ret = te_expr(TE_FUNCTION2, (const void *) t, param);
    }

    return ret;
}


static te_expr term(state *s) {
    /* <term>      =    <factor> {("*" | "/" | "%") <factor>} */
    te_expr ret = factor(s);

    while (s->type == TOK_INFIX && (s->function == (const void*)(te_fun2)mul || s->function == (const void*)(te_fun2)divide || s->function == (const void*)(te_fun2)fmod)) {
        te_fun2 t = (te_fun2) s->function;
        next_token(s);
        te_expr params[2] = { ret, factor(s) };
        ret = te_expr(TE_FUNCTION2, (const void *) t, params);
    }

    return ret;
}


static te_expr expr(state *s) {
    /* <expr>      =    <term> {("+" | "-") <term>} */
    te_expr ret = term(s);

    while (s->type == TOK_INFIX && (s->function == add || s->function == sub)) {
        te_fun2 t = (te_fun2) s->function;
        next_token(s);
        te_expr params[2] = { ret, term(s) };
        ret = te_expr(TE_FUNCTION2, (const void *) t, params);
    }

    return ret;
}


#define TE_FUN(...) ((double(*)(__VA_ARGS__))n.function)
#define M(e) te_eval(n.parameters[e])


double te_eval(const te_expr &n) {
    switch(n.type) {
        case TE_CONSTANT:
            return n.value;
        case TE_FUNCTION0:
            return TE_FUN(void)();
        case TE_FUNCTION1:
            return TE_FUN(double)(M(0));
        case TE_FUNCTION2:
            return TE_FUN(double, double)(M(0), M(1));
        case TE_FUNCTION3:
            return TE_FUN(double, double, double)(M(0), M(1), M(2));
        default: return NAN;
    }
}

#undef TE_FUN
#undef M

static void optimize(te_expr &n) {
    /* Evaluates as much as possible. */
    if (n.type == TE_CONSTANT) return;

    bool known = true;
    for (int i = 0; i < n.arity; ++i) {
        optimize(n.parameters[i]);
        if ((n.parameters[i]).type != TE_CONSTANT) {
            known = false;
        }
    }
    if (known) {
        const double value = te_eval(n);
        n.type = TE_CONSTANT;
        n.value = value;
    }
}


te_expr te_compile(const char *expression, te_error_t *error) {
    state s;
    s.start = s.next = expression;
    s.error = TE_ERROR_NONE;

    next_token(&s);
    te_expr root = expr(&s);

    if (s.type != TOK_END) {
        if (error) {
            error->position = (s.next - s.start) + 1;
            if (s.error != TE_ERROR_NONE) {
                error->type = s.error;
            } else {
                // If we're not at the end but there's no error, then that means we have a superfluous
                // token that we have no idea what to do with.
                // This occurs in e.g. `2 + 2 4` - the "4" is just not part of the expression.
                // We can report either "too many arguments" or "expected operator", but the operator
                // should be reported between the "2" and the "4".
                // So we report TOO_MANY_ARGS on the "4".
                error->type = TE_ERROR_TOO_MANY_ARGS;
            }
        }
    } else {
        optimize(root);
        if (error) error->position = 0;
    }
    return root;
}

double te_interp(const char *expression, te_error_t *error) {
    te_expr n = te_compile(expression, error);
    double ret;
    ret = te_eval(n);
    return ret;
}
