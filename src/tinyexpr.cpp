// SPDX-FileCopyrightText: Copyright (c) 2015, 2016 Lewis Van Winkle
// SPDX-FileCopyrightText: © 2018 fish-shell contributors
//
// SPDX-License-Identifier: Zlib

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
#include "config.h"

#include "tinyexpr.h"

#include <ctype.h>
#include <limits.h>

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <iterator>
#include <limits>
#include <vector>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "wutil.h"

struct te_fun_t {
    using fn_va = double (*)(const std::vector<double> &);
    using fn_2 = double (*)(double, double);
    using fn_1 = double (*)(double);
    using fn_0 = double (*)();

    constexpr te_fun_t(double val) : type_{CONSTANT}, arity_{0}, value{val} {}
    constexpr te_fun_t(fn_0 fn) : type_{FN_FIXED}, arity_{0}, fun0{fn} {}
    constexpr te_fun_t(fn_1 fn) : type_{FN_FIXED}, arity_{1}, fun1{fn} {}
    constexpr te_fun_t(fn_2 fn) : type_{FN_FIXED}, arity_{2}, fun2{fn} {}
    constexpr te_fun_t(fn_va fn) : type_{FN_VARIADIC}, arity_{-1}, fun_va{fn} {}

    bool operator==(fn_2 fn) const { return arity_ == 2 && fun2 == fn; }

    __warn_unused int arity() const { return arity_; }

    double operator()() const {
        assert(arity_ == 0);
        return type_ == CONSTANT ? value : fun0();
    }

    double operator()(double a, double b) const {
        assert(arity_ == 2);
        return fun2(a, b);
    }

    double operator()(const std::vector<double> &args) const {
        if (type_ == FN_VARIADIC) return fun_va(args);
        if (arity_ != static_cast<int>(args.size())) return NAN;
        switch (arity_) {
            case 0:
                return type_ == CONSTANT ? value : fun0();
            case 1:
                return fun1(args[0]);
            case 2:
                return fun2(args[0], args[1]);
        }
        return NAN;
    }

   private:
    enum {
        CONSTANT,
        FN_FIXED,
        FN_VARIADIC,
    } type_;
    int arity_;

    union {
        double value;
        fn_0 fun0;
        fn_1 fun1;
        fn_2 fun2;
        fn_va fun_va;
    };
};

enum te_state_type_t {
    TOK_NULL,
    TOK_ERROR,
    TOK_END,
    TOK_SEP,
    TOK_OPEN,
    TOK_CLOSE,
    TOK_NUMBER,
    TOK_FUNCTION,
    TOK_INFIX
};

struct state {
    explicit state(const wchar_t *expr) : start_{expr}, next_{expr} { next_token(); }
    double eval() { return expr(); }

    __warn_unused te_error_t error() const {
        if (type_ == TOK_END) return {TE_ERROR_NONE, 0, 0};
        // If we have an error position set, use that,
        // otherwise the current position.
        const wchar_t *tok = errpos_ ? errpos_ : next_;
        te_error_t err{error_, static_cast<int>(tok - start_) + 1, errlen_};
        if (error_ == TE_ERROR_NONE) {
            // If we're not at the end but there's no error, then that means we have a
            // superfluous token that we have no idea what to do with.
            err.type = TE_ERROR_TOO_MANY_ARGS;
        }
        return err;
    }

   private:
    te_state_type_t type_{TOK_NULL};
    te_error_type_t error_{TE_ERROR_NONE};

    const wchar_t *start_;
    const wchar_t *next_;
    const wchar_t *errpos_{nullptr};
    int errlen_{0};

    te_fun_t current_{NAN};
    void next_token();

    double expr();
    double power();
    double base();
    double factor();
    double term();
};

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

static double maximum(const std::vector<double> &args) {
    double ret = -std::numeric_limits<double>::infinity();
    for (auto a : args) ret = max(ret, a);
    return ret;
}

static double minimum(const std::vector<double> &args) {
    double ret = std::numeric_limits<double>::infinity();
    for (auto a : args) ret = min(ret, a);
    return ret;
}

struct te_builtin {
    const wchar_t *name;
    te_fun_t fn;
};

