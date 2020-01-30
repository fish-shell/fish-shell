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

// This version was altered and ported to C++ for inclusion in fish.

#ifndef __TINYEXPR_H__
#define __TINYEXPR_H__

typedef enum {
    TE_ERROR_NONE = 0,
    TE_ERROR_UNKNOWN_FUNCTION = 1,
    TE_ERROR_MISSING_CLOSING_PAREN = 2,
    TE_ERROR_MISSING_OPENING_PAREN = 3,
    TE_ERROR_TOO_FEW_ARGS = 4,
    TE_ERROR_TOO_MANY_ARGS = 5,
    TE_ERROR_MISSING_OPERATOR = 6,
    TE_ERROR_UNEXPECTED_TOKEN = 7,
    TE_ERROR_LOGICAL_OPERATOR = 8,
    TE_ERROR_UNKNOWN = 9
} te_error_type_t;

typedef struct te_error_t {
    te_error_type_t type;
    int position;
} te_error_t;

/* Parses the input expression, evaluates it, and frees it. */
/* Returns NaN on error. */
double te_interp(const char *expression, te_error_t *error);

#endif /*__TINYEXPR_H__*/