static constexpr te_builtin functions[] = {
    /* must be in alphabetical order */
    // clang-format off
    {L"abs", std::fabs},
    {L"acos", std::acos},
    {L"asin", std::asin},
    {L"atan", std::atan},
    {L"atan2", std::atan2},
    {L"bitand", bit_and},
    {L"bitor", bit_or},
    {L"bitxor", bit_xor},
    {L"ceil", std::ceil},
    {L"cos", std::cos},
    {L"cosh", std::cosh},
    {L"e", M_E},
    {L"exp", std::exp},
    {L"fac", fac},
    {L"floor", std::floor},
    {L"ln", std::log},
    {L"log", std::log10},
    {L"log10", std::log10},
    {L"log2", std::log2},
    {L"max", maximum},
    {L"min", minimum},
    {L"ncr", ncr},
    {L"npr", npr},
    {L"pi", M_PI},
    {L"pow", std::pow},
    {L"round", std::round},
    {L"sin", std::sin},
    {L"sinh", std::sinh},
    {L"sqrt", std::sqrt},
    {L"tan", std::tan},
    {L"tanh", std::tanh},
    {L"tau", 2 * M_PI},
    // clang-format on
};
ASSERT_SORTED_BY_NAME(functions);

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

void state::next_token() {
    type_ = TOK_NULL;

    do {
        if (!*next_) {
            type_ = TOK_END;
            return;
        }

        /* Try reading a number. */
        if ((next_[0] >= '0' && next_[0] <= '9') || next_[0] == '.') {
            current_ = fish_wcstod_underscores(next_, const_cast<wchar_t **>(&next_));
            type_ = TOK_NUMBER;
        } else {
            /* Look for a function call. */
            // But not when it's an "x" followed by whitespace
            // - that's the alternative multiplication operator.
            if (next_[0] >= 'a' && next_[0] <= 'z' && !(next_[0] == 'x' && isspace(next_[1]))) {
                const wchar_t *start = next_;
                while ((next_[0] >= 'a' && next_[0] <= 'z') ||
                       (next_[0] >= '0' && next_[0] <= '9') || (next_[0] == '_'))
                    next_++;

                const te_builtin *var = find_builtin(start, next_ - start);

                if (var) {
                    type_ = TOK_FUNCTION;
                    current_ = var->fn;
                } else if (type_ != TOK_ERROR || error_ == TE_ERROR_UNKNOWN) {
                    // Our error is more specific, so it takes precedence.
                    type_ = TOK_ERROR;
                    error_ = TE_ERROR_UNKNOWN_FUNCTION;
                    errpos_ = start + 1;
                    errlen_ = next_ - start;
                }
            } else {
                /* Look for an operator or special character. */
                switch (next_++[0]) {
                    case '+':
                        type_ = TOK_INFIX;
                        current_ = add;
                        break;
                    case '-':
                        type_ = TOK_INFIX;
                        current_ = sub;
                        break;
                    case 'x':
                    case '*':
                        // We've already checked for whitespace above.
                        type_ = TOK_INFIX;
                        current_ = mul;
                        break;
                    case '/':
                        type_ = TOK_INFIX;
                        current_ = divide;
                        break;
                    case '^':
                        type_ = TOK_INFIX;
                        current_ = pow;
                        break;
                    case '%':
                        type_ = TOK_INFIX;
                        current_ = fmod;
                        break;
                    case '(':
                        type_ = TOK_OPEN;
                        break;
                    case ')':
                        type_ = TOK_CLOSE;
                        break;
                    case ',':
                        type_ = TOK_SEP;
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
                        type_ = TOK_ERROR;
                        error_ = TE_ERROR_LOGICAL_OPERATOR;
                        break;
                    default:
                        type_ = TOK_ERROR;
                        error_ = TE_ERROR_MISSING_OPERATOR;
                        break;
                }
            }
        }
    } while (type_ == TOK_NULL);
}

double state::base() {
    /* <base>      =    <constant> | <function-0> {"(" ")"} | <function-1> <power> |
     * <function-X> "(" <expr> {"," <expr>} ")" | "(" <list> ")" */

    auto next = next_;
    switch (type_) {
        case TOK_NUMBER: {
            auto val = current_();
            next_token();
            if (type_ == TOK_NUMBER || type_ == TOK_FUNCTION) {
                // Two numbers after each other:
                // math '5 2'
                // math '3 pi'
                // (of course 3 pi could also be interpreted as 3 x pi)
                type_ = TOK_ERROR;
                error_ = TE_ERROR_MISSING_OPERATOR;
                // The error should be given *between*
                // the last two tokens.
                errpos_ = next + 1;
                // Go to the end of whitespace and then one more.
                while (wcschr(L" \t\n\r", next[0])) {
                    next++;
                }
                next++;
                errlen_ = next - errpos_;
            }
            return val;
        }

        case TOK_FUNCTION: {
            auto fn = current_;
            int arity = fn.arity();
            next_token();

            const bool have_open = type_ == TOK_OPEN;
            if (have_open) {
                // If we *have* an opening parenthesis,
                // we need to consume it and
                // expect a closing one.
                next_token();
            }

            if (arity == 0) {
                if (have_open) {
                    if (type_ == TOK_CLOSE) {
                        next_token();
                    } else if (type_ != TOK_ERROR || error_ == TE_ERROR_UNKNOWN) {
                        type_ = TOK_ERROR;
                        error_ = TE_ERROR_MISSING_CLOSING_PAREN;
                        break;
                    }
                }
                return fn();
            }

            std::vector<double> parameters;
            int i;
            const wchar_t *first_err = nullptr;
            for (i = 0;; i++) {
                if (i == arity) first_err = next_;
                parameters.push_back(expr());
                if (type_ != TOK_SEP) {
                    break;
                }
                next_token();
            }

            if (arity < 0 || i == arity - 1) {
                if (!have_open) {
                    return fn(parameters);
                }
                if (type_ == TOK_CLOSE) {
                    // We have an opening and a closing paren, consume the closing one and done.
                    next_token();
                    return fn(parameters);
                }
                if (type_ != TOK_ERROR) {
                    // If we had the right number of arguments, we're missing a closing paren.
                    error_ = TE_ERROR_MISSING_CLOSING_PAREN;
                    type_ = TOK_ERROR;
                }
            }
            if (type_ != TOK_ERROR || error_ == TE_ERROR_UNEXPECTED_TOKEN) {
                // Otherwise we complain about the number of arguments *first*,
                // a closing parenthesis should be more obvious.
                //
                // Vararg functions need at least one argument.
                error_ = (i < arity || (arity == -1 && i == 0)) ? TE_ERROR_TOO_FEW_ARGS
                                                                : TE_ERROR_TOO_MANY_ARGS;
                type_ = TOK_ERROR;
                if (first_err) {
                    errpos_ = first_err;
                    errlen_ = next_ - first_err;
                    // TODO: Rationalize where we put the cursor exactly.
                    // If we have a closing paren it's on it, if we don't it's before the number.
                    if (type_ != TOK_CLOSE) errlen_++;
                }
            }
            break;
        }

        case TOK_OPEN: {
            next_token();
            auto ret = expr();
            if (type_ == TOK_CLOSE) {
                next_token();
                return ret;
            }
            if (type_ != TOK_ERROR && type_ != TOK_END && error_ == TE_ERROR_NONE) {
                type_ = TOK_ERROR;
                error_ = TE_ERROR_TOO_MANY_ARGS;
            } else if (type_ != TOK_ERROR || error_ == TE_ERROR_UNKNOWN) {
                type_ = TOK_ERROR;
                error_ = TE_ERROR_MISSING_CLOSING_PAREN;
            }
            break;
        }

        case TOK_END:
            // The expression ended before we expected it.
            // e.g. `2 - `.
            // This means we have too few things.
            // Instead of introducing another error, just call it
            // "too few args".
            type_ = TOK_ERROR;
            error_ = TE_ERROR_TOO_FEW_ARGS;
            break;
        default:
            if (type_ != TOK_ERROR || error_ == TE_ERROR_UNKNOWN) {
                type_ = TOK_ERROR;
                error_ = TE_ERROR_UNEXPECTED_TOKEN;
            }
            break;
    }

    return NAN;
}

double state::power() {
    /* <power>     =    {("-" | "+")} <base> */
    int sign = 1;
    while (type_ == TOK_INFIX && (current_ == add || current_ == sub)) {
        if (current_ == sub) sign = -sign;
        next_token();
    }
    return sign * base();
}

double state::factor() {
    /* <factor>    =    <power> {"^" <power>} */
    auto ret = power();
    if (type_ == TOK_INFIX && current_ == pow) {
        next_token();
        ret = pow(ret, factor());
    }
    return ret;
}

double state::term() {
    /* <term>      =    <factor> {("*" | "/" | "%") <factor>} */
    auto ret = factor();
    while (type_ == TOK_INFIX && (current_ == mul || current_ == divide || current_ == fmod)) {
        auto fn = current_;
        auto tok = next_;
        next_token();
        auto ret2 = factor();
        if (ret2 == 0 && (fn == divide || fn == fmod)) {
            // Division by zero (also for modulo)
            type_ = TOK_ERROR;
            error_ = TE_ERROR_DIV_BY_ZERO;
            // Error position is the "/" or "%" sign for now
            errpos_ = tok;
            errlen_ = 1;
        }
        ret = fn(ret, ret2);
    }
    return ret;
}

double state::expr() {
    /* <expr>      =    <term> {("+" | "-") <term>} */
    auto ret = term();
    while (type_ == TOK_INFIX && (current_ == add || current_ == sub)) {
        auto fn = current_;
        next_token();
        ret = fn(ret, term());
    }
    return ret;
}

double te_interp(const wchar_t *expression, te_error_t *error) {
    state s{expression};
    double ret = s.eval();
    if (error) *error = s.error();
    return ret;
}
