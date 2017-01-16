/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/

/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
     Original API code Copyright (c) 1997-2012 University of Cambridge
         New API code Copyright (c) 2016 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define NLBLOCK cb             /* Block containing newline information */
#define PSSTART start_pattern  /* Field containing processed string start */
#define PSEND   end_pattern    /* Field containing processed string end */

#include "pcre2_internal.h"

/* In rare error cases debugging might require calling pcre2_printint(). */

#if 0
#ifdef EBCDIC
#define PRINTABLE(c) ((c) >= 64 && (c) < 255)
#else
#define PRINTABLE(c) ((c) >= 32 && (c) < 127)
#endif
#include "pcre2_printint.c"
#define CALL_PRINTINT
#endif

/* There are a few things that vary with different code unit sizes. Handle them
by defining macros in order to minimize #if usage. */

#if PCRE2_CODE_UNIT_WIDTH == 8
#define STRING_UTFn_RIGHTPAR     STRING_UTF8_RIGHTPAR, 5
#define XDIGIT(c)                xdigitab[c]

#else  /* Either 16-bit or 32-bit */
#define XDIGIT(c)                (MAX_255(c)? xdigitab[c] : 0xff)

#if PCRE2_CODE_UNIT_WIDTH == 16
#define STRING_UTFn_RIGHTPAR     STRING_UTF16_RIGHTPAR, 6

#else  /* 32-bit */
#define STRING_UTFn_RIGHTPAR     STRING_UTF32_RIGHTPAR, 6
#endif
#endif

/* Function definitions to allow mutual recursion */

static unsigned int
  add_list_to_class(uint8_t *, PCRE2_UCHAR **, uint32_t, compile_block *,
    const uint32_t *, unsigned int);

static BOOL
  compile_regex(uint32_t, PCRE2_UCHAR **, PCRE2_SPTR *, int *, BOOL, BOOL,
    uint32_t, int, uint32_t *, int32_t *, uint32_t *, int32_t *,
    branch_chain *, compile_block *, size_t *);



/*************************************************
*      Code parameters and static tables         *
*************************************************/

/* This value specifies the size of stack workspace, which is used in different
ways in the different pattern scans. The group-identifying pre-scan uses it to
handle nesting, and needs it to be 16-bit aligned.

During the first compiling phase, when determining how much memory is required,
the regex is partly compiled into this space, but the compiled parts are
discarded as soon as they can be, so that hopefully there will never be an
overrun. The code does, however, check for an overrun, which can occur for
pathological patterns. The size of the workspace depends on LINK_SIZE because
the length of compiled items varies with this.

In the real compile phase, the workspace is used for remembering data about
numbered groups, provided there are not too many of them (if there are, extra
memory is acquired). For this phase the memory must be 32-bit aligned. Having
defined the size in code units, we set up C32_WORK_SIZE as the number of
elements in the 32-bit vector. */

#define COMPILE_WORK_SIZE (2048*LINK_SIZE)   /* Size in code units */

#define C32_WORK_SIZE \
  ((COMPILE_WORK_SIZE * sizeof(PCRE2_UCHAR))/sizeof(uint32_t))

/* The overrun tests check for a slightly smaller size so that they detect the
overrun before it actually does run off the end of the data block. */

#define WORK_SIZE_SAFETY_MARGIN (100)

/* This value determines the size of the initial vector that is used for
remembering named groups during the pre-compile. It is allocated on the stack,
but if it is too small, it is expanded, in a similar way to the workspace. The
value is the number of slots in the list. */

#define NAMED_GROUP_LIST_SIZE  20

/* The original PCRE required patterns to be zero-terminated, and it simplifies
the compiling code if it is guaranteed that there is a zero code unit at the
end of the pattern, because this means that tests for coding sequences such as
(*SKIP) or even just (?<= can check a sequence of code units without having to
keep checking for the end of the pattern. The new PCRE2 API allows zero code
units within patterns if a positive length is given, but in order to keep most
of the compiling code as it was, we copy such patterns and add a zero on the
end. This value determines the size of space on the stack that is used if the
pattern fits; if not, heap memory is used. */

#define COPIED_PATTERN_SIZE 1024

/* Maximum length value to check against when making sure that the variable
that holds the compiled pattern length does not overflow. We make it a bit less
than INT_MAX to allow for adding in group terminating bytes, so that we don't
have to check them every time. */

#define OFLOW_MAX (INT_MAX - 20)

/* Macro for setting individual bits in class bitmaps. It took some
experimenting to figure out how to stop gcc 5.3.0 from warning with
-Wconversion. This version gets a warning:

  #define SETBIT(a,b) a[(b)/8] |= (uint8_t)(1 << ((b)&7))

Let's hope the apparently less efficient version isn't actually so bad if the
compiler is clever with identical subexpressions. */

#define SETBIT(a,b) a[(b)/8] = (uint8_t)(a[(b)/8] | (1 << ((b)&7)))

/* Private flags added to firstcu and reqcu. */

#define REQ_CASELESS    (1 << 0)        /* Indicates caselessness */
#define REQ_VARY        (1 << 1)        /* reqcu followed non-literal item */
/* Negative values for the firstcu and reqcu flags */
#define REQ_UNSET       (-2)            /* Not yet found anything */
#define REQ_NONE        (-1)            /* Found not fixed char */

/* These flags are used in the groupinfo vector. */

#define GI_SET_COULD_BE_EMPTY  0x80000000u
#define GI_COULD_BE_EMPTY      0x40000000u
#define GI_NOT_FIXED_LENGTH    0x20000000u
#define GI_SET_FIXED_LENGTH    0x10000000u
#define GI_FIXED_LENGTH_MASK   0x0000ffffu

/* This bit (which is greater than any UTF value) is used to indicate that a
variable contains a number of code units instead of an actual code point. */

#define UTF_LENGTH     0x10000000l

/* This simple test for a decimal digit works for both ASCII/Unicode and EBCDIC
and is fast (a good compiler can turn it into a subtraction and unsigned
comparison). */

#define IS_DIGIT(x) ((x) >= CHAR_0 && (x) <= CHAR_9)

/* Table to identify hex digits. The tables in chartables are dependent on the
locale, and may mark arbitrary characters as digits. We want to recognize only
0-9, a-z, and A-Z as hex digits, which is why we have a private table here. It
costs 256 bytes, but it is a lot faster than doing character value tests (at
least in some simple cases I timed), and in some applications one wants PCRE to
compile efficiently as well as match efficiently. The value in the table is
the binary hex digit value, or 0xff for non-hex digits. */

/* This is the "normal" case, for ASCII systems, and EBCDIC systems running in
UTF-8 mode. */

#ifndef EBCDIC
static const uint8_t xdigitab[] =
  {
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*   0-  7 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*   8- 15 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  16- 23 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  24- 31 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*    - '  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  ( - /  */
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07, /*  0 - 7  */
  0x08,0x09,0xff,0xff,0xff,0xff,0xff,0xff, /*  8 - ?  */
  0xff,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0xff, /*  @ - G  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  H - O  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  P - W  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  X - _  */
  0xff,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0xff, /*  ` - g  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  h - o  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  p - w  */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  x -127 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 128-135 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 136-143 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 144-151 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 152-159 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 160-167 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 168-175 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 176-183 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 184-191 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 192-199 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 2ff-207 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 208-215 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 216-223 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 224-231 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 232-239 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 240-247 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};/* 248-255 */

#else

/* This is the "abnormal" case, for EBCDIC systems not running in UTF-8 mode. */

static const uint8_t xdigitab[] =
  {
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*   0-  7  0 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*   8- 15    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  16- 23 10 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  24- 31    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  32- 39 20 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  40- 47    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  48- 55 30 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  56- 63    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*    - 71 40 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  72- |     */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  & - 87 50 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  88- 95    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  - -103 60 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 104- ?     */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 112-119 70 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 120- "     */
  0xff,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0xff, /* 128- g  80 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  h -143    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 144- p  90 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  q -159    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 160- x  A0 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  y -175    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  ^ -183 B0 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /* 184-191    */
  0xff,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0xff, /*  { - G  C0 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  H -207    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  } - P  D0 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  Q -223    */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  \ - X  E0 */
  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, /*  Y -239    */
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07, /*  0 - 7  F0 */
  0x08,0x09,0xff,0xff,0xff,0xff,0xff,0xff};/*  8 -255    */
#endif  /* EBCDIC */


/* Table for handling alphanumeric escaped characters. Positive returns are
simple data values; negative values are for special things like \d and so on.
Zero means further processing is needed (for things like \x), or the escape is
invalid. */

/* This is the "normal" table for ASCII systems or for EBCDIC systems running
in UTF-8 mode. It runs from '0' to 'z'. */

#ifndef EBCDIC
#define ESCAPES_FIRST       CHAR_0
#define ESCAPES_LAST        CHAR_z
#define UPPER_CASE(c)       (c-32)

static const short int escapes[] = {
     0,                       0,
     0,                       0,
     0,                       0,
     0,                       0,
     0,                       0,
     CHAR_COLON,              CHAR_SEMICOLON,
     CHAR_LESS_THAN_SIGN,     CHAR_EQUALS_SIGN,
     CHAR_GREATER_THAN_SIGN,  CHAR_QUESTION_MARK,
     CHAR_COMMERCIAL_AT,      -ESC_A,
     -ESC_B,                  -ESC_C,
     -ESC_D,                  -ESC_E,
     0,                       -ESC_G,
     -ESC_H,                  0,
     0,                       -ESC_K,
     0,                       0,
     -ESC_N,                  0,
     -ESC_P,                  -ESC_Q,
     -ESC_R,                  -ESC_S,
     0,                       0,
     -ESC_V,                  -ESC_W,
     -ESC_X,                  0,
     -ESC_Z,                  CHAR_LEFT_SQUARE_BRACKET,
     CHAR_BACKSLASH,          CHAR_RIGHT_SQUARE_BRACKET,
     CHAR_CIRCUMFLEX_ACCENT,  CHAR_UNDERSCORE,
     CHAR_GRAVE_ACCENT,       ESC_a,
     -ESC_b,                  0,
     -ESC_d,                  ESC_e,
     ESC_f,                   0,
     -ESC_h,                  0,
     0,                       -ESC_k,
     0,                       0,
     ESC_n,                   0,
     -ESC_p,                  0,
     ESC_r,                   -ESC_s,
     ESC_tee,                 0,
     -ESC_v,                  -ESC_w,
     0,                       0,
     -ESC_z
};

#else

/* This is the "abnormal" table for EBCDIC systems without UTF-8 support.
It runs from 'a' to '9'. For some minimal testing of EBCDIC features, the code
is sometimes compiled on an ASCII system. In this case, we must not use CHAR_a
because it is defined as 'a', which of course picks up the ASCII value. */

#if 'a' == 0x81                    /* Check for a real EBCDIC environment */
#define ESCAPES_FIRST       CHAR_a
#define ESCAPES_LAST        CHAR_9
#define UPPER_CASE(c)       (c+64)
#else                              /* Testing in an ASCII environment */
#define ESCAPES_FIRST  ((unsigned char)'\x81')   /* EBCDIC 'a' */
#define ESCAPES_LAST   ((unsigned char)'\xf9')   /* EBCDIC '9' */
#define UPPER_CASE(c)  (c-32)
#endif

static const short int escapes[] = {
/*  80 */        ESC_a, -ESC_b,       0, -ESC_d, ESC_e,  ESC_f,      0,
/*  88 */-ESC_h,     0,      0,     '{',      0,     0,      0,      0,
/*  90 */     0,     0, -ESC_k,       0,      0, ESC_n,      0, -ESC_p,
/*  98 */     0, ESC_r,      0,     '}',      0,     0,      0,      0,
/*  A0 */     0,   '~', -ESC_s, ESC_tee,      0,-ESC_v, -ESC_w,      0,
/*  A8 */     0,-ESC_z,      0,       0,      0,   '[',      0,      0,
/*  B0 */     0,     0,      0,       0,      0,     0,      0,      0,
/*  B8 */     0,     0,      0,       0,      0,   ']',    '=',    '-',
/*  C0 */   '{',-ESC_A, -ESC_B,  -ESC_C, -ESC_D,-ESC_E,      0, -ESC_G,
/*  C8 */-ESC_H,     0,      0,       0,      0,     0,      0,      0,
/*  D0 */   '}',     0, -ESC_K,       0,      0,-ESC_N,      0, -ESC_P,
/*  D8 */-ESC_Q,-ESC_R,      0,       0,      0,     0,      0,      0,
/*  E0 */  '\\',     0, -ESC_S,       0,      0,-ESC_V, -ESC_W, -ESC_X,
/*  E8 */     0,-ESC_Z,      0,       0,      0,     0,      0,      0,
/*  F0 */     0,     0,      0,       0,      0,     0,      0,      0,
/*  F8 */     0,     0
};

/* We also need a table of characters that may follow \c in an EBCDIC
environment for characters 0-31. */

static unsigned char ebcdic_escape_c[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_";

#endif   /* EBCDIC */


/* Table of special "verbs" like (*PRUNE). This is a short table, so it is
searched linearly. Put all the names into a single string, in order to reduce
the number of relocations when a shared library is dynamically linked. The
string is built from string macros so that it works in UTF-8 mode on EBCDIC
platforms. */

typedef struct verbitem {
  int   len;                 /* Length of verb name */
  int   op;                  /* Op when no arg, or -1 if arg mandatory */
  int   op_arg;              /* Op when arg present, or -1 if not allowed */
} verbitem;

static const char verbnames[] =
  "\0"                       /* Empty name is a shorthand for MARK */
  STRING_MARK0
  STRING_ACCEPT0
  STRING_COMMIT0
  STRING_F0
  STRING_FAIL0
  STRING_PRUNE0
  STRING_SKIP0
  STRING_THEN;

static const verbitem verbs[] = {
  { 0, -1,        OP_MARK },
  { 4, -1,        OP_MARK },
  { 6, OP_ACCEPT, -1 },
  { 6, OP_COMMIT, -1 },
  { 1, OP_FAIL,   -1 },
  { 4, OP_FAIL,   -1 },
  { 5, OP_PRUNE,  OP_PRUNE_ARG },
  { 4, OP_SKIP,   OP_SKIP_ARG  },
  { 4, OP_THEN,   OP_THEN_ARG  }
};

static const int verbcount = sizeof(verbs)/sizeof(verbitem);


/* Substitutes for [[:<:]] and [[:>:]], which mean start and end of word in
another regex library. */

static const PCRE2_UCHAR sub_start_of_word[] = {
  CHAR_BACKSLASH, CHAR_b, CHAR_LEFT_PARENTHESIS, CHAR_QUESTION_MARK,
  CHAR_EQUALS_SIGN, CHAR_BACKSLASH, CHAR_w, CHAR_RIGHT_PARENTHESIS, '\0' };

static const PCRE2_UCHAR sub_end_of_word[] = {
  CHAR_BACKSLASH, CHAR_b, CHAR_LEFT_PARENTHESIS, CHAR_QUESTION_MARK,
  CHAR_LESS_THAN_SIGN, CHAR_EQUALS_SIGN, CHAR_BACKSLASH, CHAR_w,
  CHAR_RIGHT_PARENTHESIS, '\0' };


/* Tables of names of POSIX character classes and their lengths. The names are
now all in a single string, to reduce the number of relocations when a shared
library is dynamically loaded. The list of lengths is terminated by a zero
length entry. The first three must be alpha, lower, upper, as this is assumed
for handling case independence. The indices for graph, print, and punct are
needed, so identify them. */

static const char posix_names[] =
  STRING_alpha0 STRING_lower0 STRING_upper0 STRING_alnum0
  STRING_ascii0 STRING_blank0 STRING_cntrl0 STRING_digit0
  STRING_graph0 STRING_print0 STRING_punct0 STRING_space0
  STRING_word0  STRING_xdigit;

static const uint8_t posix_name_lengths[] = {
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 6, 0 };

#define PC_GRAPH  8
#define PC_PRINT  9
#define PC_PUNCT 10


/* Table of class bit maps for each POSIX class. Each class is formed from a
base map, with an optional addition or removal of another map. Then, for some
classes, there is some additional tweaking: for [:blank:] the vertical space
characters are removed, and for [:alpha:] and [:alnum:] the underscore
character is removed. The triples in the table consist of the base map offset,
second map offset or -1 if no second map, and a non-negative value for map
addition or a negative value for map subtraction (if there are two maps). The
absolute value of the third field has these meanings: 0 => no tweaking, 1 =>
remove vertical space characters, 2 => remove underscore. */

static const int posix_class_maps[] = {
  cbit_word,  cbit_digit, -2,             /* alpha */
  cbit_lower, -1,          0,             /* lower */
  cbit_upper, -1,          0,             /* upper */
  cbit_word,  -1,          2,             /* alnum - word without underscore */
  cbit_print, cbit_cntrl,  0,             /* ascii */
  cbit_space, -1,          1,             /* blank - a GNU extension */
  cbit_cntrl, -1,          0,             /* cntrl */
  cbit_digit, -1,          0,             /* digit */
  cbit_graph, -1,          0,             /* graph */
  cbit_print, -1,          0,             /* print */
  cbit_punct, -1,          0,             /* punct */
  cbit_space, -1,          0,             /* space */
  cbit_word,  -1,          0,             /* word - a Perl extension */
  cbit_xdigit,-1,          0              /* xdigit */
};

/* Table of substitutes for \d etc when PCRE2_UCP is set. They are replaced by
Unicode property escapes. */

#ifdef SUPPORT_UNICODE
static const PCRE2_UCHAR string_PNd[]  = {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_N, CHAR_d, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pNd[]  = {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_N, CHAR_d, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PXsp[] = {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_s, CHAR_p, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pXsp[] = {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_s, CHAR_p, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PXwd[] = {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_w, CHAR_d, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pXwd[] = {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_w, CHAR_d, CHAR_RIGHT_CURLY_BRACKET, '\0' };

static PCRE2_SPTR substitutes[] = {
  string_PNd,           /* \D */
  string_pNd,           /* \d */
  string_PXsp,          /* \S */   /* Xsp is Perl space, but from 8.34, Perl */
  string_pXsp,          /* \s */   /* space and POSIX space are the same. */
  string_PXwd,          /* \W */
  string_pXwd           /* \w */
};

/* The POSIX class substitutes must be in the order of the POSIX class names,
defined above, and there are both positive and negative cases. NULL means no
general substitute of a Unicode property escape (\p or \P). However, for some
POSIX classes (e.g. graph, print, punct) a special property code is compiled
directly. */

static const PCRE2_UCHAR string_pCc[] =  {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_C, CHAR_c, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pL[] =   {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_L, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pLl[] =  {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_L, CHAR_l, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pLu[] =  {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_L, CHAR_u, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_pXan[] = {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_a, CHAR_n, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_h[] =    {
  CHAR_BACKSLASH, CHAR_h, '\0' };
static const PCRE2_UCHAR string_pXps[] = {
  CHAR_BACKSLASH, CHAR_p, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_p, CHAR_s, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PCc[] =  {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_C, CHAR_c, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PL[] =   {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_L, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PLl[] =  {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_L, CHAR_l, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PLu[] =  {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_L, CHAR_u, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_PXan[] = {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_a, CHAR_n, CHAR_RIGHT_CURLY_BRACKET, '\0' };
static const PCRE2_UCHAR string_H[] =    {
  CHAR_BACKSLASH, CHAR_H, '\0' };
static const PCRE2_UCHAR string_PXps[] = {
  CHAR_BACKSLASH, CHAR_P, CHAR_LEFT_CURLY_BRACKET,
  CHAR_X, CHAR_p, CHAR_s, CHAR_RIGHT_CURLY_BRACKET, '\0' };

static PCRE2_SPTR posix_substitutes[] = {
  string_pL,            /* alpha */
  string_pLl,           /* lower */
  string_pLu,           /* upper */
  string_pXan,          /* alnum */
  NULL,                 /* ascii */
  string_h,             /* blank */
  string_pCc,           /* cntrl */
  string_pNd,           /* digit */
  NULL,                 /* graph */
  NULL,                 /* print */
  NULL,                 /* punct */
  string_pXps,          /* space */   /* Xps is POSIX space, but from 8.34 */
  string_pXwd,          /* word  */   /* Perl and POSIX space are the same */
  NULL,                 /* xdigit */
  /* Negated cases */
  string_PL,            /* ^alpha */
  string_PLl,           /* ^lower */
  string_PLu,           /* ^upper */
  string_PXan,          /* ^alnum */
  NULL,                 /* ^ascii */
  string_H,             /* ^blank */
  string_PCc,           /* ^cntrl */
  string_PNd,           /* ^digit */
  NULL,                 /* ^graph */
  NULL,                 /* ^print */
  NULL,                 /* ^punct */
  string_PXps,          /* ^space */  /* Xps is POSIX space, but from 8.34 */
  string_PXwd,          /* ^word */   /* Perl and POSIX space are the same */
  NULL                  /* ^xdigit */
};
#define POSIX_SUBSIZE (sizeof(posix_substitutes) / sizeof(PCRE2_UCHAR *))
#endif  /* SUPPORT_UNICODE */

/* Masks for checking option settings. */

#define PUBLIC_COMPILE_OPTIONS \
  (PCRE2_ANCHORED|PCRE2_ALLOW_EMPTY_CLASS|PCRE2_ALT_BSUX|PCRE2_ALT_CIRCUMFLEX| \
   PCRE2_ALT_VERBNAMES|PCRE2_AUTO_CALLOUT|PCRE2_CASELESS|PCRE2_DOLLAR_ENDONLY| \
   PCRE2_DOTALL|PCRE2_DUPNAMES|PCRE2_EXTENDED|PCRE2_FIRSTLINE| \
   PCRE2_MATCH_UNSET_BACKREF|PCRE2_MULTILINE|PCRE2_NEVER_BACKSLASH_C| \
   PCRE2_NEVER_UCP|PCRE2_NEVER_UTF|PCRE2_NO_AUTO_CAPTURE| \
   PCRE2_NO_AUTO_POSSESS|PCRE2_NO_DOTSTAR_ANCHOR|PCRE2_NO_START_OPTIMIZE| \
   PCRE2_NO_UTF_CHECK|PCRE2_UCP|PCRE2_UNGREEDY|PCRE2_USE_OFFSET_LIMIT| \
   PCRE2_UTF)

/* Compile time error code numbers. They are given names so that they can more
easily be tracked. When a new number is added, the tables called eint1 and
eint2 in pcre2posix.c may need to be updated, and a new error text must be
added to compile_error_texts in pcre2_error.c. */

enum { ERR0 = COMPILE_ERROR_BASE,
       ERR1,  ERR2,  ERR3,  ERR4,  ERR5,  ERR6,  ERR7,  ERR8,  ERR9,  ERR10,
       ERR11, ERR12, ERR13, ERR14, ERR15, ERR16, ERR17, ERR18, ERR19, ERR20,
       ERR21, ERR22, ERR23, ERR24, ERR25, ERR26, ERR27, ERR28, ERR29, ERR30,
       ERR31, ERR32, ERR33, ERR34, ERR35, ERR36, ERR37, ERR38, ERR39, ERR40,
       ERR41, ERR42, ERR43, ERR44, ERR45, ERR46, ERR47, ERR48, ERR49, ERR50,
       ERR51, ERR52, ERR53, ERR54, ERR55, ERR56, ERR57, ERR58, ERR59, ERR60,
       ERR61, ERR62, ERR63, ERR64, ERR65, ERR66, ERR67, ERR68, ERR69, ERR70,
       ERR71, ERR72, ERR73, ERR74, ERR75, ERR76, ERR77, ERR78, ERR79, ERR80,
       ERR81, ERR82, ERR83, ERR84, ERR85, ERR86, ERR87, ERR88 };

/* Error codes that correspond to negative error codes returned by
find_fixedlength(). */

static int fixed_length_errors[] =
  {
  ERR0,    /* Not an error */
  ERR0,    /* Not an error; -1 is used for "process later" */
  ERR25,   /* Lookbehind is not fixed length */
  ERR36,   /* \C in lookbehind is not allowed */
  ERR87,   /* Lookbehind is too long */
  ERR86,   /* Pattern too complicated */
  ERR70    /* Internal error: unknown opcode encountered */
  };

/* This is a table of start-of-pattern options such as (*UTF) and settings such
as (*LIMIT_MATCH=nnnn) and (*CRLF). For completeness and backward
compatibility, (*UTFn) is supported in the relevant libraries, but (*UTF) is
generic and always supported. */

enum { PSO_OPT,     /* Value is an option bit */
       PSO_FLG,     /* Value is a flag bit */
       PSO_NL,      /* Value is a newline type */
       PSO_BSR,     /* Value is a \R type */
       PSO_LIMM,    /* Read integer value for match limit */
       PSO_LIMR };  /* Read integer value for recursion limit */

typedef struct pso {
  const uint8_t *name;
  uint16_t length;
  uint16_t type;
  uint32_t value;
} pso;

/* NB: STRING_UTFn_RIGHTPAR contains the length as well */

static pso pso_list[] = {
  { (uint8_t *)STRING_UTFn_RIGHTPAR,                  PSO_OPT, PCRE2_UTF },
  { (uint8_t *)STRING_UTF_RIGHTPAR,                4, PSO_OPT, PCRE2_UTF },
  { (uint8_t *)STRING_UCP_RIGHTPAR,                4, PSO_OPT, PCRE2_UCP },
  { (uint8_t *)STRING_NOTEMPTY_RIGHTPAR,           9, PSO_FLG, PCRE2_NOTEMPTY_SET },
  { (uint8_t *)STRING_NOTEMPTY_ATSTART_RIGHTPAR,  17, PSO_FLG, PCRE2_NE_ATST_SET },
  { (uint8_t *)STRING_NO_AUTO_POSSESS_RIGHTPAR,   16, PSO_OPT, PCRE2_NO_AUTO_POSSESS },
  { (uint8_t *)STRING_NO_DOTSTAR_ANCHOR_RIGHTPAR, 18, PSO_OPT, PCRE2_NO_DOTSTAR_ANCHOR },
  { (uint8_t *)STRING_NO_JIT_RIGHTPAR,             7, PSO_FLG, PCRE2_NOJIT },
  { (uint8_t *)STRING_NO_START_OPT_RIGHTPAR,      13, PSO_OPT, PCRE2_NO_START_OPTIMIZE },
  { (uint8_t *)STRING_LIMIT_MATCH_EQ,             12, PSO_LIMM, 0 },
  { (uint8_t *)STRING_LIMIT_RECURSION_EQ,         16, PSO_LIMR, 0 },
  { (uint8_t *)STRING_CR_RIGHTPAR,                 3, PSO_NL,  PCRE2_NEWLINE_CR },
  { (uint8_t *)STRING_LF_RIGHTPAR,                 3, PSO_NL,  PCRE2_NEWLINE_LF },
  { (uint8_t *)STRING_CRLF_RIGHTPAR,               5, PSO_NL,  PCRE2_NEWLINE_CRLF },
  { (uint8_t *)STRING_ANY_RIGHTPAR,                4, PSO_NL,  PCRE2_NEWLINE_ANY },
  { (uint8_t *)STRING_ANYCRLF_RIGHTPAR,            8, PSO_NL,  PCRE2_NEWLINE_ANYCRLF },
  { (uint8_t *)STRING_BSR_ANYCRLF_RIGHTPAR,       12, PSO_BSR, PCRE2_BSR_ANYCRLF },
  { (uint8_t *)STRING_BSR_UNICODE_RIGHTPAR,       12, PSO_BSR, PCRE2_BSR_UNICODE }
};

/* This table is used when converting repeating opcodes into possessified
versions as a result of an explicit possessive quantifier such as ++. A zero
value means there is no possessified version - in those cases the item in
question must be wrapped in ONCE brackets. The table is truncated at OP_CALLOUT
because all relevant opcodes are less than that. */

static const uint8_t opcode_possessify[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0 - 15  */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 16 - 31 */

  0,                       /* NOTI */
  OP_POSSTAR, 0,           /* STAR, MINSTAR */
  OP_POSPLUS, 0,           /* PLUS, MINPLUS */
  OP_POSQUERY, 0,          /* QUERY, MINQUERY */
  OP_POSUPTO, 0,           /* UPTO, MINUPTO */
  0,                       /* EXACT */
  0, 0, 0, 0,              /* POS{STAR,PLUS,QUERY,UPTO} */

  OP_POSSTARI, 0,          /* STARI, MINSTARI */
  OP_POSPLUSI, 0,          /* PLUSI, MINPLUSI */
  OP_POSQUERYI, 0,         /* QUERYI, MINQUERYI */
  OP_POSUPTOI, 0,          /* UPTOI, MINUPTOI */
  0,                       /* EXACTI */
  0, 0, 0, 0,              /* POS{STARI,PLUSI,QUERYI,UPTOI} */

  OP_NOTPOSSTAR, 0,        /* NOTSTAR, NOTMINSTAR */
  OP_NOTPOSPLUS, 0,        /* NOTPLUS, NOTMINPLUS */
  OP_NOTPOSQUERY, 0,       /* NOTQUERY, NOTMINQUERY */
  OP_NOTPOSUPTO, 0,        /* NOTUPTO, NOTMINUPTO */
  0,                       /* NOTEXACT */
  0, 0, 0, 0,              /* NOTPOS{STAR,PLUS,QUERY,UPTO} */

  OP_NOTPOSSTARI, 0,       /* NOTSTARI, NOTMINSTARI */
  OP_NOTPOSPLUSI, 0,       /* NOTPLUSI, NOTMINPLUSI */
  OP_NOTPOSQUERYI, 0,      /* NOTQUERYI, NOTMINQUERYI */
  OP_NOTPOSUPTOI, 0,       /* NOTUPTOI, NOTMINUPTOI */
  0,                       /* NOTEXACTI */
  0, 0, 0, 0,              /* NOTPOS{STARI,PLUSI,QUERYI,UPTOI} */

  OP_TYPEPOSSTAR, 0,       /* TYPESTAR, TYPEMINSTAR */
  OP_TYPEPOSPLUS, 0,       /* TYPEPLUS, TYPEMINPLUS */
  OP_TYPEPOSQUERY, 0,      /* TYPEQUERY, TYPEMINQUERY */
  OP_TYPEPOSUPTO, 0,       /* TYPEUPTO, TYPEMINUPTO */
  0,                       /* TYPEEXACT */
  0, 0, 0, 0,              /* TYPEPOS{STAR,PLUS,QUERY,UPTO} */

  OP_CRPOSSTAR, 0,         /* CRSTAR, CRMINSTAR */
  OP_CRPOSPLUS, 0,         /* CRPLUS, CRMINPLUS */
  OP_CRPOSQUERY, 0,        /* CRQUERY, CRMINQUERY */
  OP_CRPOSRANGE, 0,        /* CRRANGE, CRMINRANGE */
  0, 0, 0, 0,              /* CRPOS{STAR,PLUS,QUERY,RANGE} */

  0, 0, 0,                 /* CLASS, NCLASS, XCLASS */
  0, 0,                    /* REF, REFI */
  0, 0,                    /* DNREF, DNREFI */
  0, 0                     /* RECURSE, CALLOUT */
};



/*************************************************
*               Copy compiled code               *
*************************************************/

/* Compiled JIT code cannot be copied, so the new compiled block has no
associated JIT data. */

PCRE2_EXP_DEFN pcre2_code * PCRE2_CALL_CONVENTION
pcre2_code_copy(const pcre2_code *code)
{
PCRE2_SIZE* ref_count;
pcre2_code *newcode;

if (code == NULL) return NULL;
newcode = code->memctl.malloc(code->blocksize, code->memctl.memory_data);
if (newcode == NULL) return NULL;
memcpy(newcode, code, code->blocksize);
newcode->executable_jit = NULL;

/* If the code is one that has been deserialized, increment the reference count
in the decoded tables. */

if ((code->flags & PCRE2_DEREF_TABLES) != 0)
  {
  ref_count = (PCRE2_SIZE *)(code->tables + tables_length);
  (*ref_count)++;
  }

return newcode;
}



/*************************************************
*               Free compiled code               *
*************************************************/

PCRE2_EXP_DEFN void PCRE2_CALL_CONVENTION
pcre2_code_free(pcre2_code *code)
{
PCRE2_SIZE* ref_count;

if (code != NULL)
  {
  if (code->executable_jit != NULL)
    PRIV(jit_free)(code->executable_jit, &code->memctl);

  if ((code->flags & PCRE2_DEREF_TABLES) != 0)
    {
    /* Decoded tables belong to the codes after deserialization, and they must
    be freed when there are no more reference to them. The *ref_count should
    always be > 0. */

    ref_count = (PCRE2_SIZE *)(code->tables + tables_length);
    if (*ref_count > 0)
      {
      (*ref_count)--;
      if (*ref_count == 0)
        code->memctl.free((void *)code->tables, code->memctl.memory_data);
      }
    }

  code->memctl.free(code, code->memctl.memory_data);
  }
}



/*************************************************
*        Insert an automatic callout point       *
*************************************************/

/* This function is called when the PCRE2_AUTO_CALLOUT option is set, to insert
callout points before each pattern item.

Arguments:
  code           current code pointer
  ptr            current pattern pointer
  cb             general compile-time data

Returns:         new code pointer
*/

static PCRE2_UCHAR *
auto_callout(PCRE2_UCHAR *code, PCRE2_SPTR ptr, compile_block *cb)
{
code[0] = OP_CALLOUT;
PUT(code, 1, ptr - cb->start_pattern);  /* Pattern offset */
PUT(code, 1 + LINK_SIZE, 0);            /* Default length */
code[1 + 2*LINK_SIZE] = 255;
return code + PRIV(OP_lengths)[OP_CALLOUT];
}



/*************************************************
*         Complete a callout item                *
*************************************************/

/* A callout item contains the length of the next item in the pattern, which
we can't fill in till after we have reached the relevant point. This is used
for both automatic and manual callouts.

Arguments:
  previous_callout   points to previous callout item
  ptr                current pattern pointer
  cb                 general compile-time data

Returns:             nothing
*/

static void
complete_callout(PCRE2_UCHAR *previous_callout, PCRE2_SPTR ptr,
  compile_block *cb)
{
size_t length = (size_t)(ptr - cb->start_pattern - GET(previous_callout, 1));
PUT(previous_callout, 1 + LINK_SIZE, length);
}



/*************************************************
*        Find the fixed length of a branch       *
*************************************************/

/* Scan a branch and compute the fixed length of subject that will match it, if
the length is fixed. This is needed for dealing with lookbehind assertions. In
UTF mode, the result is in code units rather than bytes. The branch is
temporarily terminated with OP_END when this function is called.

This function is called when a lookbehind assertion is encountered, so that if
it fails, the error message can point to the correct place in the pattern.
However, we cannot do this when the assertion contains subroutine calls,
because they can be forward references. We solve this by remembering this case
and doing the check at the end; a flag specifies which mode we are running in.

Lookbehind lengths are held in 16-bit fields and the maximum value is defined
as LOOKBEHIND_MAX.

Arguments:
  code        points to the start of the pattern (the bracket)
  utf         TRUE in UTF mode
  atend       TRUE if called when the pattern is complete
  cb          the "compile data" structure
  recurses    chain of recurse_check to catch mutual recursion
  countptr    pointer to counter, to catch over-complexity

Returns:   if non-negative, the fixed length,
             or -1 if an OP_RECURSE item was encountered and atend is FALSE
             or -2 if there is no fixed length,
             or -3 if \C was encountered (in UTF mode only)
             or -4 if length is too long
             or -5 if regex is too complicated
             or -6 if an unknown opcode was encountered (internal error)
*/

#define FFL_LATER           (-1)
#define FFL_NOTFIXED        (-2)
#define FFL_BACKSLASHC      (-3)
#define FFL_TOOLONG         (-4)
#define FFL_TOOCOMPLICATED  (-5)
#define FFL_UNKNOWNOP       (-6)

static int
find_fixedlength(PCRE2_UCHAR *code, BOOL utf, BOOL atend, compile_block *cb,
  recurse_check *recurses, int *countptr)
{
uint32_t length = 0xffffffffu;   /* Unset */
uint32_t group = 0;
uint32_t groupinfo = 0;
recurse_check this_recurse;
register uint32_t branchlength = 0;
register PCRE2_UCHAR *cc = code + 1 + LINK_SIZE;

/* If this is a capturing group, we may have the answer cached, but we can only
use this information if there are no (?| groups in the pattern, because
otherwise group numbers are not unique. */

if (*code == OP_CBRA || *code == OP_CBRAPOS || *code == OP_SCBRA ||
    *code == OP_SCBRAPOS)
  {
  group = GET2(cc, 0);
  cc += IMM2_SIZE;
  groupinfo = cb->groupinfo[group];
  if ((cb->external_flags & PCRE2_DUPCAPUSED) == 0)
    {
    if ((groupinfo & GI_NOT_FIXED_LENGTH) != 0) return FFL_NOTFIXED;
    if ((groupinfo & GI_SET_FIXED_LENGTH) != 0)
      return groupinfo & GI_FIXED_LENGTH_MASK;
    }
  }

/* A large and/or complex regex can take too long to process. This can happen
more often when (?| groups are present in the pattern. */

if ((*countptr)++ > 2000) return FFL_TOOCOMPLICATED;

/* Scan along the opcodes for this branch. If we get to the end of the
branch, check the length against that of the other branches. */

for (;;)
  {
  int d;
  PCRE2_UCHAR *ce, *cs;
  register PCRE2_UCHAR op = *cc;

  if (branchlength > LOOKBEHIND_MAX) return FFL_TOOLONG;

  switch (op)
    {
    /* We only need to continue for OP_CBRA (normal capturing bracket) and
    OP_BRA (normal non-capturing bracket) because the other variants of these
    opcodes are all concerned with unlimited repeated groups, which of course
    are not of fixed length. */

    case OP_CBRA:
    case OP_BRA:
    case OP_ONCE:
    case OP_ONCE_NC:
    case OP_COND:
    d = find_fixedlength(cc, utf, atend, cb, recurses, countptr);
    if (d < 0) return d;
    branchlength += (uint32_t)d;
    do cc += GET(cc, 1); while (*cc == OP_ALT);
    cc += 1 + LINK_SIZE;
    break;

    /* Reached end of a branch; if it's a ket it is the end of a nested call.
    If it's ALT it is an alternation in a nested call. An ACCEPT is effectively
    an ALT. If it is END it's the end of the outer call. All can be handled by
    the same code. Note that we must not include the OP_KETRxxx opcodes here,
    because they all imply an unlimited repeat. */

    case OP_ALT:
    case OP_KET:
    case OP_END:
    case OP_ACCEPT:
    case OP_ASSERT_ACCEPT:
    if (length == 0xffffffffu) length = branchlength;
      else if (length != branchlength) goto ISNOTFIXED;
    if (*cc != OP_ALT)
      {
      if (group > 0)
        {
        groupinfo |= (uint32_t)(GI_SET_FIXED_LENGTH | length);
        cb->groupinfo[group] = groupinfo;
        }
      return (int)length;
      }
    cc += 1 + LINK_SIZE;
    branchlength = 0;
    break;

    /* A true recursion implies not fixed length, but a subroutine call may
    be OK. If the subroutine is a forward reference, we can't deal with
    it until the end of the pattern, so return FFL_LATER. */

    case OP_RECURSE:
    if (!atend) return FFL_LATER;
    cs = ce = (PCRE2_UCHAR *)cb->start_code + GET(cc, 1); /* Start subpattern */
    do ce += GET(ce, 1); while (*ce == OP_ALT);           /* End subpattern */
    if (cc > cs && cc < ce) goto ISNOTFIXED;          /* Recursion */
    else   /* Check for mutual recursion */
      {
      recurse_check *r = recurses;
      for (r = recurses; r != NULL; r = r->prev) if (r->group == cs) break;
      if (r != NULL) goto ISNOTFIXED;   /* Mutual recursion */
      }
    this_recurse.prev = recurses;
    this_recurse.group = cs;
    d = find_fixedlength(cs, utf, atend, cb, &this_recurse, countptr);
    if (d < 0) return d;
    branchlength += (uint32_t)d;
    cc += 1 + LINK_SIZE;
    break;

    /* Skip over assertive subpatterns. Note that we must increment cc by
    1 + LINK_SIZE at the end, not by OP_length[*cc] because in a recursive
    situation this assertion may be the one that is ultimately being checked
    for having a fixed length, in which case its terminating OP_KET will have
    been temporarily replaced by OP_END. */

    case OP_ASSERT:
    case OP_ASSERT_NOT:
    case OP_ASSERTBACK:
    case OP_ASSERTBACK_NOT:
    do cc += GET(cc, 1); while (*cc == OP_ALT);
    cc += 1 + LINK_SIZE;
    break;

    /* Skip over things that don't match chars */

    case OP_MARK:
    case OP_PRUNE_ARG:
    case OP_SKIP_ARG:
    case OP_THEN_ARG:
    cc += cc[1] + PRIV(OP_lengths)[*cc];
    break;

    case OP_CALLOUT:
    case OP_CIRC:
    case OP_CIRCM:
    case OP_CLOSE:
    case OP_COMMIT:
    case OP_CREF:
    case OP_FALSE:
    case OP_TRUE:
    case OP_DNCREF:
    case OP_DNRREF:
    case OP_DOLL:
    case OP_DOLLM:
    case OP_EOD:
    case OP_EODN:
    case OP_FAIL:
    case OP_NOT_WORD_BOUNDARY:
    case OP_PRUNE:
    case OP_REVERSE:
    case OP_RREF:
    case OP_SET_SOM:
    case OP_SKIP:
    case OP_SOD:
    case OP_SOM:
    case OP_THEN:
    case OP_WORD_BOUNDARY:
    cc += PRIV(OP_lengths)[*cc];
    break;

    case OP_CALLOUT_STR:
    cc += GET(cc, 1 + 2*LINK_SIZE);
    break;

    /* Handle literal characters */

    case OP_CHAR:
    case OP_CHARI:
    case OP_NOT:
    case OP_NOTI:
    branchlength++;
    cc += 2;
#ifdef SUPPORT_UNICODE
    if (utf && HAS_EXTRALEN(cc[-1])) cc += GET_EXTRALEN(cc[-1]);
#endif
    break;

    /* Handle exact repetitions. The count is already in characters, but we
    need to skip over a multibyte character in UTF8 mode.  */

    case OP_EXACT:
    case OP_EXACTI:
    case OP_NOTEXACT:
    case OP_NOTEXACTI:
    branchlength += GET2(cc,1);
    cc += 2 + IMM2_SIZE;
#ifdef SUPPORT_UNICODE
    if (utf && HAS_EXTRALEN(cc[-1])) cc += GET_EXTRALEN(cc[-1]);
#endif
    break;

    case OP_TYPEEXACT:
    branchlength += GET2(cc,1);
    if (cc[1 + IMM2_SIZE] == OP_PROP || cc[1 + IMM2_SIZE] == OP_NOTPROP)
      cc += 2;
    cc += 1 + IMM2_SIZE + 1;
    break;

    /* Handle single-char matchers */

    case OP_PROP:
    case OP_NOTPROP:
    cc += 2;
    /* Fall through */

    case OP_HSPACE:
    case OP_VSPACE:
    case OP_NOT_HSPACE:
    case OP_NOT_VSPACE:
    case OP_NOT_DIGIT:
    case OP_DIGIT:
    case OP_NOT_WHITESPACE:
    case OP_WHITESPACE:
    case OP_NOT_WORDCHAR:
    case OP_WORDCHAR:
    case OP_ANY:
    case OP_ALLANY:
    branchlength++;
    cc++;
    break;

    /* The single-byte matcher isn't allowed. This only happens in UTF-8 or
    UTF-16 mode; otherwise \C is coded as OP_ALLANY. */

    case OP_ANYBYTE:
    return FFL_BACKSLASHC;

    /* Check a class for variable quantification */

    case OP_CLASS:
    case OP_NCLASS:
#ifdef SUPPORT_WIDE_CHARS
    case OP_XCLASS:
    /* The original code caused an unsigned overflow in 64 bit systems,
    so now we use a conditional statement. */
    if (op == OP_XCLASS)
      cc += GET(cc, 1);
    else
      cc += PRIV(OP_lengths)[OP_CLASS];
#else
    cc += PRIV(OP_lengths)[OP_CLASS];
#endif

    switch (*cc)
      {
      case OP_CRSTAR:
      case OP_CRMINSTAR:
      case OP_CRPLUS:
      case OP_CRMINPLUS:
      case OP_CRQUERY:
      case OP_CRMINQUERY:
      case OP_CRPOSSTAR:
      case OP_CRPOSPLUS:
      case OP_CRPOSQUERY:
      goto ISNOTFIXED;

      case OP_CRRANGE:
      case OP_CRMINRANGE:
      case OP_CRPOSRANGE:
      if (GET2(cc,1) != GET2(cc,1+IMM2_SIZE)) goto ISNOTFIXED;
      branchlength += GET2(cc,1);
      cc += 1 + 2 * IMM2_SIZE;
      break;

      default:
      branchlength++;
      }
    break;

    /* Anything else is variable length */

    case OP_ANYNL:
    case OP_BRAMINZERO:
    case OP_BRAPOS:
    case OP_BRAPOSZERO:
    case OP_BRAZERO:
    case OP_CBRAPOS:
    case OP_EXTUNI:
    case OP_KETRMAX:
    case OP_KETRMIN:
    case OP_KETRPOS:
    case OP_MINPLUS:
    case OP_MINPLUSI:
    case OP_MINQUERY:
    case OP_MINQUERYI:
    case OP_MINSTAR:
    case OP_MINSTARI:
    case OP_MINUPTO:
    case OP_MINUPTOI:
    case OP_NOTMINPLUS:
    case OP_NOTMINPLUSI:
    case OP_NOTMINQUERY:
    case OP_NOTMINQUERYI:
    case OP_NOTMINSTAR:
    case OP_NOTMINSTARI:
    case OP_NOTMINUPTO:
    case OP_NOTMINUPTOI:
    case OP_NOTPLUS:
    case OP_NOTPLUSI:
    case OP_NOTPOSPLUS:
    case OP_NOTPOSPLUSI:
    case OP_NOTPOSQUERY:
    case OP_NOTPOSQUERYI:
    case OP_NOTPOSSTAR:
    case OP_NOTPOSSTARI:
    case OP_NOTPOSUPTO:
    case OP_NOTPOSUPTOI:
    case OP_NOTQUERY:
    case OP_NOTQUERYI:
    case OP_NOTSTAR:
    case OP_NOTSTARI:
    case OP_NOTUPTO:
    case OP_NOTUPTOI:
    case OP_PLUS:
    case OP_PLUSI:
    case OP_POSPLUS:
    case OP_POSPLUSI:
    case OP_POSQUERY:
    case OP_POSQUERYI:
    case OP_POSSTAR:
    case OP_POSSTARI:
    case OP_POSUPTO:
    case OP_POSUPTOI:
    case OP_QUERY:
    case OP_QUERYI:
    case OP_REF:
    case OP_REFI:
    case OP_DNREF:
    case OP_DNREFI:
    case OP_SBRA:
    case OP_SBRAPOS:
    case OP_SCBRA:
    case OP_SCBRAPOS:
    case OP_SCOND:
    case OP_SKIPZERO:
    case OP_STAR:
    case OP_STARI:
    case OP_TYPEMINPLUS:
    case OP_TYPEMINQUERY:
    case OP_TYPEMINSTAR:
    case OP_TYPEMINUPTO:
    case OP_TYPEPLUS:
    case OP_TYPEPOSPLUS:
    case OP_TYPEPOSQUERY:
    case OP_TYPEPOSSTAR:
    case OP_TYPEPOSUPTO:
    case OP_TYPEQUERY:
    case OP_TYPESTAR:
    case OP_TYPEUPTO:
    case OP_UPTO:
    case OP_UPTOI:
    goto ISNOTFIXED;

    /* Catch unrecognized opcodes so that when new ones are added they
    are not forgotten, as has happened in the past. */

    default:
    return FFL_UNKNOWNOP;
    }
  }
/* Control never gets here except by goto. */

ISNOTFIXED:
if (group > 0)
  {
  groupinfo |= GI_NOT_FIXED_LENGTH;
  cb->groupinfo[group] = groupinfo;
  }
return FFL_NOTFIXED;
}



/*************************************************
*      Find first significant op code            *
*************************************************/

/* This is called by several functions that scan a compiled expression looking
for a fixed first character, or an anchoring op code etc. It skips over things
that do not influence this. For some calls, it makes sense to skip negative
forward and all backward assertions, and also the \b assertion; for others it
does not.

Arguments:
  code         pointer to the start of the group
  skipassert   TRUE if certain assertions are to be skipped

Returns:       pointer to the first significant opcode
*/

static const PCRE2_UCHAR*
first_significant_code(PCRE2_SPTR code, BOOL skipassert)
{
for (;;)
  {
  switch ((int)*code)
    {
    case OP_ASSERT_NOT:
    case OP_ASSERTBACK:
    case OP_ASSERTBACK_NOT:
    if (!skipassert) return code;
    do code += GET(code, 1); while (*code == OP_ALT);
    code += PRIV(OP_lengths)[*code];
    break;

    case OP_WORD_BOUNDARY:
    case OP_NOT_WORD_BOUNDARY:
    if (!skipassert) return code;
    /* Fall through */

    case OP_CALLOUT:
    case OP_CREF:
    case OP_DNCREF:
    case OP_RREF:
    case OP_DNRREF:
    case OP_FALSE:
    case OP_TRUE:
    code += PRIV(OP_lengths)[*code];
    break;

    case OP_CALLOUT_STR:
    code += GET(code, 1 + 2*LINK_SIZE);
    break;

    default:
    return code;
    }
  }
/* Control never reaches here */
}



/*************************************************
*    Scan compiled branch for non-emptiness      *
*************************************************/

/* This function scans through a branch of a compiled pattern to see whether it
can match the empty string. It is called at the end of compiling to check the
entire pattern, and from compile_branch() when checking for an unlimited repeat
of a group that can match nothing. In the latter case it is called only when
doing the real compile, not during the pre-compile that measures the size of
the compiled pattern.

Note that first_significant_code() skips over backward and negative forward
assertions when its final argument is TRUE. If we hit an unclosed bracket, we
return "empty" - this means we've struck an inner bracket whose current branch
will already have been scanned.

Arguments:
  code        points to start of search
  endcode     points to where to stop
  utf         TRUE if in UTF mode
  cb          compile data
  atend       TRUE if being called to check an entire pattern
  recurses    chain of recurse_check to catch mutual recursion
  countptr    pointer to count to catch over-complicated pattern

Returns:      0 if what is matched cannot be empty
              1 if what is matched could be empty
             -1 if the pattern is too complicated
*/

#define CBE_NOTEMPTY          0
#define CBE_EMPTY             1
#define CBE_TOOCOMPLICATED  (-1)


static int
could_be_empty_branch(PCRE2_SPTR code, PCRE2_SPTR endcode, BOOL utf,
  compile_block *cb, BOOL atend, recurse_check *recurses, int *countptr)
{
uint32_t group = 0;
uint32_t groupinfo = 0;
register PCRE2_UCHAR c;
recurse_check this_recurse;

/* If what we are checking has already been set as "could be empty", we know
the answer. */

if (*code >= OP_SBRA && *code <= OP_SCOND) return CBE_EMPTY;

/* If this is a capturing group, we may have the answer cached, but we can only
use this information if there are no (?| groups in the pattern, because
otherwise group numbers are not unique. */

if ((cb->external_flags & PCRE2_DUPCAPUSED) == 0 &&
    (*code == OP_CBRA || *code == OP_CBRAPOS))
  {
  group = GET2(code, 1 + LINK_SIZE);
  groupinfo = cb->groupinfo[group];
  if ((groupinfo & GI_SET_COULD_BE_EMPTY) != 0)
    return ((groupinfo & GI_COULD_BE_EMPTY) != 0)? CBE_EMPTY : CBE_NOTEMPTY;
  }

/* A large and/or complex regex can take too long to process. We have to assume
it can match an empty string. This can happen more often when (?| groups are
present in the pattern and the caching is disabled. Setting the cap at 1100
allows the test for more than 1023 capturing patterns to work. */

if ((*countptr)++ > 1100) return CBE_TOOCOMPLICATED;

/* Scan the opcodes for this branch. */

for (code = first_significant_code(code + PRIV(OP_lengths)[*code], TRUE);
     code < endcode;
     code = first_significant_code(code + PRIV(OP_lengths)[c], TRUE))
  {
  PCRE2_SPTR ccode;

  c = *code;

  /* Skip over forward assertions; the other assertions are skipped by
  first_significant_code() with a TRUE final argument. */

  if (c == OP_ASSERT)
    {
    do code += GET(code, 1); while (*code == OP_ALT);
    c = *code;
    continue;
    }

  /* For a recursion/subroutine call we can scan the recursion when this
  function is called at the end, to check a complete pattern. Before then,
  recursions just have the group number as their argument and in any case may
  be forward references. In that situation, we return CBE_EMPTY, just in case.
  It means that unlimited repeats of groups that contain recursions are always
  treated as "could be empty" - which just adds a bit more processing time
  because of the runtime check. */

  if (c == OP_RECURSE)
    {
    PCRE2_SPTR scode, endgroup;
    BOOL empty_branch;

    if (!atend) goto ISTRUE;
    scode = cb->start_code + GET(code, 1);
    endgroup = scode;

    /* We need to detect whether this is a recursive call, as otherwise there
    will be an infinite loop. If it is a recursion, just skip over it. Simple
    recursions are easily detected. For mutual recursions we keep a chain on
    the stack. */

    do endgroup += GET(endgroup, 1); while (*endgroup == OP_ALT);
    if (code >= scode && code <= endgroup) continue;  /* Simple recursion */
    else
      {
      recurse_check *r = recurses;
      for (r = recurses; r != NULL; r = r->prev)
        if (r->group == scode) break;
      if (r != NULL) continue;   /* Mutual recursion */
      }

    /* Scan the referenced group, remembering it on the stack chain to detect
    mutual recursions. */

    empty_branch = FALSE;
    this_recurse.prev = recurses;
    this_recurse.group = scode;

    do
      {
      int rc = could_be_empty_branch(scode, endcode, utf, cb, atend,
        &this_recurse, countptr);
      if (rc < 0) return rc;
      if (rc > 0)
        {
        empty_branch = TRUE;
        break;
        }
      scode += GET(scode, 1);
      }
    while (*scode == OP_ALT);

    if (!empty_branch) goto ISFALSE;  /* All branches are non-empty */
    continue;
    }

  /* Groups with zero repeats can of course be empty; skip them. */

  if (c == OP_BRAZERO || c == OP_BRAMINZERO || c == OP_SKIPZERO ||
      c == OP_BRAPOSZERO)
    {
    code += PRIV(OP_lengths)[c];
    do code += GET(code, 1); while (*code == OP_ALT);
    c = *code;
    continue;
    }

  /* A nested group that is already marked as "could be empty" can just be
  skipped. */

  if (c == OP_SBRA  || c == OP_SBRAPOS ||
      c == OP_SCBRA || c == OP_SCBRAPOS)
    {
    do code += GET(code, 1); while (*code == OP_ALT);
    c = *code;
    continue;
    }

  /* For other groups, scan the branches. */

  if (c == OP_BRA  || c == OP_BRAPOS ||
      c == OP_CBRA || c == OP_CBRAPOS ||
      c == OP_ONCE || c == OP_ONCE_NC ||
      c == OP_COND || c == OP_SCOND)
    {
    BOOL empty_branch;
    if (GET(code, 1) == 0) goto ISTRUE;    /* Hit unclosed bracket */

    /* If a conditional group has only one branch, there is a second, implied,
    empty branch, so just skip over the conditional, because it could be empty.
    Otherwise, scan the individual branches of the group. */

    if (c == OP_COND && code[GET(code, 1)] != OP_ALT)
      code += GET(code, 1);
    else
      {
      empty_branch = FALSE;
      do
        {
        if (!empty_branch)
          {
          int rc = could_be_empty_branch(code, endcode, utf, cb, atend,
            recurses, countptr);
          if (rc < 0) return rc;
          if (rc > 0) empty_branch = TRUE;
          }
        code += GET(code, 1);
        }
      while (*code == OP_ALT);
      if (!empty_branch) goto ISFALSE;   /* All branches are non-empty */
      }

    c = *code;
    continue;
    }

  /* Handle the other opcodes */

  switch (c)
    {
    /* Check for quantifiers after a class. XCLASS is used for classes that
    cannot be represented just by a bit map. This includes negated single
    high-valued characters. The length in PRIV(OP_lengths)[] is zero; the
    actual length is stored in the compiled code, so we must update "code"
    here. */

#if defined SUPPORT_UNICODE || PCRE2_CODE_UNIT_WIDTH != 8
    case OP_XCLASS:
    ccode = code += GET(code, 1);
    goto CHECK_CLASS_REPEAT;
#endif

    case OP_CLASS:
    case OP_NCLASS:
    ccode = code + PRIV(OP_lengths)[OP_CLASS];

#if defined SUPPORT_UNICODE || PCRE2_CODE_UNIT_WIDTH != 8
    CHECK_CLASS_REPEAT:
#endif

    switch (*ccode)
      {
      case OP_CRSTAR:            /* These could be empty; continue */
      case OP_CRMINSTAR:
      case OP_CRQUERY:
      case OP_CRMINQUERY:
      case OP_CRPOSSTAR:
      case OP_CRPOSQUERY:
      break;

      default:                   /* Non-repeat => class must match */
      case OP_CRPLUS:            /* These repeats aren't empty */
      case OP_CRMINPLUS:
      case OP_CRPOSPLUS:
      goto ISFALSE;

      case OP_CRRANGE:
      case OP_CRMINRANGE:
      case OP_CRPOSRANGE:
      if (GET2(ccode, 1) > 0) goto ISFALSE;  /* Minimum > 0 */
      break;
      }
    break;

    /* Opcodes that must match a character */

    case OP_ANY:
    case OP_ALLANY:
    case OP_ANYBYTE:

    case OP_PROP:
    case OP_NOTPROP:
    case OP_ANYNL:

    case OP_NOT_HSPACE:
    case OP_HSPACE:
    case OP_NOT_VSPACE:
    case OP_VSPACE:
    case OP_EXTUNI:

    case OP_NOT_DIGIT:
    case OP_DIGIT:
    case OP_NOT_WHITESPACE:
    case OP_WHITESPACE:
    case OP_NOT_WORDCHAR:
    case OP_WORDCHAR:

    case OP_CHAR:
    case OP_CHARI:
    case OP_NOT:
    case OP_NOTI:

    case OP_PLUS:
    case OP_PLUSI:
    case OP_MINPLUS:
    case OP_MINPLUSI:

    case OP_NOTPLUS:
    case OP_NOTPLUSI:
    case OP_NOTMINPLUS:
    case OP_NOTMINPLUSI:

    case OP_POSPLUS:
    case OP_POSPLUSI:
    case OP_NOTPOSPLUS:
    case OP_NOTPOSPLUSI:

    case OP_EXACT:
    case OP_EXACTI:
    case OP_NOTEXACT:
    case OP_NOTEXACTI:

    case OP_TYPEPLUS:
    case OP_TYPEMINPLUS:
    case OP_TYPEPOSPLUS:
    case OP_TYPEEXACT:
    goto ISFALSE;

    /* These are going to continue, as they may be empty, but we have to
    fudge the length for the \p and \P cases. */

    case OP_TYPESTAR:
    case OP_TYPEMINSTAR:
    case OP_TYPEPOSSTAR:
    case OP_TYPEQUERY:
    case OP_TYPEMINQUERY:
    case OP_TYPEPOSQUERY:
    if (code[1] == OP_PROP || code[1] == OP_NOTPROP) code += 2;
    break;

    /* Same for these */

    case OP_TYPEUPTO:
    case OP_TYPEMINUPTO:
    case OP_TYPEPOSUPTO:
    if (code[1 + IMM2_SIZE] == OP_PROP || code[1 + IMM2_SIZE] == OP_NOTPROP)
      code += 2;
    break;

    /* End of branch */

    case OP_KET:
    case OP_KETRMAX:
    case OP_KETRMIN:
    case OP_KETRPOS:
    case OP_ALT:
    goto ISTRUE;

    /* In UTF-8 or UTF-16 mode, STAR, MINSTAR, POSSTAR, QUERY, MINQUERY,
    POSQUERY, UPTO, MINUPTO, and POSUPTO and their caseless and negative
    versions may be followed by a multibyte character. */

#ifdef MAYBE_UTF_MULTI
    case OP_STAR:
    case OP_STARI:
    case OP_NOTSTAR:
    case OP_NOTSTARI:

    case OP_MINSTAR:
    case OP_MINSTARI:
    case OP_NOTMINSTAR:
    case OP_NOTMINSTARI:

    case OP_POSSTAR:
    case OP_POSSTARI:
    case OP_NOTPOSSTAR:
    case OP_NOTPOSSTARI:

    case OP_QUERY:
    case OP_QUERYI:
    case OP_NOTQUERY:
    case OP_NOTQUERYI:

    case OP_MINQUERY:
    case OP_MINQUERYI:
    case OP_NOTMINQUERY:
    case OP_NOTMINQUERYI:

    case OP_POSQUERY:
    case OP_POSQUERYI:
    case OP_NOTPOSQUERY:
    case OP_NOTPOSQUERYI:
    if (utf && HAS_EXTRALEN(code[1])) code += GET_EXTRALEN(code[1]);
    break;

    case OP_UPTO:
    case OP_UPTOI:
    case OP_NOTUPTO:
    case OP_NOTUPTOI:

    case OP_MINUPTO:
    case OP_MINUPTOI:
    case OP_NOTMINUPTO:
    case OP_NOTMINUPTOI:

    case OP_POSUPTO:
    case OP_POSUPTOI:
    case OP_NOTPOSUPTO:
    case OP_NOTPOSUPTOI:
    if (utf && HAS_EXTRALEN(code[1 + IMM2_SIZE])) code += GET_EXTRALEN(code[1 + IMM2_SIZE]);
    break;
#endif  /* MAYBE_UTF_MULTI */

    /* MARK, and PRUNE/SKIP/THEN with an argument must skip over the argument
    string. */

    case OP_MARK:
    case OP_PRUNE_ARG:
    case OP_SKIP_ARG:
    case OP_THEN_ARG:
    code += code[1];
    break;

    /* None of the remaining opcodes are required to match a character. */

    default:
    break;
    }
  }

ISTRUE:
groupinfo |= GI_COULD_BE_EMPTY;

ISFALSE:
if (group > 0) cb->groupinfo[group] = groupinfo | GI_SET_COULD_BE_EMPTY;

return ((groupinfo & GI_COULD_BE_EMPTY) != 0)? CBE_EMPTY : CBE_NOTEMPTY;
}



/*************************************************
*            Check for counted repeat            *
*************************************************/

/* This function is called when a '{' is encountered in a place where it might
start a quantifier. It looks ahead to see if it really is a quantifier, that
is, one of the forms {ddd} {ddd,} or {ddd,ddd} where the ddds are digits.

Argument:   pointer to the first char after '{'
Returns:    TRUE or FALSE
*/

static BOOL
is_counted_repeat(PCRE2_SPTR p)
{
if (!IS_DIGIT(*p)) return FALSE;
p++;
while (IS_DIGIT(*p)) p++;
if (*p == CHAR_RIGHT_CURLY_BRACKET) return TRUE;

if (*p++ != CHAR_COMMA) return FALSE;
if (*p == CHAR_RIGHT_CURLY_BRACKET) return TRUE;

if (!IS_DIGIT(*p)) return FALSE;
p++;
while (IS_DIGIT(*p)) p++;

return (*p == CHAR_RIGHT_CURLY_BRACKET);
}



/*************************************************
*            Handle escapes                      *
*************************************************/

/* This function is called when a \ has been encountered. It either returns a
positive value for a simple escape such as \d, or 0 for a data character, which
is placed in chptr. A backreference to group n is returned as negative n. On
entry, ptr is pointing at the \. On exit, it points the final code unit of the
escape sequence.

This function is also called from pcre2_substitute() to handle escape sequences
in replacement strings. In this case, the cb argument is NULL, and only
sequences that define a data character are recognised. The isclass argument is
not relevant, but the options argument is the final value of the compiled
pattern's options.

There is one "trick" case: when a sequence such as [[:>:]] or \s in UCP mode is
processed, it is replaced by a nested alternative sequence. If this contains a
backslash (which is usually does), ptrend does not point to its end - it still
points to the end of the whole pattern. However, we can detect this case
because cb->nestptr[0] will be non-NULL. The nested sequences are all zero-
terminated and there are only ever two levels of nesting.

Arguments:
  ptrptr         points to the input position pointer
  ptrend         points to the end of the input
  chptr          points to a returned data character
  errorcodeptr   points to the errorcode variable (containing zero)
  options        the current options bits
  isclass        TRUE if inside a character class
  cb             compile data block

Returns:         zero => a data character
                 positive => a special escape sequence
                 negative => a back reference
                 on error, errorcodeptr is set non-zero
*/

int
PRIV(check_escape)(PCRE2_SPTR *ptrptr, PCRE2_SPTR ptrend, uint32_t *chptr,
  int *errorcodeptr, uint32_t options, BOOL isclass, compile_block *cb)
{
BOOL utf = (options & PCRE2_UTF) != 0;
PCRE2_SPTR ptr = *ptrptr + 1;
register uint32_t c, cc;
int escape = 0;
int i;

/* Find the end of a nested insert. */

if (cb != NULL && cb->nestptr[0] != NULL)
  ptrend = ptr + PRIV(strlen)(ptr);

/* If backslash is at the end of the string, it's an error. */

if (ptr >= ptrend)
  {
  *errorcodeptr = ERR1;
  return 0;
  }

GETCHARINCTEST(c, ptr);         /* Get character value, increment pointer */
ptr--;                          /* Set pointer back to the last code unit */

/* Non-alphanumerics are literals, so we just leave the value in c. An initial
value test saves a memory lookup for code points outside the alphanumeric
range. Otherwise, do a table lookup. A non-zero result is something that can be
returned immediately. Otherwise further processing is required. */

if (c < ESCAPES_FIRST || c > ESCAPES_LAST) {}  /* Definitely literal */

else if ((i = escapes[c - ESCAPES_FIRST]) != 0)
  {
  if (i > 0) c = (uint32_t)i; else  /* Positive is a data character */
    {
    escape = -i;                    /* Else return a special escape */
    if (escape == ESC_P || escape == ESC_p || escape == ESC_X)
      cb->external_flags |= PCRE2_HASBKPORX;   /* Note \P, \p, or \X */
    }
  }

/* Escapes that need further processing, including those that are unknown.
When called from pcre2_substitute(), only \c, \o, and \x are recognized (and \u
when BSUX is set). */

else
  {
  PCRE2_SPTR oldptr;
  BOOL braced, negated, overflow;
  unsigned int s;

  /* Filter calls from pcre2_substitute(). */

  if (cb == NULL && c != CHAR_c && c != CHAR_o && c != CHAR_x &&
      (c != CHAR_u || (options & PCRE2_ALT_BSUX) != 0))
    {
    *errorcodeptr = ERR3;
    return 0;
    }

  switch (c)
    {
    /* A number of Perl escapes are not handled by PCRE. We give an explicit
    error. */

    case CHAR_l:
    case CHAR_L:
    *errorcodeptr = ERR37;
    break;

    /* \u is unrecognized when PCRE2_ALT_BSUX is not set. When it is treated
    specially, \u must be followed by four hex digits. Otherwise it is a
    lowercase u letter. */

    case CHAR_u:
    if ((options & PCRE2_ALT_BSUX) == 0) *errorcodeptr = ERR37; else
      {
      uint32_t xc;
      if ((cc = XDIGIT(ptr[1])) == 0xff) break;  /* Not a hex digit */
      if ((xc = XDIGIT(ptr[2])) == 0xff) break;  /* Not a hex digit */
      cc = (cc << 4) | xc;
      if ((xc = XDIGIT(ptr[3])) == 0xff) break;  /* Not a hex digit */
      cc = (cc << 4) | xc;
      if ((xc = XDIGIT(ptr[4])) == 0xff) break;  /* Not a hex digit */
      c = (cc << 4) | xc;
      ptr += 4;
      if (utf)
        {
        if (c > 0x10ffffU) *errorcodeptr = ERR77;
          else if (c >= 0xd800 && c <= 0xdfff) *errorcodeptr = ERR73;
        }
      else if (c > MAX_NON_UTF_CHAR) *errorcodeptr = ERR77;
      }
    break;

    case CHAR_U:
    /* \U is unrecognized unless PCRE2_ALT_BSUX is set, in which case it is an
    upper case letter. */
    if ((options & PCRE2_ALT_BSUX) == 0) *errorcodeptr = ERR37;
    break;

    /* In a character class, \g is just a literal "g". Outside a character
    class, \g must be followed by one of a number of specific things:

    (1) A number, either plain or braced. If positive, it is an absolute
    backreference. If negative, it is a relative backreference. This is a Perl
    5.10 feature.

    (2) Perl 5.10 also supports \g{name} as a reference to a named group. This
    is part of Perl's movement towards a unified syntax for back references. As
    this is synonymous with \k{name}, we fudge it up by pretending it really
    was \k.

    (3) For Oniguruma compatibility we also support \g followed by a name or a
    number either in angle brackets or in single quotes. However, these are
    (possibly recursive) subroutine calls, _not_ backreferences. Just return
    the ESC_g code (cf \k). */

    case CHAR_g:
    if (isclass) break;
    if (ptr[1] == CHAR_LESS_THAN_SIGN || ptr[1] == CHAR_APOSTROPHE)
      {
      escape = ESC_g;
      break;
      }

    /* Handle the Perl-compatible cases */

    if (ptr[1] == CHAR_LEFT_CURLY_BRACKET)
      {
      PCRE2_SPTR p;
      for (p = ptr+2; *p != CHAR_NULL && *p != CHAR_RIGHT_CURLY_BRACKET; p++)
        if (*p != CHAR_MINUS && !IS_DIGIT(*p)) break;
      if (*p != CHAR_NULL && *p != CHAR_RIGHT_CURLY_BRACKET)
        {
        escape = ESC_k;
        break;
        }
      braced = TRUE;
      ptr++;
      }
    else braced = FALSE;

    if (ptr[1] == CHAR_MINUS)
      {
      negated = TRUE;
      ptr++;
      }
    else negated = FALSE;

    /* The integer range is limited by the machine's int representation. */
    s = 0;
    overflow = FALSE;
    while (IS_DIGIT(ptr[1]))
      {
      if (s > INT_MAX / 10 - 1) /* Integer overflow */
        {
        overflow = TRUE;
        break;
        }
      s = s * 10 + (unsigned int)(*(++ptr) - CHAR_0);
      }
    if (overflow) /* Integer overflow */
      {
      while (IS_DIGIT(ptr[1])) ptr++;
      *errorcodeptr = ERR61;
      break;
      }

    if (braced && *(++ptr) != CHAR_RIGHT_CURLY_BRACKET)
      {
      *errorcodeptr = ERR57;
      break;
      }

    if (s == 0)
      {
      *errorcodeptr = ERR58;
      break;
      }

    if (negated)
      {
      if (s > cb->bracount)
        {
        *errorcodeptr = ERR15;
        break;
        }
      s = cb->bracount - (s - 1);
      }

    escape = -(int)s;
    break;

    /* The handling of escape sequences consisting of a string of digits
    starting with one that is not zero is not straightforward. Perl has changed
    over the years. Nowadays \g{} for backreferences and \o{} for octal are
    recommended to avoid the ambiguities in the old syntax.

    Outside a character class, the digits are read as a decimal number. If the
    number is less than 10, or if there are that many previous extracting left
    brackets, it is a back reference. Otherwise, up to three octal digits are
    read to form an escaped character code. Thus \123 is likely to be octal 123
    (cf \0123, which is octal 012 followed by the literal 3).

    Inside a character class, \ followed by a digit is always either a literal
    8 or 9 or an octal number. */

    case CHAR_1: case CHAR_2: case CHAR_3: case CHAR_4: case CHAR_5:
    case CHAR_6: case CHAR_7: case CHAR_8: case CHAR_9:

    if (!isclass)
      {
      oldptr = ptr;
      /* The integer range is limited by the machine's int representation. */
      s = c - CHAR_0;
      overflow = FALSE;
      while (IS_DIGIT(ptr[1]))
        {
        if (s > INT_MAX / 10 - 1) /* Integer overflow */
          {
          overflow = TRUE;
          break;
          }
        s = s * 10 + (unsigned int)(*(++ptr) - CHAR_0);
        }
      if (overflow) /* Integer overflow */
        {
        while (IS_DIGIT(ptr[1])) ptr++;
        *errorcodeptr = ERR61;
        break;
        }

      /* \1 to \9 are always back references. \8x and \9x are too; \1x to \7x
      are octal escapes if there are not that many previous captures. */

      if (s < 10 || *oldptr >= CHAR_8 || s <= cb->bracount)
        {
        escape = -(int)s;     /* Indicates a back reference */
        break;
        }
      ptr = oldptr;      /* Put the pointer back and fall through */
      }

    /* Handle a digit following \ when the number is not a back reference, or
    we are within a character class. If the first digit is 8 or 9, Perl used to
    generate a binary zero byte and then treat the digit as a following
    literal. At least by Perl 5.18 this changed so as not to insert the binary
    zero. */

    if ((c = *ptr) >= CHAR_8) break;

    /* Fall through with a digit less than 8 */

    /* \0 always starts an octal number, but we may drop through to here with a
    larger first octal digit. The original code used just to take the least
    significant 8 bits of octal numbers (I think this is what early Perls used
    to do). Nowadays we allow for larger numbers in UTF-8 mode and 16-bit mode,
    but no more than 3 octal digits. */

    case CHAR_0:
    c -= CHAR_0;
    while(i++ < 2 && ptr[1] >= CHAR_0 && ptr[1] <= CHAR_7)
        c = c * 8 + *(++ptr) - CHAR_0;
#if PCRE2_CODE_UNIT_WIDTH == 8
    if (!utf && c > 0xff) *errorcodeptr = ERR51;
#endif
    break;

    /* \o is a relatively new Perl feature, supporting a more general way of
    specifying character codes in octal. The only supported form is \o{ddd}. */

    case CHAR_o:
    if (ptr[1] != CHAR_LEFT_CURLY_BRACKET) *errorcodeptr = ERR55; else
    if (ptr[2] == CHAR_RIGHT_CURLY_BRACKET) *errorcodeptr = ERR78; else
      {
      ptr += 2;
      c = 0;
      overflow = FALSE;
      while (*ptr >= CHAR_0 && *ptr <= CHAR_7)
        {
        cc = *ptr++;
        if (c == 0 && cc == CHAR_0) continue;     /* Leading zeroes */
#if PCRE2_CODE_UNIT_WIDTH == 32
        if (c >= 0x20000000l) { overflow = TRUE; break; }
#endif
        c = (c << 3) + (cc - CHAR_0);
#if PCRE2_CODE_UNIT_WIDTH == 8
        if (c > (utf ? 0x10ffffU : 0xffU)) { overflow = TRUE; break; }
#elif PCRE2_CODE_UNIT_WIDTH == 16
        if (c > (utf ? 0x10ffffU : 0xffffU)) { overflow = TRUE; break; }
#elif PCRE2_CODE_UNIT_WIDTH == 32
        if (utf && c > 0x10ffffU) { overflow = TRUE; break; }
#endif
        }
      if (overflow)
        {
        while (*ptr >= CHAR_0 && *ptr <= CHAR_7) ptr++;
        *errorcodeptr = ERR34;
        }
      else if (*ptr == CHAR_RIGHT_CURLY_BRACKET)
        {
        if (utf && c >= 0xd800 && c <= 0xdfff) *errorcodeptr = ERR73;
        }
      else *errorcodeptr = ERR64;
      }
    break;

    /* \x is complicated. When PCRE2_ALT_BSUX is set, \x must be followed by
    two hexadecimal digits. Otherwise it is a lowercase x letter. */

    case CHAR_x:
    if ((options & PCRE2_ALT_BSUX) != 0)
      {
      uint32_t xc;
      if ((cc = XDIGIT(ptr[1])) == 0xff) break;  /* Not a hex digit */
      if ((xc = XDIGIT(ptr[2])) == 0xff) break;  /* Not a hex digit */
      c = (cc << 4) | xc;
      ptr += 2;
      }    /* End PCRE2_ALT_BSUX handling */

    /* Handle \x in Perl's style. \x{ddd} is a character number which can be
    greater than 0xff in UTF-8 or non-8bit mode, but only if the ddd are hex
    digits. If not, { used to be treated as a data character. However, Perl
    seems to read hex digits up to the first non-such, and ignore the rest, so
    that, for example \x{zz} matches a binary zero. This seems crazy, so PCRE
    now gives an error. */

    else
      {
      if (ptr[1] == CHAR_LEFT_CURLY_BRACKET)
        {
        ptr += 2;
        if (*ptr == CHAR_RIGHT_CURLY_BRACKET)
          {
          *errorcodeptr = ERR78;
          break;
          }
        c = 0;
        overflow = FALSE;

        while ((cc = XDIGIT(*ptr)) != 0xff)
          {
          ptr++;
          if (c == 0 && cc == 0) continue;   /* Leading zeroes */
#if PCRE2_CODE_UNIT_WIDTH == 32
          if (c >= 0x10000000l) { overflow = TRUE; break; }
#endif
          c = (c << 4) | cc;
          if ((utf && c > 0x10ffffU) || (!utf && c > MAX_NON_UTF_CHAR))
            {
            overflow = TRUE;
            break;
            }
          }

        if (overflow)
          {
          while (XDIGIT(*ptr) != 0xff) ptr++;
          *errorcodeptr = ERR34;
          }
        else if (*ptr == CHAR_RIGHT_CURLY_BRACKET)
          {
          if (utf && c >= 0xd800 && c <= 0xdfff) *errorcodeptr = ERR73;
          }

        /* If the sequence of hex digits does not end with '}', give an error.
        We used just to recognize this construct and fall through to the normal
        \x handling, but nowadays Perl gives an error, which seems much more
        sensible, so we do too. */

        else *errorcodeptr = ERR67;
        }   /* End of \x{} processing */

      /* Read a single-byte hex-defined char (up to two hex digits after \x) */

      else
        {
        c = 0;
        if ((cc = XDIGIT(ptr[1])) == 0xff) break;  /* Not a hex digit */
        ptr++;
        c = cc;
        if ((cc = XDIGIT(ptr[1])) == 0xff) break;  /* Not a hex digit */
        ptr++;
        c = (c << 4) | cc;
        }     /* End of \xdd handling */
      }       /* End of Perl-style \x handling */
    break;

    /* The handling of \c is different in ASCII and EBCDIC environments. In an
    ASCII (or Unicode) environment, an error is given if the character
    following \c is not a printable ASCII character. Otherwise, the following
    character is upper-cased if it is a letter, and after that the 0x40 bit is
    flipped. The result is the value of the escape.

    In an EBCDIC environment the handling of \c is compatible with the
    specification in the perlebcdic document. The following character must be
    a letter or one of small number of special characters. These provide a
    means of defining the character values 0-31.

    For testing the EBCDIC handling of \c in an ASCII environment, recognize
    the EBCDIC value of 'c' explicitly. */

#if defined EBCDIC && 'a' != 0x81
    case 0x83:
#else
    case CHAR_c:
#endif

    c = *(++ptr);
    if (c >= CHAR_a && c <= CHAR_z) c = UPPER_CASE(c);
    if (c == CHAR_NULL && ptr >= ptrend)
      {
      *errorcodeptr = ERR2;
      break;
      }

    /* Handle \c in an ASCII/Unicode environment. */

#ifndef EBCDIC    /* ASCII/UTF-8 coding */
    if (c < 32 || c > 126)  /* Excludes all non-printable ASCII */
      {
      *errorcodeptr = ERR68;
      break;
      }
    c ^= 0x40;

    /* Handle \c in an EBCDIC environment. The special case \c? is converted to
    255 (0xff) or 95 (0x5f) if other character suggest we are using th POSIX-BC
    encoding. (This is the way Perl indicates that it handles \c?.) The other
    valid sequences correspond to a list of specific characters. */

#else
    if (c == CHAR_QUESTION_MARK)
      c = ('\\' == 188 && '`' == 74)? 0x5f : 0xff;
    else
      {
      for (i = 0; i < 32; i++)
        {
        if (c == ebcdic_escape_c[i]) break;
        }
      if (i < 32) c = i; else *errorcodeptr = ERR68;
      }
#endif  /* EBCDIC */

    break;

    /* Any other alphanumeric following \ is an error. Perl gives an error only
    if in warning mode, but PCRE doesn't have a warning mode. */

    default:
    *errorcodeptr = ERR3;
    break;
    }
  }

/* Perl supports \N{name} for character names, as well as plain \N for "not
newline". PCRE does not support \N{name}. However, it does support
quantification such as \N{2,3}. */

if (escape == ESC_N && ptr[1] == CHAR_LEFT_CURLY_BRACKET &&
     !is_counted_repeat(ptr+2))
  *errorcodeptr = ERR37;

/* If PCRE2_UCP is set, we change the values for \d etc. */

if ((options & PCRE2_UCP) != 0 && escape >= ESC_D && escape <= ESC_w)
  escape += (ESC_DU - ESC_D);

/* Set the pointer to the final character before returning. */

*ptrptr = ptr;
*chptr = c;
return escape;
}



#ifdef SUPPORT_UNICODE
/*************************************************
*               Handle \P and \p                 *
*************************************************/

/* This function is called after \P or \p has been encountered, provided that
PCRE2 is compiled with support for UTF and Unicode properties. On entry, the
contents of ptrptr are pointing at the P or p. On exit, it is left pointing at
the final code unit of the escape sequence.

Arguments:
  ptrptr         the pattern position pointer
  negptr         a boolean that is set TRUE for negation else FALSE
  ptypeptr       an unsigned int that is set to the type value
  pdataptr       an unsigned int that is set to the detailed property value
  errorcodeptr   the error code variable
  cb             the compile data

Returns:         TRUE if the type value was found, or FALSE for an invalid type
*/

static BOOL
get_ucp(PCRE2_SPTR *ptrptr, BOOL *negptr, unsigned int *ptypeptr,
  unsigned int *pdataptr, int *errorcodeptr, compile_block *cb)
{
register PCRE2_UCHAR c;
size_t i, bot, top;
PCRE2_SPTR ptr = *ptrptr;
PCRE2_UCHAR name[32];

*negptr = FALSE;
c = *(++ptr);

/* \P or \p can be followed by a name in {}, optionally preceded by ^ for
negation. */

if (c == CHAR_LEFT_CURLY_BRACKET)
  {
  if (ptr[1] == CHAR_CIRCUMFLEX_ACCENT)
    {
    *negptr = TRUE;
    ptr++;
    }
  for (i = 0; i < (int)(sizeof(name) / sizeof(PCRE2_UCHAR)) - 1; i++)
    {
    c = *(++ptr);
    if (c == CHAR_NULL) goto ERROR_RETURN;
    if (c == CHAR_RIGHT_CURLY_BRACKET) break;
    name[i] = c;
    }
  if (c != CHAR_RIGHT_CURLY_BRACKET) goto ERROR_RETURN;
  name[i] = 0;
  }

/* Otherwise there is just one following character, which must be an ASCII
letter. */

else if (MAX_255(c) && (cb->ctypes[c] & ctype_letter) != 0)
  {
  name[0] = c;
  name[1] = 0;
  }
else goto ERROR_RETURN;

*ptrptr = ptr;

/* Search for a recognized property name using binary chop. */

bot = 0;
top = PRIV(utt_size);

while (bot < top)
  {
  int r;
  i = (bot + top) >> 1;
  r = PRIV(strcmp_c8)(name, PRIV(utt_names) + PRIV(utt)[i].name_offset);
  if (r == 0)
    {
    *ptypeptr = PRIV(utt)[i].type;
    *pdataptr = PRIV(utt)[i].value;
    return TRUE;
    }
  if (r > 0) bot = i + 1; else top = i;
  }
*errorcodeptr = ERR47;   /* Unrecognized name */
return FALSE;

ERROR_RETURN:            /* Malformed \P or \p */
*errorcodeptr = ERR46;
*ptrptr = ptr;
return FALSE;
}
#endif



/*************************************************
*         Read repeat counts                     *
*************************************************/

/* Read an item of the form {n,m} and return the values. This is called only
after is_counted_repeat() has confirmed that a repeat-count quantifier exists,
so the syntax is guaranteed to be correct, but we need to check the values.

Arguments:
  p              pointer to first char after '{'
  minp           pointer to int for min
  maxp           pointer to int for max
                 returned as -1 if no max
  errorcodeptr   points to error code variable

Returns:         pointer to '}' on success;
                 current ptr on error, with errorcodeptr set non-zero
*/

static PCRE2_SPTR
read_repeat_counts(PCRE2_SPTR p, int *minp, int *maxp, int *errorcodeptr)
{
int min = 0;
int max = -1;

while (IS_DIGIT(*p))
  {
  min = min * 10 + (int)(*p++ - CHAR_0);
  if (min > 65535)
    {
    *errorcodeptr = ERR5;
    return p;
    }
  }

if (*p == CHAR_RIGHT_CURLY_BRACKET) max = min; else
  {
  if (*(++p) != CHAR_RIGHT_CURLY_BRACKET)
    {
    max = 0;
    while(IS_DIGIT(*p))
      {
      max = max * 10 + (int)(*p++ - CHAR_0);
      if (max > 65535)
        {
        *errorcodeptr = ERR5;
        return p;
        }
      }
    if (max < min)
      {
      *errorcodeptr = ERR4;
      return p;
      }
    }
  }

*minp = min;
*maxp = max;
return p;
}



/*************************************************
*   Scan compiled regex for recursion reference  *
*************************************************/

/* This function scans through a compiled pattern until it finds an instance of
OP_RECURSE.

Arguments:
  code        points to start of expression
  utf         TRUE in UTF mode

Returns:      pointer to the opcode for OP_RECURSE, or NULL if not found
*/

static PCRE2_SPTR
find_recurse(PCRE2_SPTR code, BOOL utf)
{
for (;;)
  {
  register PCRE2_UCHAR c = *code;
  if (c == OP_END) return NULL;
  if (c == OP_RECURSE) return code;

  /* XCLASS is used for classes that cannot be represented just by a bit map.
  This includes negated single high-valued characters. CALLOUT_STR is used for
  callouts with string arguments. In both cases the length in the table is
  zero; the actual length is stored in the compiled code. */

  if (c == OP_XCLASS) code += GET(code, 1);
    else if (c == OP_CALLOUT_STR) code += GET(code, 1 + 2*LINK_SIZE);

  /* Otherwise, we can get the item's length from the table, except that for
  repeated character types, we have to test for \p and \P, which have an extra
  two bytes of parameters, and for MARK/PRUNE/SKIP/THEN with an argument, we
  must add in its length. */

  else
    {
    switch(c)
      {
      case OP_TYPESTAR:
      case OP_TYPEMINSTAR:
      case OP_TYPEPLUS:
      case OP_TYPEMINPLUS:
      case OP_TYPEQUERY:
      case OP_TYPEMINQUERY:
      case OP_TYPEPOSSTAR:
      case OP_TYPEPOSPLUS:
      case OP_TYPEPOSQUERY:
      if (code[1] == OP_PROP || code[1] == OP_NOTPROP) code += 2;
      break;

      case OP_TYPEPOSUPTO:
      case OP_TYPEUPTO:
      case OP_TYPEMINUPTO:
      case OP_TYPEEXACT:
      if (code[1 + IMM2_SIZE] == OP_PROP || code[1 + IMM2_SIZE] == OP_NOTPROP)
        code += 2;
      break;

      case OP_MARK:
      case OP_PRUNE_ARG:
      case OP_SKIP_ARG:
      case OP_THEN_ARG:
      code += code[1];
      break;
      }

    /* Add in the fixed length from the table */

    code += PRIV(OP_lengths)[c];

    /* In UTF-8 and UTF-16 modes, opcodes that are followed by a character may
    be followed by a multi-unit character. The length in the table is a
    minimum, so we have to arrange to skip the extra units. */

#ifdef MAYBE_UTF_MULTI
    if (utf) switch(c)
      {
      case OP_CHAR:
      case OP_CHARI:
      case OP_NOT:
      case OP_NOTI:
      case OP_EXACT:
      case OP_EXACTI:
      case OP_NOTEXACT:
      case OP_NOTEXACTI:
      case OP_UPTO:
      case OP_UPTOI:
      case OP_NOTUPTO:
      case OP_NOTUPTOI:
      case OP_MINUPTO:
      case OP_MINUPTOI:
      case OP_NOTMINUPTO:
      case OP_NOTMINUPTOI:
      case OP_POSUPTO:
      case OP_POSUPTOI:
      case OP_NOTPOSUPTO:
      case OP_NOTPOSUPTOI:
      case OP_STAR:
      case OP_STARI:
      case OP_NOTSTAR:
      case OP_NOTSTARI:
      case OP_MINSTAR:
      case OP_MINSTARI:
      case OP_NOTMINSTAR:
      case OP_NOTMINSTARI:
      case OP_POSSTAR:
      case OP_POSSTARI:
      case OP_NOTPOSSTAR:
      case OP_NOTPOSSTARI:
      case OP_PLUS:
      case OP_PLUSI:
      case OP_NOTPLUS:
      case OP_NOTPLUSI:
      case OP_MINPLUS:
      case OP_MINPLUSI:
      case OP_NOTMINPLUS:
      case OP_NOTMINPLUSI:
      case OP_POSPLUS:
      case OP_POSPLUSI:
      case OP_NOTPOSPLUS:
      case OP_NOTPOSPLUSI:
      case OP_QUERY:
      case OP_QUERYI:
      case OP_NOTQUERY:
      case OP_NOTQUERYI:
      case OP_MINQUERY:
      case OP_MINQUERYI:
      case OP_NOTMINQUERY:
      case OP_NOTMINQUERYI:
      case OP_POSQUERY:
      case OP_POSQUERYI:
      case OP_NOTPOSQUERY:
      case OP_NOTPOSQUERYI:
      if (HAS_EXTRALEN(code[-1])) code += GET_EXTRALEN(code[-1]);
      break;
      }
#else
    (void)(utf);  /* Keep compiler happy by referencing function argument */
#endif  /* MAYBE_UTF_MULTI */
    }
  }
}



/*************************************************
*           Check for POSIX class syntax         *
*************************************************/

/* This function is called when the sequence "[:" or "[." or "[=" is
encountered in a character class. It checks whether this is followed by a
sequence of characters terminated by a matching ":]" or ".]" or "=]". If we
reach an unescaped ']' without the special preceding character, return FALSE.

Originally, this function only recognized a sequence of letters between the
terminators, but it seems that Perl recognizes any sequence of characters,
though of course unknown POSIX names are subsequently rejected. Perl gives an
"Unknown POSIX class" error for [:f\oo:] for example, where previously PCRE
didn't consider this to be a POSIX class. Likewise for [:1234:].

The problem in trying to be exactly like Perl is in the handling of escapes. We
have to be sure that [abc[:x\]pqr] is *not* treated as containing a POSIX
class, but [abc[:x\]pqr:]] is (so that an error can be generated). The code
below handles the special cases \\ and \], but does not try to do any other
escape processing. This makes it different from Perl for cases such as
[:l\ower:] where Perl recognizes it as the POSIX class "lower" but PCRE does
not recognize "l\ower". This is a lesser evil than not diagnosing bad classes
when Perl does, I think.

A user pointed out that PCRE was rejecting [:a[:digit:]] whereas Perl was not.
It seems that the appearance of a nested POSIX class supersedes an apparent
external class. For example, [:a[:digit:]b:] matches "a", "b", ":", or
a digit. This is handled by returning FALSE if the start of a new group with
the same terminator is encountered, since the next closing sequence must close
the nested group, not the outer one.

In Perl, unescaped square brackets may also appear as part of class names. For
example, [:a[:abc]b:] gives unknown POSIX class "[:abc]b:]". However, for
[:a[:abc]b][b:] it gives unknown POSIX class "[:abc]b][b:]", which does not
seem right at all. PCRE does not allow closing square brackets in POSIX class
names.

Arguments:
  ptr      pointer to the initial [
  endptr   where to return a pointer to the terminating ':', '.', or '='

Returns:   TRUE or FALSE
*/

static BOOL
check_posix_syntax(PCRE2_SPTR ptr, PCRE2_SPTR *endptr)
{
PCRE2_UCHAR terminator;  /* Don't combine these lines; the Solaris cc */
terminator = *(++ptr);   /* compiler warns about "non-constant" initializer. */

for (++ptr; *ptr != CHAR_NULL; ptr++)
  {
  if (*ptr == CHAR_BACKSLASH &&
      (ptr[1] == CHAR_RIGHT_SQUARE_BRACKET || ptr[1] == CHAR_BACKSLASH))
    ptr++;
  else if ((*ptr == CHAR_LEFT_SQUARE_BRACKET && ptr[1] == terminator) ||
            *ptr == CHAR_RIGHT_SQUARE_BRACKET) return FALSE;
  else if (*ptr == terminator && ptr[1] == CHAR_RIGHT_SQUARE_BRACKET)
    {
    *endptr = ptr;
    return TRUE;
    }
  }

return FALSE;
}



/*************************************************
*          Check POSIX class name                *
*************************************************/

/* This function is called to check the name given in a POSIX-style class entry
such as [:alnum:].

Arguments:
  ptr        points to the first letter
  len        the length of the name

Returns:     a value representing the name, or -1 if unknown
*/

static int
check_posix_name(PCRE2_SPTR ptr, int len)
{
const char *pn = posix_names;
register int yield = 0;
while (posix_name_lengths[yield] != 0)
  {
  if (len == posix_name_lengths[yield] &&
    PRIV(strncmp_c8)(ptr, pn, (unsigned int)len) == 0) return yield;
  pn += posix_name_lengths[yield] + 1;
  yield++;
  }
return -1;
}



#ifdef SUPPORT_UNICODE
/*************************************************
*           Get othercase range                  *
*************************************************/

/* This function is passed the start and end of a class range in UCT mode. It
searches up the characters, looking for ranges of characters in the "other"
case. Each call returns the next one, updating the start address. A character
with multiple other cases is returned on its own with a special return value.

Arguments:
  cptr        points to starting character value; updated
  d           end value
  ocptr       where to put start of othercase range
  odptr       where to put end of othercase range

Yield:        -1 when no more
               0 when a range is returned
              >0 the CASESET offset for char with multiple other cases
                in this case, ocptr contains the original
*/

static int
get_othercase_range(uint32_t *cptr, uint32_t d, uint32_t *ocptr,
  uint32_t *odptr)
{
uint32_t c, othercase, next;
unsigned int co;

/* Find the first character that has an other case. If it has multiple other
cases, return its case offset value. */

for (c = *cptr; c <= d; c++)
  {
  if ((co = UCD_CASESET(c)) != 0)
    {
    *ocptr = c++;   /* Character that has the set */
    *cptr = c;      /* Rest of input range */
    return (int)co;
    }
  if ((othercase = UCD_OTHERCASE(c)) != c) break;
  }

if (c > d) return -1;  /* Reached end of range */

/* Found a character that has a single other case. Search for the end of the
range, which is either the end of the input range, or a character that has zero
or more than one other cases. */

*ocptr = othercase;
next = othercase + 1;

for (++c; c <= d; c++)
  {
  if ((co = UCD_CASESET(c)) != 0 || UCD_OTHERCASE(c) != next) break;
  next++;
  }

*odptr = next - 1;     /* End of othercase range */
*cptr = c;             /* Rest of input range */
return 0;
}
#endif  /* SUPPORT_UNICODE */



/*************************************************
*        Add a character or range to a class     *
*************************************************/

/* This function packages up the logic of adding a character or range of
characters to a class. The character values in the arguments will be within the
valid values for the current mode (8-bit, 16-bit, UTF, etc). This function is
mutually recursive with the function immediately below.

Arguments:
  classbits     the bit map for characters < 256
  uchardptr     points to the pointer for extra data
  options       the options word
  cb            compile data
  start         start of range character
  end           end of range character

Returns:        the number of < 256 characters added
                the pointer to extra data is updated
*/

static unsigned int
add_to_class(uint8_t *classbits, PCRE2_UCHAR **uchardptr, uint32_t options,
  compile_block *cb, uint32_t start, uint32_t end)
{
uint32_t c;
uint32_t classbits_end = (end <= 0xff ? end : 0xff);
unsigned int n8 = 0;

/* If caseless matching is required, scan the range and process alternate
cases. In Unicode, there are 8-bit characters that have alternate cases that
are greater than 255 and vice-versa. Sometimes we can just extend the original
range. */

if ((options & PCRE2_CASELESS) != 0)
  {
#ifdef SUPPORT_UNICODE
  if ((options & PCRE2_UTF) != 0)
    {
    int rc;
    uint32_t oc, od;

    options &= ~PCRE2_CASELESS;   /* Remove for recursive calls */
    c = start;

    while ((rc = get_othercase_range(&c, end, &oc, &od)) >= 0)
      {
      /* Handle a single character that has more than one other case. */

      if (rc > 0) n8 += add_list_to_class(classbits, uchardptr, options, cb,
        PRIV(ucd_caseless_sets) + rc, oc);

      /* Do nothing if the other case range is within the original range. */

      else if (oc >= start && od <= end) continue;

      /* Extend the original range if there is overlap, noting that if oc < c, we
      can't have od > end because a subrange is always shorter than the basic
      range. Otherwise, use a recursive call to add the additional range. */

      else if (oc < start && od >= start - 1) start = oc; /* Extend downwards */
      else if (od > end && oc <= end + 1)
        {
        end = od;       /* Extend upwards */
        if (end > classbits_end) classbits_end = (end <= 0xff ? end : 0xff);
        }
      else n8 += add_to_class(classbits, uchardptr, options, cb, oc, od);
      }
    }
  else
#endif  /* SUPPORT_UNICODE */

  /* Not UTF mode */

  for (c = start; c <= classbits_end; c++)
    {
    SETBIT(classbits, cb->fcc[c]);
    n8++;
    }
  }

/* Now handle the original range. Adjust the final value according to the bit
length - this means that the same lists of (e.g.) horizontal spaces can be used
in all cases. */

if ((options & PCRE2_UTF) == 0 && end > MAX_NON_UTF_CHAR)
  end = MAX_NON_UTF_CHAR;

/* Use the bitmap for characters < 256. Otherwise use extra data.*/

for (c = start; c <= classbits_end; c++)
  {
  /* Regardless of start, c will always be <= 255. */
  SETBIT(classbits, c);
  n8++;
  }

#ifdef SUPPORT_WIDE_CHARS
if (start <= 0xff) start = 0xff + 1;

if (end >= start)
  {
  PCRE2_UCHAR *uchardata = *uchardptr;

#ifdef SUPPORT_UNICODE
  if ((options & PCRE2_UTF) != 0)
    {
    if (start < end)
      {
      *uchardata++ = XCL_RANGE;
      uchardata += PRIV(ord2utf)(start, uchardata);
      uchardata += PRIV(ord2utf)(end, uchardata);
      }
    else if (start == end)
      {
      *uchardata++ = XCL_SINGLE;
      uchardata += PRIV(ord2utf)(start, uchardata);
      }
    }
  else
#endif  /* SUPPORT_UNICODE */

  /* Without UTF support, character values are constrained by the bit length,
  and can only be > 256 for 16-bit and 32-bit libraries. */

#if PCRE2_CODE_UNIT_WIDTH == 8
    {}
#else
  if (start < end)
    {
    *uchardata++ = XCL_RANGE;
    *uchardata++ = start;
    *uchardata++ = end;
    }
  else if (start == end)
    {
    *uchardata++ = XCL_SINGLE;
    *uchardata++ = start;
    }
#endif
  *uchardptr = uchardata;   /* Updata extra data pointer */
  }
#else
  (void)uchardptr;          /* Avoid compiler warning */
#endif /* SUPPORT_WIDE_CHARS */

return n8;    /* Number of 8-bit characters */
}



/*************************************************
*        Add a list of characters to a class     *
*************************************************/

/* This function is used for adding a list of case-equivalent characters to a
class, and also for adding a list of horizontal or vertical whitespace. If the
list is in order (which it should be), ranges of characters are detected and
handled appropriately. This function is mutually recursive with the function
above.

Arguments:
  classbits     the bit map for characters < 256
  uchardptr     points to the pointer for extra data
  options       the options word
  cb            contains pointers to tables etc.
  p             points to row of 32-bit values, terminated by NOTACHAR
  except        character to omit; this is used when adding lists of
                  case-equivalent characters to avoid including the one we
                  already know about

Returns:        the number of < 256 characters added
                the pointer to extra data is updated
*/

static unsigned int
add_list_to_class(uint8_t *classbits, PCRE2_UCHAR **uchardptr, uint32_t options,
  compile_block *cb, const uint32_t *p, unsigned int except)
{
unsigned int n8 = 0;
while (p[0] < NOTACHAR)
  {
  unsigned int n = 0;
  if (p[0] != except)
    {
    while(p[n+1] == p[0] + n + 1) n++;
    n8 += add_to_class(classbits, uchardptr, options, cb, p[0], p[n]);
    }
  p += n + 1;
  }
return n8;
}



/*************************************************
*    Add characters not in a list to a class     *
*************************************************/

/* This function is used for adding the complement of a list of horizontal or
vertical whitespace to a class. The list must be in order.

Arguments:
  classbits     the bit map for characters < 256
  uchardptr     points to the pointer for extra data
  options       the options word
  cb            contains pointers to tables etc.
  p             points to row of 32-bit values, terminated by NOTACHAR

Returns:        the number of < 256 characters added
                the pointer to extra data is updated
*/

static unsigned int
add_not_list_to_class(uint8_t *classbits, PCRE2_UCHAR **uchardptr,
  uint32_t options, compile_block *cb, const uint32_t *p)
{
BOOL utf = (options & PCRE2_UTF) != 0;
unsigned int n8 = 0;
if (p[0] > 0)
  n8 += add_to_class(classbits, uchardptr, options, cb, 0, p[0] - 1);
while (p[0] < NOTACHAR)
  {
  while (p[1] == p[0] + 1) p++;
  n8 += add_to_class(classbits, uchardptr, options, cb, p[0] + 1,
    (p[1] == NOTACHAR) ? (utf ? 0x10ffffu : 0xffffffffu) : p[1] - 1);
  p++;
  }
return n8;
}



/*************************************************
*       Process (*VERB) name for escapes         *
*************************************************/

/* This function is called when the PCRE2_ALT_VERBNAMES option is set, to
process the characters in a verb's name argument. It is called twice, once with
codeptr == NULL, to find out the length of the processed name, and again to put
the name into memory.

Arguments:
  ptrptr        pointer to the input pointer
  codeptr       pointer to the compiled code pointer
  errorcodeptr  pointer to the error code
  options       the options bits
  utf           TRUE if processing UTF
  cb            compile data block

Returns:        length of the processed name, or < 0 on error
*/

static int
process_verb_name(PCRE2_SPTR *ptrptr, PCRE2_UCHAR **codeptr, int *errorcodeptr,
  uint32_t options, BOOL utf, compile_block *cb)
{
int32_t arglen = 0;
BOOL inescq = FALSE;
PCRE2_SPTR ptr = *ptrptr;
PCRE2_UCHAR *code = (codeptr == NULL)? NULL : *codeptr;

for (; ptr < cb->end_pattern; ptr++)
  {
  uint32_t x = *ptr;

  /* Skip over literals */

  if (inescq)
    {
    if (x == CHAR_BACKSLASH && ptr[1] == CHAR_E)
      {
      inescq = FALSE;
      ptr++;;
      continue;
      }
    }

  else  /* Not a literal character */
    {
    if (x == CHAR_RIGHT_PARENTHESIS) break;

    /* Skip over comments and whitespace in extended mode. */

    if ((options & PCRE2_EXTENDED) != 0)
      {
      PCRE2_SPTR wscptr = ptr;
      while (MAX_255(x) && (cb->ctypes[x] & ctype_space) != 0) x = *(++ptr);
      if (x == CHAR_NUMBER_SIGN)
        {
        ptr++;
        while (*ptr != CHAR_NULL || ptr < cb->end_pattern)
          {
          if (IS_NEWLINE(ptr))       /* For non-fixed-length newline cases, */
            {                        /* IS_NEWLINE sets cb->nllen. */
            ptr += cb->nllen;
            break;
            }
          ptr++;
#ifdef SUPPORT_UNICODE
          if (utf) FORWARDCHAR(ptr);
#endif
          }
        }

      /* If we have skipped any characters, restart the loop. */

      if (ptr > wscptr)
        {
        ptr--;
        continue;
        }
      }

    /* Process escapes */

    if (x == '\\')
      {
      int rc;
      *errorcodeptr = 0;
      rc = PRIV(check_escape)(&ptr, cb->end_pattern, &x, errorcodeptr, options,
        FALSE, cb);
      *ptrptr = ptr;   /* For possible error */
      if (*errorcodeptr != 0) return -1;
      if (rc != 0)
        {
        if (rc == ESC_Q)
          {
          inescq = TRUE;
          continue;
          }
        if (rc == ESC_E) continue;
        *errorcodeptr = ERR40;
        return -1;
        }
      }
    }

  /* We have the next character in the name. */

#ifdef SUPPORT_UNICODE
  if (utf)
    {
    if (code == NULL)   /* Just want the length */
      {
#if PCRE2_CODE_UNIT_WIDTH == 8
      int i;
      for (i = 0; i < PRIV(utf8_table1_size); i++)
        if ((int)x <= PRIV(utf8_table1)[i]) break;
      arglen += i;
#elif PCRE2_CODE_UNIT_WIDTH == 16
      if (x > 0xffff) arglen++;
#endif
      }
    else
      {
      PCRE2_UCHAR cbuff[8];
      x = PRIV(ord2utf)(x, cbuff);
      memcpy(code, cbuff, CU2BYTES(x));
      code += x;
      }
    }
  else
#endif  /* SUPPORT_UNICODE */

  /* Not UTF */
    {
    if (code != NULL) *code++ = (PCRE2_UCHAR)x;
    }

  arglen++;

  if ((unsigned int)arglen > MAX_MARK)
    {
    *errorcodeptr = ERR76;
    *ptrptr = ptr;
    return -1;
    }
  }

/* Update the pointers before returning. */

*ptrptr = ptr;
if (codeptr != NULL) *codeptr = code;
return arglen;
}



/*************************************************
*          Macro for the next two functions      *
*************************************************/

/* Both scan_for_captures() and compile_branch() use this macro to generate a
fragment of code that reads the characters of a name and sets its length
(checking for not being too long). Count the characters dynamically, to avoid
the possibility of integer overflow. The same macro is used for reading *VERB
names. */

#define READ_NAME(ctype, errno, errset)                      \
  namelen = 0;                                               \
  while (MAX_255(*ptr) && (cb->ctypes[*ptr] & ctype) != 0)   \
    {                                                        \
    ptr++;                                                   \
    namelen++;                                               \
    if (namelen > MAX_NAME_SIZE)                             \
      {                                                      \
      errset = errno;                                        \
      goto FAILED;                                           \
      }                                                      \
    }



/*************************************************
*      Scan regex to identify named groups       *
*************************************************/

/* This function is called first of all, to scan for named capturing groups so
that information about them is fully available to both the compiling scans.
It skips over everything except parenthesized items.

Arguments:
  ptrptr   points to pointer to the start of the pattern
  options  compiling dynamic options
  cb       pointer to the compile data block

Returns:   zero on success or a non-zero error code, with pointer updated
*/

typedef struct nest_save {
  uint16_t  nest_depth;
  uint16_t  reset_group;
  uint16_t  max_group;
  uint16_t  flags;
} nest_save;

#define NSF_RESET    0x0001u
#define NSF_EXTENDED 0x0002u
#define NSF_DUPNAMES 0x0004u

static int scan_for_captures(PCRE2_SPTR *ptrptr, uint32_t options,
  compile_block *cb)
{
uint32_t c;
uint32_t delimiter;
uint32_t set, unset, *optset;
uint32_t skiptoket = 0;
uint16_t nest_depth = 0;
int errorcode = 0;
int escape;
int namelen;
int i;
BOOL inescq = FALSE;
BOOL isdupname;
BOOL utf = (options & PCRE2_UTF) != 0;
BOOL negate_class;
PCRE2_SPTR name;
PCRE2_SPTR start;
PCRE2_SPTR ptr = *ptrptr;
named_group *ng;
nest_save *top_nest = NULL;
nest_save *end_nests = (nest_save *)(cb->start_workspace + cb->workspace_size);

/* The size of the nest_save structure might not be a factor of the size of the
workspace. Therefore we must round down end_nests so as to correctly avoid
creating a nest_save that spans the end of the workspace. */

end_nests = (nest_save *)((char *)end_nests -
  ((cb->workspace_size * sizeof(PCRE2_UCHAR)) % sizeof(nest_save)));

/* Now scan the pattern */

for (; ptr < cb->end_pattern; ptr++)
  {
  c = *ptr;

  /* Parenthesized groups set skiptoket when all following characters up to the
  next closing parenthesis must be ignored. The parenthesis itself must be
  processed (to end the nested parenthesized item). */

  if (skiptoket != 0)
    {
    if (c != CHAR_RIGHT_PARENTHESIS) continue;
    skiptoket = 0;
    }

  /* Skip over literals */

  if (inescq)
    {
    if (c == CHAR_BACKSLASH && ptr[1] == CHAR_E)
      {
      inescq = FALSE;
      ptr++;
      }
    continue;
    }

  /* Skip over # comments and whitespace in extended mode. */

  if ((options & PCRE2_EXTENDED) != 0)
    {
    PCRE2_SPTR wscptr = ptr;
    while (MAX_255(c) && (cb->ctypes[c] & ctype_space) != 0) c = *(++ptr);
    if (c == CHAR_NUMBER_SIGN)
      {
      ptr++;
      while (ptr < cb->end_pattern)
        {
        if (IS_NEWLINE(ptr))         /* For non-fixed-length newline cases, */
          {                          /* IS_NEWLINE sets cb->nllen. */
          ptr += cb->nllen;
          break;
          }
        ptr++;
#ifdef SUPPORT_UNICODE
        if (utf) FORWARDCHAR(ptr);
#endif
        }
      }

    /* If we skipped any characters, restart the loop. Otherwise, we didn't see
    a comment. */

    if (ptr > wscptr)
      {
      ptr--;
      continue;
      }
    }

  /* Process the next pattern item. */

  switch(c)
    {
    default:              /* Most characters are just skipped */
    break;

    /* Skip escapes except for \Q */

    case CHAR_BACKSLASH:
    errorcode = 0;
    escape = PRIV(check_escape)(&ptr, cb->end_pattern, &c, &errorcode, options,
      FALSE, cb);
    if (errorcode != 0) goto FAILED;
    if (escape == ESC_Q) inescq = TRUE;
    break;

    /* Skip a character class. The syntax is complicated so we have to
    replicate some of what happens when a class is processed for real. */

    case CHAR_LEFT_SQUARE_BRACKET:
    if (PRIV(strncmp_c8)(ptr+1, STRING_WEIRD_STARTWORD, 6) == 0 ||
        PRIV(strncmp_c8)(ptr+1, STRING_WEIRD_ENDWORD, 6) == 0)
      {
      ptr += 6;
      break;
      }

    /* If the first character is '^', set the negation flag (not actually used
    here, except to recognize only one ^) and skip it. If the first few
    characters (either before or after ^) are \Q\E or \E we skip them too. This
    makes for compatibility with Perl. */

    negate_class = FALSE;
    for (;;)
      {
      c = *(++ptr);   /* First character in class */
      if (c == CHAR_BACKSLASH)
        {
        if (ptr[1] == CHAR_E)
          ptr++;
        else if (PRIV(strncmp_c8)(ptr + 1, STR_Q STR_BACKSLASH STR_E, 3) == 0)
          ptr += 3;
        else
          break;
        }
      else if (!negate_class && c == CHAR_CIRCUMFLEX_ACCENT)
        negate_class = TRUE;
      else break;
      }

    if (c == CHAR_RIGHT_SQUARE_BRACKET &&
        (cb->external_options & PCRE2_ALLOW_EMPTY_CLASS) != 0)
      break;

    /* Loop for the contents of the class */

    for (;;)
      {
      PCRE2_SPTR tempptr;

      if (c == CHAR_NULL && ptr >= cb->end_pattern)
        {
        errorcode = ERR6;  /* Missing terminating ']' */
        goto FAILED;
        }

#ifdef SUPPORT_UNICODE
      if (utf && HAS_EXTRALEN(c))
        {                           /* Braces are required because the */
        GETCHARLEN(c, ptr, ptr);    /* macro generates multiple statements */
        }
#endif

      /* Inside \Q...\E everything is literal except \E */

      if (inescq)
        {
        if (c == CHAR_BACKSLASH && ptr[1] == CHAR_E)  /* If we are at \E */
          {
          inescq = FALSE;                   /* Reset literal state */
          ptr++;                            /* Skip the 'E' */
          }
        goto CONTINUE_CLASS;
        }

      /* Skip POSIX class names. */
      if (c == CHAR_LEFT_SQUARE_BRACKET &&
          (ptr[1] == CHAR_COLON || ptr[1] == CHAR_DOT ||
           ptr[1] == CHAR_EQUALS_SIGN) && check_posix_syntax(ptr, &tempptr))
        {
        ptr = tempptr + 1;
        }
      else if (c == CHAR_BACKSLASH)
        {
        errorcode = 0;
        escape = PRIV(check_escape)(&ptr, cb->end_pattern, &c, &errorcode,
          options, TRUE, cb);
        if (errorcode != 0) goto FAILED;
        if (escape == ESC_Q) inescq = TRUE;
        }

      CONTINUE_CLASS:
      c = *(++ptr);
      if (c == CHAR_RIGHT_SQUARE_BRACKET && !inescq) break;
      }     /* End of class-processing loop */
    break;

    /* This is the real work of this function - handling parentheses. */

    case CHAR_LEFT_PARENTHESIS:
    nest_depth++;

    if (ptr[1] != CHAR_QUESTION_MARK)
      {
      if (ptr[1] != CHAR_ASTERISK)
        {
        if ((options & PCRE2_NO_AUTO_CAPTURE) == 0) cb->bracount++;
        }

      /* (*something) - skip over a name, and then just skip to closing ket
      unless PCRE2_ALT_VERBNAMES is set, in which case we have to process
      escapes in the string after a verb name terminated by a colon. */

      else
        {
        ptr += 2;
        while (MAX_255(*ptr) && (cb->ctypes[*ptr] & ctype_word) != 0) ptr++;
        if (*ptr == CHAR_COLON && (options & PCRE2_ALT_VERBNAMES) != 0)
          {
          ptr++;
          if (process_verb_name(&ptr, NULL, &errorcode, options, utf, cb) < 0)
            goto FAILED;
          }
        else
          {
          while (ptr < cb->end_pattern && *ptr != CHAR_RIGHT_PARENTHESIS)
            ptr++;
          }
        nest_depth--;
        }
      }

    /* Handle (?...) groups */

    else switch(ptr[2])
      {
      default:
      ptr += 2;
      if (ptr[0] == CHAR_R ||                           /* (?R) */
          ptr[0] == CHAR_NUMBER_SIGN ||                 /* (?#) */
          IS_DIGIT(ptr[0]) ||                           /* (?n) */
          (ptr[0] == CHAR_MINUS && IS_DIGIT(ptr[1])))   /* (?-n) */
        {
        skiptoket = ptr[0];
        break;
        }

      /* Handle (?| and (?imsxJU: which are the only other valid forms. Both
      need a new block on the nest stack. */

      if (top_nest == NULL) top_nest = (nest_save *)(cb->start_workspace);
      else if (++top_nest >= end_nests)
        {
        errorcode = ERR84;
        goto FAILED;
        }
      top_nest->nest_depth = nest_depth;
      top_nest->flags = 0;
      if ((options & PCRE2_EXTENDED) != 0) top_nest->flags |= NSF_EXTENDED;
      if ((options & PCRE2_DUPNAMES) != 0) top_nest->flags |= NSF_DUPNAMES;

      if (*ptr == CHAR_VERTICAL_LINE)
        {
        top_nest->reset_group = (uint16_t)cb->bracount;
        top_nest->max_group = (uint16_t)cb->bracount;
        top_nest->flags |= NSF_RESET;
        cb->external_flags |= PCRE2_DUPCAPUSED;
        break;
        }

      /* Scan options */

      top_nest->reset_group = 0;
      top_nest->max_group = 0;

      set = unset = 0;
      optset = &set;

      /* Need only track (?x: and (?J: at this stage */

      while (*ptr != CHAR_RIGHT_PARENTHESIS && *ptr != CHAR_COLON)
        {
        switch (*ptr++)
          {
          case CHAR_MINUS: optset = &unset; break;

          case CHAR_x: *optset |= PCRE2_EXTENDED; break;

          case CHAR_J:
          *optset |= PCRE2_DUPNAMES;
          cb->external_flags |= PCRE2_JCHANGED;
          break;

          case CHAR_i:
          case CHAR_m:
          case CHAR_s:
          case CHAR_U:
          break;

          default:
          errorcode = ERR11;
          ptr--;    /* Correct the offset */
          goto FAILED;
          }
        }

      options = (options | set) & (~unset);

      /* If the options ended with ')' this is not the start of a nested
      group with option changes, so the options change at this level. If the
      previous level set up a nest block, discard the one we have just created.
      Otherwise adjust it for the previous level. */

      if (*ptr == CHAR_RIGHT_PARENTHESIS)
        {
        nest_depth--;
        if (top_nest > (nest_save *)(cb->start_workspace) &&
            (top_nest-1)->nest_depth == nest_depth) top_nest --;
        else top_nest->nest_depth = nest_depth;
        }
      break;

      /* Skip over a numerical or string argument for a callout. */

      case CHAR_C:
      ptr += 2;
      if (ptr[1] == CHAR_RIGHT_PARENTHESIS) break;
      if (IS_DIGIT(ptr[1]))
        {
        while (IS_DIGIT(ptr[1])) ptr++;
        }

      /* Handle a string argument */

      else
        {
        ptr++;
        delimiter = 0;
        for (i = 0; PRIV(callout_start_delims)[i] != 0; i++)
          {
          if (*ptr == PRIV(callout_start_delims)[i])
            {
            delimiter = PRIV(callout_end_delims)[i];
            break;
            }
          }

        if (delimiter == 0)
          {
          errorcode = ERR82;
          goto FAILED;
          }

        start = ptr;
        do
          {
          if (++ptr >= cb->end_pattern)
            {
            errorcode = ERR81;
            ptr = start;   /* To give a more useful message */
            goto FAILED;
            }
          if (ptr[0] == delimiter && ptr[1] == delimiter) ptr += 2;
          }
        while (ptr[0] != delimiter);
        }

      /* Check terminating ) */

      if (ptr[1] != CHAR_RIGHT_PARENTHESIS)
        {
        errorcode = ERR39;
        ptr++;
        goto FAILED;
        }
      break;

      /* Conditional group */

      case CHAR_LEFT_PARENTHESIS:
      if (ptr[3] != CHAR_QUESTION_MARK)   /* Not assertion or callout */
        {
        nest_depth++;
        ptr += 2;
        break;
        }

      /* Must be an assertion or a callout */

      switch(ptr[4])
       {
       case CHAR_LESS_THAN_SIGN:
       if (ptr[5] != CHAR_EXCLAMATION_MARK && ptr[5] != CHAR_EQUALS_SIGN)
         goto MISSING_ASSERTION;
       /* Fall through */

       case CHAR_C:
       case CHAR_EXCLAMATION_MARK:
       case CHAR_EQUALS_SIGN:
       ptr++;
       break;

       default:
       MISSING_ASSERTION:
       ptr += 3;            /* To improve error message */
       errorcode = ERR28;
       goto FAILED;
       }
      break;

      case CHAR_COLON:
      case CHAR_GREATER_THAN_SIGN:
      case CHAR_EQUALS_SIGN:
      case CHAR_EXCLAMATION_MARK:
      case CHAR_AMPERSAND:
      case CHAR_PLUS:
      ptr += 2;
      break;

      case CHAR_P:
      if (ptr[3] != CHAR_LESS_THAN_SIGN)
        {
        ptr += 3;
        break;
        }
      ptr++;
      c = CHAR_GREATER_THAN_SIGN;   /* Terminator */
      goto DEFINE_NAME;

      case CHAR_LESS_THAN_SIGN:
      if (ptr[3] == CHAR_EQUALS_SIGN || ptr[3] == CHAR_EXCLAMATION_MARK)
        {
        ptr += 3;
        break;
        }
      c = CHAR_GREATER_THAN_SIGN;   /* Terminator */
      goto DEFINE_NAME;

      case CHAR_APOSTROPHE:
      c = CHAR_APOSTROPHE;    /* Terminator */

      DEFINE_NAME:
      name = ptr = ptr + 3;

      if (*ptr == c)          /* Empty name */
        {
        errorcode = ERR62;
        goto FAILED;
        }

      if (IS_DIGIT(*ptr))
        {
        errorcode = ERR44;   /* Group name must start with non-digit */
        goto FAILED;
        }

      if (MAX_255(*ptr) && (cb->ctypes[*ptr] & ctype_word) == 0)
        {
        errorcode = ERR24;
        goto FAILED;
        }

      /* Advance ptr, set namelen and check its length. */
      READ_NAME(ctype_word, ERR48, errorcode);

      if (*ptr != c)
        {
        errorcode = ERR42;
        goto FAILED;
        }

      if (cb->names_found >= MAX_NAME_COUNT)
        {
        errorcode = ERR49;
        goto FAILED;
        }

      if (namelen + IMM2_SIZE + 1 > cb->name_entry_size)
        cb->name_entry_size = (uint16_t)(namelen + IMM2_SIZE + 1);

      /* We have a valid name for this capturing group. */

      cb->bracount++;

      /* Scan the list to check for duplicates. For duplicate names, if the
      number is the same, break the loop, which causes the name to be
      discarded; otherwise, if DUPNAMES is not set, give an error.
      If it is set, allow the name with a different number, but continue
      scanning in case this is a duplicate with the same number. For
      non-duplicate names, give an error if the number is duplicated. */

      isdupname = FALSE;
      ng = cb->named_groups;
      for (i = 0; i < cb->names_found; i++, ng++)
        {
        if (namelen == ng->length &&
            PRIV(strncmp)(name, ng->name, (size_t)namelen) == 0)
          {
          if (ng->number == cb->bracount) break;
          if ((options & PCRE2_DUPNAMES) == 0)
            {
            errorcode = ERR43;
            goto FAILED;
            }
          isdupname = ng->isdup = TRUE;     /* Mark as a duplicate */
          cb->dupnames = TRUE;              /* Duplicate names exist */
          }
        else if (ng->number == cb->bracount)
          {
          errorcode = ERR65;
          goto FAILED;
          }
        }

      if (i < cb->names_found) break;   /* Ignore duplicate with same number */

      /* Increase the list size if necessary */

      if (cb->names_found >= cb->named_group_list_size)
        {
        uint32_t newsize = cb->named_group_list_size * 2;
        named_group *newspace =
          cb->cx->memctl.malloc(newsize * sizeof(named_group),
          cb->cx->memctl.memory_data);
        if (newspace == NULL)
          {
          errorcode = ERR21;
          goto FAILED;
          }

        memcpy(newspace, cb->named_groups,
          cb->named_group_list_size * sizeof(named_group));
        if (cb->named_group_list_size > NAMED_GROUP_LIST_SIZE)
          cb->cx->memctl.free((void *)cb->named_groups,
          cb->cx->memctl.memory_data);
        cb->named_groups = newspace;
        cb->named_group_list_size = newsize;
        }

      /* Add this name to the list */

      cb->named_groups[cb->names_found].name = name;
      cb->named_groups[cb->names_found].length = (uint16_t)namelen;
      cb->named_groups[cb->names_found].number = cb->bracount;
      cb->named_groups[cb->names_found].isdup = (uint16_t)isdupname;
      cb->names_found++;
      break;
      }        /* End of (? switch */
    break;     /* End of ( handling */

    /* At an alternation, reset the capture count if we are in a (?| group. */

    case CHAR_VERTICAL_LINE:
    if (top_nest != NULL && top_nest->nest_depth == nest_depth &&
        (top_nest->flags & NSF_RESET) != 0)
      {
      if (cb->bracount > top_nest->max_group)
        top_nest->max_group = (uint16_t)cb->bracount;
      cb->bracount = top_nest->reset_group;
      }
    break;

    /* At a right parenthesis, reset the capture count to the maximum if we
    are in a (?| group and/or reset the extended option. */

    case CHAR_RIGHT_PARENTHESIS:
    if (top_nest != NULL && top_nest->nest_depth == nest_depth)
      {
      if ((top_nest->flags & NSF_RESET) != 0 &&
          top_nest->max_group > cb->bracount)
        cb->bracount = top_nest->max_group;
      if ((top_nest->flags & NSF_EXTENDED) != 0) options |= PCRE2_EXTENDED;
        else options &= ~PCRE2_EXTENDED;
      if ((top_nest->flags & NSF_DUPNAMES) != 0) options |= PCRE2_DUPNAMES;
        else options &= ~PCRE2_DUPNAMES;
      if (top_nest == (nest_save *)(cb->start_workspace)) top_nest = NULL;
        else top_nest--;
      }
    if (nest_depth == 0)    /* Unmatched closing parenthesis */
      {
      errorcode = ERR22;
      goto FAILED;
      }
    nest_depth--;
    break;
    }
  }

if (nest_depth == 0)
  {
  cb->final_bracount = cb->bracount;
  return 0;
  }

/* We give a special error for a missing closing parentheses after (?# because
it might otherwise be hard to see where the missing character is. */

errorcode = (skiptoket == CHAR_NUMBER_SIGN)? ERR18 : ERR14;

FAILED:
*ptrptr = ptr;
return errorcode;
}



/*************************************************
*           Compile one branch                   *
*************************************************/

/* Scan the pattern, compiling it into the a vector. If the options are
changed during the branch, the pointer is used to change the external options
bits. This function is used during the pre-compile phase when we are trying
to find out the amount of memory needed, as well as during the real compile
phase. The value of lengthptr distinguishes the two phases.

Arguments:
  optionsptr        pointer to the option bits
  codeptr           points to the pointer to the current code point
  ptrptr            points to the current pattern pointer
  errorcodeptr      points to error code variable
  firstcuptr        place to put the first required code unit
  firstcuflagsptr   place to put the first code unit flags, or a negative number
  reqcuptr          place to put the last required code unit
  reqcuflagsptr     place to put the last required code unit flags, or a negative number
  bcptr             points to current branch chain
  cond_depth        conditional nesting depth
  cb                contains pointers to tables etc.
  lengthptr         NULL during the real compile phase
                    points to length accumulator during pre-compile phase

Returns:            TRUE on success
                    FALSE, with *errorcodeptr set non-zero on error
*/

static BOOL
compile_branch(uint32_t *optionsptr, PCRE2_UCHAR **codeptr,
  PCRE2_SPTR *ptrptr, int *errorcodeptr,
  uint32_t *firstcuptr, int32_t *firstcuflagsptr,
  uint32_t *reqcuptr, int32_t *reqcuflagsptr,
  branch_chain *bcptr, int cond_depth,
  compile_block *cb, size_t *lengthptr)
{
int repeat_min = 0, repeat_max = 0;      /* To please picky compilers */
int bravalue = 0;
uint32_t greedy_default, greedy_non_default;
uint32_t repeat_type, op_type;
uint32_t options = *optionsptr;               /* May change dynamically */
uint32_t firstcu, reqcu;
int32_t firstcuflags, reqcuflags;
uint32_t zeroreqcu, zerofirstcu;
int32_t zeroreqcuflags, zerofirstcuflags;
int32_t req_caseopt, reqvary, tempreqvary;
int after_manual_callout = 0;
int escape;
size_t length_prevgroup = 0;
register uint32_t c;
register PCRE2_UCHAR *code = *codeptr;
PCRE2_UCHAR *last_code = code;
PCRE2_UCHAR *orig_code = code;
PCRE2_UCHAR *tempcode;
BOOL inescq = FALSE;
BOOL groupsetfirstcu = FALSE;
PCRE2_SPTR ptr = *ptrptr;
PCRE2_SPTR tempptr;
PCRE2_UCHAR *previous = NULL;
PCRE2_UCHAR *previous_callout = NULL;
uint8_t classbits[32];

/* We can fish out the UTF setting once and for all into a BOOL, but we must
not do this for other options (e.g. PCRE2_EXTENDED) because they may change
dynamically as we process the pattern. */

#ifdef SUPPORT_UNICODE
BOOL utf = (options & PCRE2_UTF) != 0;
#if PCRE2_CODE_UNIT_WIDTH != 32
PCRE2_UCHAR utf_units[6];      /* For setting up multi-cu chars */
#endif

#else  /* No UTF support */
BOOL utf = FALSE;
#endif

/* Helper variables for OP_XCLASS opcode (for characters > 255). We define
class_uchardata always so that it can be passed to add_to_class() always,
though it will not be used in non-UTF 8-bit cases. This avoids having to supply
alternative calls for the different cases. */

PCRE2_UCHAR *class_uchardata;
#ifdef SUPPORT_WIDE_CHARS
BOOL xclass;
PCRE2_UCHAR *class_uchardata_base;
#endif

/* Set up the default and non-default settings for greediness */

greedy_default = ((options & PCRE2_UNGREEDY) != 0);
greedy_non_default = greedy_default ^ 1;

/* Initialize no first unit, no required unit. REQ_UNSET means "no char
matching encountered yet". It gets changed to REQ_NONE if we hit something that
matches a non-fixed first unit; reqcu just remains unset if we never find one.

When we hit a repeat whose minimum is zero, we may have to adjust these values
to take the zero repeat into account. This is implemented by setting them to
zerofirstcu and zeroreqcu when such a repeat is encountered. The individual
item types that can be repeated set these backoff variables appropriately. */

firstcu = reqcu = zerofirstcu = zeroreqcu = 0;
firstcuflags = reqcuflags = zerofirstcuflags = zeroreqcuflags = REQ_UNSET;

/* The variable req_caseopt contains either the REQ_CASELESS value or zero,
according to the current setting of the caseless flag. The REQ_CASELESS value
leaves the lower 28 bit empty. It is added into the firstcu or reqcu variables
to record the case status of the value. This is used only for ASCII characters.
*/

req_caseopt = ((options & PCRE2_CASELESS) != 0)? REQ_CASELESS:0;

/* Switch on next character until the end of the branch */

for (;; ptr++)
  {
  BOOL negate_class;
  BOOL should_flip_negation;
  BOOL match_all_or_no_wide_chars;
  BOOL possessive_quantifier;
  BOOL is_quantifier;
  BOOL is_recurse;
  BOOL is_dupname;
  BOOL reset_bracount;
  int class_has_8bitchar;
  int class_one_char;
#ifdef SUPPORT_WIDE_CHARS
  BOOL xclass_has_prop;
#endif
  int recno;                               /* Must be signed */
  int refsign;                             /* Must be signed */
  int terminator;                          /* Must be signed */
  unsigned int mclength;
  unsigned int tempbracount;
  uint32_t ec;
  uint32_t newoptions;
  uint32_t skipunits;
  uint32_t subreqcu, subfirstcu;
  int32_t subreqcuflags, subfirstcuflags;  /* Must be signed */
  PCRE2_UCHAR mcbuffer[8];

  /* Come here to restart the loop. */

  REDO_LOOP:

  /* Get next character in the pattern */

  c = *ptr;

  /* If we are at the end of a nested substitution, revert to the outer level
  string. Nesting only happens one or two levels deep, and the inserted string
  is always zero terminated. */

  if (c == CHAR_NULL && cb->nestptr[0] != NULL)
    {
    ptr = cb->nestptr[0];
    cb->nestptr[0] = cb->nestptr[1];
    cb->nestptr[1] = NULL;
    c = *ptr;
    }

  /* If we are in the pre-compile phase, accumulate the length used for the
  previous cycle of this loop. */

  if (lengthptr != NULL)
    {
    if (code > cb->start_workspace + cb->workspace_size -
        WORK_SIZE_SAFETY_MARGIN)                       /* Check for overrun */
      {
      *errorcodeptr = (code >= cb->start_workspace + cb->workspace_size)?
        ERR52 : ERR86;
      goto FAILED;
      }

    /* There is at least one situation where code goes backwards: this is the
    case of a zero quantifier after a class (e.g. [ab]{0}). At compile time,
    the class is simply eliminated. However, it is created first, so we have to
    allow memory for it. Therefore, don't ever reduce the length at this point.
    */

    if (code < last_code) code = last_code;

    /* Paranoid check for integer overflow */

    if (OFLOW_MAX - *lengthptr < (size_t)(code - last_code))
      {
      *errorcodeptr = ERR20;
      goto FAILED;
      }
    *lengthptr += (size_t)(code - last_code);

    /* If "previous" is set and it is not at the start of the work space, move
    it back to there, in order to avoid filling up the work space. Otherwise,
    if "previous" is NULL, reset the current code pointer to the start. */

    if (previous != NULL)
      {
      if (previous > orig_code)
        {
        memmove(orig_code, previous, (size_t)CU2BYTES(code - previous));
        code -= previous - orig_code;
        previous = orig_code;
        }
      }
    else code = orig_code;

    /* Remember where this code item starts so we can pick up the length
    next time round. */

    last_code = code;
    }

  /* Before doing anything else we must handle all the special items that do
  nothing, and which may come between an item and its quantifier. Otherwise,
  when auto-callouts are enabled, a callout gets incorrectly inserted before
  the quantifier is recognized. After recognizing a "do nothing" item, restart
  the loop in case another one follows. */

  /* If c is not NULL we are not at the end of the pattern. If it is NULL, we
  may still be in the pattern with a NULL data item. In these cases, if we are
  in \Q...\E, check for the \E that ends the literal string; if not, we have a
  literal character. If not in \Q...\E, an isolated \E is ignored. */

  if (c != CHAR_NULL || ptr < cb->end_pattern)
    {
    if (c == CHAR_BACKSLASH && ptr[1] == CHAR_E)
      {
      inescq = FALSE;
      ptr++;
      continue;
      }
    else if (inescq)   /* Literal character */
      {
      if (previous_callout != NULL)
        {
        if (lengthptr == NULL)  /* Don't attempt in pre-compile phase */
          complete_callout(previous_callout, ptr, cb);
        previous_callout = NULL;
        }
      if ((options & PCRE2_AUTO_CALLOUT) != 0)
        {
        previous_callout = code;
        code = auto_callout(code, ptr, cb);
        }
      goto NORMAL_CHAR;
      }

    /* Check for the start of a \Q...\E sequence. We must do this here rather
    than later in case it is immediately followed by \E, which turns it into a
    "do nothing" sequence. */

    if (c == CHAR_BACKSLASH && ptr[1] == CHAR_Q)
      {
      inescq = TRUE;
      ptr++;
      continue;
      }
    }

  /* In extended mode, skip white space and #-comments that end at newline. */

  if ((options & PCRE2_EXTENDED) != 0)
    {
    PCRE2_SPTR wscptr = ptr;
    while (MAX_255(c) && (cb->ctypes[c] & ctype_space) != 0) c = *(++ptr);
    if (c == CHAR_NUMBER_SIGN)
      {
      ptr++;
      while (ptr < cb->end_pattern)
        {
        if (IS_NEWLINE(ptr))         /* For non-fixed-length newline cases, */
          {                          /* IS_NEWLINE sets cb->nllen. */
          ptr += cb->nllen;
          break;
          }
        ptr++;
#ifdef SUPPORT_UNICODE
        if (utf) FORWARDCHAR(ptr);
#endif
        }
      }

    /* If we skipped any characters, restart the loop. Otherwise, we didn't see
    a comment. */

    if (ptr > wscptr) goto REDO_LOOP;
    }

  /* Skip over (?# comments. */

  if (c == CHAR_LEFT_PARENTHESIS && ptr[1] == CHAR_QUESTION_MARK &&
      ptr[2] == CHAR_NUMBER_SIGN)
    {
    ptr += 3;
    while (ptr < cb->end_pattern && *ptr != CHAR_RIGHT_PARENTHESIS) ptr++;
    if (*ptr != CHAR_RIGHT_PARENTHESIS)
      {
      *errorcodeptr = ERR18;
      goto FAILED;
      }
    continue;
    }

  /* End of processing "do nothing" items. See if the next thing is a
  quantifier. */

  is_quantifier =
    c == CHAR_ASTERISK || c == CHAR_PLUS || c == CHAR_QUESTION_MARK ||
     (c == CHAR_LEFT_CURLY_BRACKET && is_counted_repeat(ptr+1));

  /* Fill in length of a previous callout and create an auto callout if
  required, except when the next thing is a quantifier or when processing a
  property substitution string for \w etc in UCP mode. */

  if (!is_quantifier && cb->nestptr[0] == NULL)
    {
    if (previous_callout != NULL && after_manual_callout-- <= 0)
      {
      if (lengthptr == NULL)      /* Don't attempt in pre-compile phase */
        complete_callout(previous_callout, ptr, cb);
      previous_callout = NULL;
      }

    if ((options & PCRE2_AUTO_CALLOUT) != 0)
      {
      previous_callout = code;
      code = auto_callout(code, ptr, cb);
      }
    }

  /* Process the next pattern item. */

  switch(c)
    {
    /* ===================================================================*/
    /* The branch terminates at string end or | or ) */

    case CHAR_NULL:
    if (ptr < cb->end_pattern) goto NORMAL_CHAR;   /* Zero data character */
    /* Fall through */

    case CHAR_VERTICAL_LINE:
    case CHAR_RIGHT_PARENTHESIS:
    *firstcuptr = firstcu;
    *firstcuflagsptr = firstcuflags;
    *reqcuptr = reqcu;
    *reqcuflagsptr = reqcuflags;
    *codeptr = code;
    *ptrptr = ptr;
    if (lengthptr != NULL)
      {
      if (OFLOW_MAX - *lengthptr < (size_t)(code - last_code))
        {
        *errorcodeptr = ERR20;
        goto FAILED;
        }
      *lengthptr += (size_t)(code - last_code);  /* To include callout length */
      }
    return TRUE;


    /* ===================================================================*/
    /* Handle single-character metacharacters. In multiline mode, ^ disables
    the setting of any following char as a first character. */

    case CHAR_CIRCUMFLEX_ACCENT:
    previous = NULL;
    if ((options & PCRE2_MULTILINE) != 0)
      {
      if (firstcuflags == REQ_UNSET)
        zerofirstcuflags = firstcuflags = REQ_NONE;
      *code++ = OP_CIRCM;
      }
    else *code++ = OP_CIRC;
    break;

    case CHAR_DOLLAR_SIGN:
    previous = NULL;
    *code++ = ((options & PCRE2_MULTILINE) != 0)? OP_DOLLM : OP_DOLL;
    break;

    /* There can never be a first char if '.' is first, whatever happens about
    repeats. The value of reqcu doesn't change either. */

    case CHAR_DOT:
    if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
    zerofirstcu = firstcu;
    zerofirstcuflags = firstcuflags;
    zeroreqcu = reqcu;
    zeroreqcuflags = reqcuflags;
    previous = code;
    *code++ = ((options & PCRE2_DOTALL) != 0)? OP_ALLANY: OP_ANY;
    break;


    /* ===================================================================*/
    /* Character classes. If the included characters are all < 256, we build a
    32-byte bitmap of the permitted characters, except in the special case
    where there is only one such character. For negated classes, we build the
    map as usual, then invert it at the end. However, we use a different opcode
    so that data characters > 255 can be handled correctly.

    If the class contains characters outside the 0-255 range, a different
    opcode is compiled. It may optionally have a bit map for characters < 256,
    but those above are are explicitly listed afterwards. A flag byte tells
    whether the bitmap is present, and whether this is a negated class or not.

    An isolated ']' character is not treated specially, so is just another data
    character. In earlier versions of PCRE that used the original API there was
    a "JavaScript compatibility mode" in which it gave an error. However,
    JavaScript itself has changed in this respect so there is no longer any
    need for this special handling.

    In another (POSIX) regex library, the ugly syntax [[:<:]] and [[:>:]] is
    used for "start of word" and "end of word". As these are otherwise illegal
    sequences, we don't break anything by recognizing them. They are replaced
    by \b(?=\w) and \b(?<=\w) respectively. This can only happen at the top
    nesting level, as no other inserted sequences will contains these oddities.
    Sequences like [a[:<:]] are erroneous and are handled by the normal code
    below. */

    case CHAR_LEFT_SQUARE_BRACKET:
    if (PRIV(strncmp_c8)(ptr+1, STRING_WEIRD_STARTWORD, 6) == 0)
      {
      cb->nestptr[0] = ptr + 7;
      ptr = sub_start_of_word;
      goto REDO_LOOP;
      }

    if (PRIV(strncmp_c8)(ptr+1, STRING_WEIRD_ENDWORD, 6) == 0)
      {
      cb->nestptr[0] = ptr + 7;
      ptr = sub_end_of_word;
      goto REDO_LOOP;
      }

    /* Handle a real character class. */

    previous = code;

    /* PCRE supports POSIX class stuff inside a class. Perl gives an error if
    they are encountered at the top level, so we'll do that too. */

    if ((ptr[1] == CHAR_COLON || ptr[1] == CHAR_DOT ||
         ptr[1] == CHAR_EQUALS_SIGN) &&
        check_posix_syntax(ptr, &tempptr))
      {
      *errorcodeptr = (ptr[1] == CHAR_COLON)? ERR12 : ERR13;
      goto FAILED;
      }

    /* If the first character is '^', set the negation flag and skip it. Also,
    if the first few characters (either before or after ^) are \Q\E or \E we
    skip them too. This makes for compatibility with Perl. */

    negate_class = FALSE;
    for (;;)
      {
      c = *(++ptr);
      if (c == CHAR_BACKSLASH)
        {
        if (ptr[1] == CHAR_E)
          ptr++;
        else if (PRIV(strncmp_c8)(ptr + 1, STR_Q STR_BACKSLASH STR_E, 3) == 0)
          ptr += 3;
        else
          break;
        }
      else if (!negate_class && c == CHAR_CIRCUMFLEX_ACCENT)
        negate_class = TRUE;
      else break;
      }

    /* Empty classes are allowed if PCRE2_ALLOW_EMPTY_CLASS is set. Otherwise,
    an initial ']' is taken as a data character -- the code below handles
    that. When empty classes are allowed, [] must always fail, so generate
    OP_FAIL, whereas [^] must match any character, so generate OP_ALLANY. */

    if (c == CHAR_RIGHT_SQUARE_BRACKET &&
        (cb->external_options & PCRE2_ALLOW_EMPTY_CLASS) != 0)
      {
      *code++ = negate_class? OP_ALLANY : OP_FAIL;
      if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
      zerofirstcu = firstcu;
      zerofirstcuflags = firstcuflags;
      break;
      }

    /* If a non-extended class contains a negative special such as \S, we need
    to flip the negation flag at the end, so that support for characters > 255
    works correctly (they are all included in the class). An extended class may
    need to insert specific matching or non-matching code for wide characters.
    */

    should_flip_negation = match_all_or_no_wide_chars = FALSE;

    /* Extended class (xclass) will be used when characters > 255
    might match. */

#ifdef SUPPORT_WIDE_CHARS
    xclass = FALSE;
    class_uchardata = code + LINK_SIZE + 2;   /* For XCLASS items */
    class_uchardata_base = class_uchardata;   /* Save the start */
#endif

    /* For optimization purposes, we track some properties of the class:
    class_has_8bitchar will be non-zero if the class contains at least one 256
    character with a code point less than 256; class_one_char will be 1 if the
    class contains just one character; xclass_has_prop will be TRUE if Unicode
    property checks are present in the class. */

    class_has_8bitchar = 0;
    class_one_char = 0;
#ifdef SUPPORT_WIDE_CHARS
    xclass_has_prop = FALSE;
#endif

    /* Initialize the 256-bit (32-byte) bit map to all zeros. We build the map
    in a temporary bit of memory, in case the class contains fewer than two
    8-bit characters because in that case the compiled code doesn't use the bit
    map. */

    memset(classbits, 0, 32 * sizeof(uint8_t));

    /* Process characters until ] is reached. As the test is at the end of the
    loop, an initial ] is taken as a data character. At the start of the loop,
    c contains the first code unit of the character. If it is zero, check for
    the end of the pattern, to allow binary zero as data. */

    for(;;)
      {
      PCRE2_SPTR oldptr;
#ifdef EBCDIC
      BOOL range_is_literal = TRUE;
#endif

      if (c == CHAR_NULL && ptr >= cb->end_pattern)
        {
        *errorcodeptr = ERR6;  /* Missing terminating ']' */
        goto FAILED;
        }

#ifdef SUPPORT_UNICODE
      if (utf && HAS_EXTRALEN(c))
        {                           /* Braces are required because the */
        GETCHARLEN(c, ptr, ptr);    /* macro generates multiple statements */
        }
#endif

      /* Inside \Q...\E everything is literal except \E */

      if (inescq)
        {
        if (c == CHAR_BACKSLASH && ptr[1] == CHAR_E)  /* If we are at \E */
          {
          inescq = FALSE;                   /* Reset literal state */
          ptr++;                            /* Skip the 'E' */
          goto CONTINUE_CLASS;              /* Carry on with next char */
          }
        goto CHECK_RANGE;                   /* Could be range if \E follows */
        }

      /* Handle POSIX class names. Perl allows a negation extension of the
      form [:^name:]. A square bracket that doesn't match the syntax is
      treated as a literal. We also recognize the POSIX constructions
      [.ch.] and [=ch=] ("collating elements") and fault them, as Perl
      5.6 and 5.8 do. */

      if (c == CHAR_LEFT_SQUARE_BRACKET &&
          (ptr[1] == CHAR_COLON || ptr[1] == CHAR_DOT ||
           ptr[1] == CHAR_EQUALS_SIGN) && check_posix_syntax(ptr, &tempptr))
        {
        BOOL local_negate = FALSE;
        int posix_class, taboffset, tabopt;
        register const uint8_t *cbits = cb->cbits;
        uint8_t pbits[32];

        if (ptr[1] != CHAR_COLON)
          {
          *errorcodeptr = ERR13;
          goto FAILED;
          }

        ptr += 2;
        if (*ptr == CHAR_CIRCUMFLEX_ACCENT)
          {
          local_negate = TRUE;
          should_flip_negation = TRUE;  /* Note negative special */
          ptr++;
          }

        posix_class = check_posix_name(ptr, (int)(tempptr - ptr));
        if (posix_class < 0)
          {
          *errorcodeptr = ERR30;
          goto FAILED;
          }

        /* If matching is caseless, upper and lower are converted to
        alpha. This relies on the fact that the class table starts with
        alpha, lower, upper as the first 3 entries. */

        if ((options & PCRE2_CASELESS) != 0 && posix_class <= 2)
          posix_class = 0;

        /* When PCRE2_UCP is set, some of the POSIX classes are converted to
        different escape sequences that use Unicode properties \p or \P. Others
        that are not available via \p or \P generate XCL_PROP/XCL_NOTPROP
        directly. UCP support is not available unless UTF support is.*/

#ifdef SUPPORT_UNICODE
        if ((options & PCRE2_UCP) != 0)
          {
          unsigned int ptype = 0;
          int pc = posix_class + ((local_negate)? POSIX_SUBSIZE/2 : 0);

          /* The posix_substitutes table specifies which POSIX classes can be
          converted to \p or \P items. This can only happen at top nestling
          level, as there will never be a POSIX class in a string that is
          substituted for something else. */

          if (posix_substitutes[pc] != NULL)
            {
            cb->nestptr[0] = tempptr + 1;
            ptr = posix_substitutes[pc] - 1;
            goto CONTINUE_CLASS;
            }

          /* There are three other classes that generate special property calls
          that are recognized only in an XCLASS. */

          else switch(posix_class)
            {
            case PC_GRAPH:
            ptype = PT_PXGRAPH;
            /* Fall through */
            case PC_PRINT:
            if (ptype == 0) ptype = PT_PXPRINT;
            /* Fall through */
            case PC_PUNCT:
            if (ptype == 0) ptype = PT_PXPUNCT;
            *class_uchardata++ = local_negate? XCL_NOTPROP : XCL_PROP;
            *class_uchardata++ = (PCRE2_UCHAR)ptype;
            *class_uchardata++ = 0;
            xclass_has_prop = TRUE;
            ptr = tempptr + 1;
            goto CONTINUE_CLASS;

            /* For the other POSIX classes (ascii, xdigit) we are going to fall
            through to the non-UCP case and build a bit map for characters with
            code points less than 256. However, if we are in a negated POSIX
            class, characters with code points greater than 255 must either all
            match or all not match, depending on whether the whole class is not
            or is negated. For example, for [[:^ascii:]... they must all match,
            whereas for [^[:^xdigit:]... they must not.

            In the special case where there are no xclass items, this is
            automatically handled by the use of OP_CLASS or OP_NCLASS, but an
            explicit range is needed for OP_XCLASS. Setting a flag here causes
            the range to be generated later when it is known that OP_XCLASS is
            required. */

            default:
            match_all_or_no_wide_chars |= local_negate;
            break;
            }
          }
#endif  /* SUPPORT_UNICODE */

        /* In the non-UCP case, or when UCP makes no difference, we build the
        bit map for the POSIX class in a chunk of local store because we may be
        adding and subtracting from it, and we don't want to subtract bits that
        may be in the main map already. At the end we or the result into the
        bit map that is being built. */

        posix_class *= 3;

        /* Copy in the first table (always present) */

        memcpy(pbits, cbits + posix_class_maps[posix_class],
          32 * sizeof(uint8_t));

        /* If there is a second table, add or remove it as required. */

        taboffset = posix_class_maps[posix_class + 1];
        tabopt = posix_class_maps[posix_class + 2];

        if (taboffset >= 0)
          {
          if (tabopt >= 0)
            for (c = 0; c < 32; c++) pbits[c] |= cbits[(int)c + taboffset];
          else
            for (c = 0; c < 32; c++) pbits[c] &= ~cbits[(int)c + taboffset];
          }

        /* Now see if we need to remove any special characters. An option
        value of 1 removes vertical space and 2 removes underscore. */

        if (tabopt < 0) tabopt = -tabopt;
        if (tabopt == 1) pbits[1] &= ~0x3c;
          else if (tabopt == 2) pbits[11] &= 0x7f;

        /* Add the POSIX table or its complement into the main table that is
        being built and we are done. */

        if (local_negate)
          for (c = 0; c < 32; c++) classbits[c] |= ~pbits[c];
        else
          for (c = 0; c < 32; c++) classbits[c] |= pbits[c];

        ptr = tempptr + 1;
        /* Every class contains at least one < 256 character. */
        class_has_8bitchar = 1;
        /* Every class contains at least two characters. */
        class_one_char = 2;
        goto CONTINUE_CLASS;    /* End of POSIX syntax handling */
        }

      /* Backslash may introduce a single character, or it may introduce one
      of the specials, which just set a flag. The sequence \b is a special
      case. Inside a class (and only there) it is treated as backspace. We
      assume that other escapes have more than one character in them, so
      speculatively set both class_has_8bitchar and class_one_char bigger
      than one. Unrecognized escapes fall through and are faulted. */

      if (c == CHAR_BACKSLASH)
        {
        escape = PRIV(check_escape)(&ptr, cb->end_pattern, &ec, errorcodeptr,
          options, TRUE, cb);
        if (*errorcodeptr != 0) goto FAILED;
        if (escape == 0)    /* Escaped single char */
          {
          c = ec;
#ifdef EBCDIC
          range_is_literal = FALSE;
#endif
          }
        else if (escape == ESC_b) c = CHAR_BS; /* \b is backspace in a class */
        else if (escape == ESC_N)          /* \N is not supported in a class */
          {
          *errorcodeptr = ERR71;
          goto FAILED;
          }
        else if (escape == ESC_Q)            /* Handle start of quoted string */
          {
          if (ptr[1] == CHAR_BACKSLASH && ptr[2] == CHAR_E)
            {
            ptr += 2; /* avoid empty string */
            }
          else inescq = TRUE;
          goto CONTINUE_CLASS;
          }
        else if (escape == ESC_E) goto CONTINUE_CLASS;  /* Ignore orphan \E */

        else  /* Handle \d-type escapes */
          {
          register const uint8_t *cbits = cb->cbits;
          /* Every class contains at least two < 256 characters. */
          class_has_8bitchar++;
          /* Every class contains at least two characters. */
          class_one_char += 2;

          switch (escape)
            {
#ifdef SUPPORT_UNICODE
            case ESC_du:     /* These are the values given for \d etc */
            case ESC_DU:     /* when PCRE2_UCP is set. We replace the */
            case ESC_wu:     /* escape sequence with an appropriate \p */
            case ESC_WU:     /* or \P to test Unicode properties instead */
            case ESC_su:     /* of the default ASCII testing. This might be */
            case ESC_SU:     /* a 2nd-level nesting for [[:<:]] or [[:>:]]. */
            cb->nestptr[1] = cb->nestptr[0];
            cb->nestptr[0] = ptr;
            ptr = substitutes[escape - ESC_DU] - 1;  /* Just before substitute */
            class_has_8bitchar--;                /* Undo! */
            break;
#endif
            case ESC_d:
            for (c = 0; c < 32; c++) classbits[c] |= cbits[c+cbit_digit];
            break;

            case ESC_D:
            should_flip_negation = TRUE;
            for (c = 0; c < 32; c++) classbits[c] |= ~cbits[c+cbit_digit];
            break;

            case ESC_w:
            for (c = 0; c < 32; c++) classbits[c] |= cbits[c+cbit_word];
            break;

            case ESC_W:
            should_flip_negation = TRUE;
            for (c = 0; c < 32; c++) classbits[c] |= ~cbits[c+cbit_word];
            break;

            /* Perl 5.004 onwards omitted VT from \s, but restored it at Perl
            5.18. Before PCRE 8.34, we had to preserve the VT bit if it was
            previously set by something earlier in the character class.
            Luckily, the value of CHAR_VT is 0x0b in both ASCII and EBCDIC, so
            we could just adjust the appropriate bit. From PCRE 8.34 we no
            longer treat \s and \S specially. */

            case ESC_s:
            for (c = 0; c < 32; c++) classbits[c] |= cbits[c+cbit_space];
            break;

            case ESC_S:
            should_flip_negation = TRUE;
            for (c = 0; c < 32; c++) classbits[c] |= ~cbits[c+cbit_space];
            break;

            /* The rest apply in both UCP and non-UCP cases. */

            case ESC_h:
            (void)add_list_to_class(classbits, &class_uchardata, options, cb,
              PRIV(hspace_list), NOTACHAR);
            break;

            case ESC_H:
            (void)add_not_list_to_class(classbits, &class_uchardata, options,
              cb, PRIV(hspace_list));
            break;

            case ESC_v:
            (void)add_list_to_class(classbits, &class_uchardata, options, cb,
              PRIV(vspace_list), NOTACHAR);
            break;

            case ESC_V:
            (void)add_not_list_to_class(classbits, &class_uchardata, options,
              cb, PRIV(vspace_list));
            break;

            case ESC_p:
            case ESC_P:
#ifdef SUPPORT_UNICODE
              {
              BOOL negated;
              unsigned int ptype = 0, pdata = 0;
              if (!get_ucp(&ptr, &negated, &ptype, &pdata, errorcodeptr, cb))
                goto FAILED;
              *class_uchardata++ = ((escape == ESC_p) != negated)?
                XCL_PROP : XCL_NOTPROP;
              *class_uchardata++ = ptype;
              *class_uchardata++ = pdata;
              xclass_has_prop = TRUE;
              class_has_8bitchar--;                /* Undo! */
              }
            break;
#else
            *errorcodeptr = ERR45;
            goto FAILED;
#endif
            /* Unrecognized escapes are faulted. */

            default:
            *errorcodeptr = ERR7;
            goto FAILED;
            }

          /* Handled \d-type escape */

          goto CONTINUE_CLASS;
          }

        /* Control gets here if the escape just defined a single character.
        This is in c and may be greater than 256. */

        escape = 0;
        }   /* End of backslash handling */

      /* A character may be followed by '-' to form a range. However, Perl does
      not permit ']' to be the end of the range. A '-' character at the end is
      treated as a literal. Perl ignores orphaned \E sequences entirely. The
      code for handling \Q and \E is messy. */

      CHECK_RANGE:
      while (ptr[1] == CHAR_BACKSLASH && ptr[2] == CHAR_E)
        {
        inescq = FALSE;
        ptr += 2;
        }
      oldptr = ptr;

      /* Remember if \r or \n were explicitly used */

      if (c == CHAR_CR || c == CHAR_NL) cb->external_flags |= PCRE2_HASCRORLF;

      /* Check for range */

      if (!inescq && ptr[1] == CHAR_MINUS)
        {
        uint32_t d;
        ptr += 2;
        while (*ptr == CHAR_BACKSLASH && ptr[1] == CHAR_E) ptr += 2;

        /* If we hit \Q (not followed by \E) at this point, go into escaped
        mode. */

        while (*ptr == CHAR_BACKSLASH && ptr[1] == CHAR_Q)
          {
          ptr += 2;
          if (*ptr == CHAR_BACKSLASH && ptr[1] == CHAR_E)
            { ptr += 2; continue; }
          inescq = TRUE;
          break;
          }

        /* Minus (hyphen) at the end of a class is treated as a literal, so put
        back the pointer and jump to handle the character that preceded it. */

        if (*ptr == CHAR_NULL || (!inescq && *ptr == CHAR_RIGHT_SQUARE_BRACKET))
          {
          ptr = oldptr;
          goto CLASS_SINGLE_CHARACTER;
          }

        /* Otherwise, we have a potential range; pick up the next character */

#ifdef SUPPORT_UNICODE
        if (utf)
          {                           /* Braces are required because the */
          GETCHARLEN(d, ptr, ptr);    /* macro generates multiple statements */
          }
        else
#endif
        d = *ptr;  /* Not UTF mode */

        /* The second part of a range can be a single-character escape
        sequence, but not any of the other escapes. Perl treats a hyphen as a
        literal in such circumstances. However, in Perl's warning mode, a
        warning is given, so PCRE now faults it as it is almost certainly a
        mistake on the user's part. */

        if (!inescq)
          {
          if (d == CHAR_BACKSLASH)
            {
            int descape;
            descape = PRIV(check_escape)(&ptr, cb->end_pattern, &d,
              errorcodeptr, options, TRUE, cb);
            if (*errorcodeptr != 0) goto FAILED;
#ifdef EBCDIC
            range_is_literal = FALSE;
#endif
            /* 0 means a character was put into d; \b is backspace; any other
            special causes an error. */

            if (descape != 0)
              {
              if (descape == ESC_b) d = CHAR_BS; else
                {
                *errorcodeptr = ERR50;
                goto FAILED;
                }
              }
            }

          /* A hyphen followed by a POSIX class is treated in the same way. */

          else if (d == CHAR_LEFT_SQUARE_BRACKET &&
                   (ptr[1] == CHAR_COLON || ptr[1] == CHAR_DOT ||
                    ptr[1] == CHAR_EQUALS_SIGN) &&
                   check_posix_syntax(ptr, &tempptr))
            {
            *errorcodeptr = ERR50;
            goto FAILED;
            }
          }

        /* Check that the two values are in the correct order. Optimize
        one-character ranges. */

        if (d < c)
          {
          *errorcodeptr = ERR8;
          goto FAILED;
          }
        if (d == c) goto CLASS_SINGLE_CHARACTER;  /* A few lines below */

        /* We have found a character range, so single character optimizations
        cannot be done anymore. Any value greater than 1 indicates that there
        is more than one character. */

        class_one_char = 2;

        /* Remember an explicit \r or \n, and add the range to the class. */

        if (d == CHAR_CR || d == CHAR_NL) cb->external_flags |= PCRE2_HASCRORLF;

        /* In an EBCDIC environment, Perl treats alphabetic ranges specially
        because there are holes in the encoding, and simply using the range A-Z
        (for example) would include the characters in the holes. This applies
        only to literal ranges; [\xC1-\xE9] is different to [A-Z]. */

#ifdef EBCDIC
        if (range_is_literal &&
             (cb->ctypes[c] & ctype_letter) != 0 &&
             (cb->ctypes[d] & ctype_letter) != 0 &&
             (c <= CHAR_z) == (d <= CHAR_z))
          {
          uint32_t uc = (c <= CHAR_z)? 0 : 64;
          uint32_t C = c - uc;
          uint32_t D = d - uc;

          if (C <= CHAR_i)
            {
            class_has_8bitchar +=
              add_to_class(classbits, &class_uchardata, options, cb, C + uc,
                ((D < CHAR_i)? D : CHAR_i) + uc);
            C = CHAR_j;
            }

          if (C <= D && C <= CHAR_r)
            {
            class_has_8bitchar +=
              add_to_class(classbits, &class_uchardata, options, cb, C + uc,
                ((D < CHAR_r)? D : CHAR_r) + uc);
            C = CHAR_s;
            }

          if (C <= D)
            {
            class_has_8bitchar +=
              add_to_class(classbits, &class_uchardata, options, cb, C + uc,
                D + uc);
            }
          }
        else
#endif
        class_has_8bitchar +=
          add_to_class(classbits, &class_uchardata, options, cb, c, d);
        goto CONTINUE_CLASS;   /* Go get the next char in the class */
        }

      /* Handle a single character - we can get here for a normal non-escape
      char, or after \ that introduces a single character or for an apparent
      range that isn't. Only the value 1 matters for class_one_char, so don't
      increase it if it is already 2 or more ... just in case there's a class
      with a zillion characters in it. */

      CLASS_SINGLE_CHARACTER:
      if (class_one_char < 2) class_one_char++;

      /* If class_one_char is 1 and xclass_has_prop is false, we have the first
      single character in the class, and there have been no prior ranges, or
      XCLASS items generated by escapes. If this is the final character in the
      class, we can optimize by turning the item into a 1-character OP_CHAR[I]
      if it's positive, or OP_NOT[I] if it's negative. In the positive case, it
      can cause firstcu to be set. Otherwise, there can be no first char if
      this item is first, whatever repeat count may follow. In the case of
      reqcu, save the previous value for reinstating. */

      if (!inescq &&
#ifdef SUPPORT_UNICODE
          !xclass_has_prop &&
#endif
          class_one_char == 1 && ptr[1] == CHAR_RIGHT_SQUARE_BRACKET)
        {
        ptr++;
        zeroreqcu = reqcu;
        zeroreqcuflags = reqcuflags;

        if (negate_class)
          {
#ifdef SUPPORT_UNICODE
          int d;
#endif
          if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
          zerofirstcu = firstcu;
          zerofirstcuflags = firstcuflags;

          /* For caseless UTF mode, check whether this character has more than
          one other case. If so, generate a special OP_NOTPROP item instead of
          OP_NOTI. */

#ifdef SUPPORT_UNICODE
          if (utf && (options & PCRE2_CASELESS) != 0 &&
              (d = UCD_CASESET(c)) != 0)
            {
            *code++ = OP_NOTPROP;
            *code++ = PT_CLIST;
            *code++ = d;
            }
          else
#endif
          /* Char has only one other case, or UCP not available */

            {
            *code++ = ((options & PCRE2_CASELESS) != 0)? OP_NOTI: OP_NOT;
            code += PUTCHAR(c, code);
            }

          /* We are finished with this character class */

          goto END_CLASS;
          }

        /* For a single, positive character, get the value into mcbuffer, and
        then we can handle this with the normal one-character code. */

        mclength = PUTCHAR(c, mcbuffer);
        goto ONE_CHAR;
        }       /* End of 1-char optimization */

      /* There is more than one character in the class, or an XCLASS item
      has been generated. Add this character to the class. */

      class_has_8bitchar +=
        add_to_class(classbits, &class_uchardata, options, cb, c, c);

      /* Continue to the next character in the class. Closing square bracket
      not within \Q..\E ends the class. A NULL character terminates a
      nested substitution string, but may be a data character in the main
      pattern (tested at the start of this loop). */

      CONTINUE_CLASS:
      c = *(++ptr);
      if (c == CHAR_NULL && cb->nestptr[0] != NULL)
        {
        ptr = cb->nestptr[0];
        cb->nestptr[0] = cb->nestptr[1];
        cb->nestptr[1] = NULL;
        c = *(++ptr);
        }

#ifdef SUPPORT_WIDE_CHARS
      /* If any wide characters have been encountered, set xclass = TRUE. Then,
      in the pre-compile phase, accumulate the length of the wide characters
      and reset the pointer. This is so that very large classes that contain a
      zillion wide characters do not overwrite the work space (which is on the
      stack). */

      if (class_uchardata > class_uchardata_base)
        {
        xclass = TRUE;
        if (lengthptr != NULL)
          {
          *lengthptr += class_uchardata - class_uchardata_base;
          class_uchardata = class_uchardata_base;
          }
        }
#endif
      /* An unescaped ] ends the class */

      if (c == CHAR_RIGHT_SQUARE_BRACKET && !inescq) break;
      }   /* End of main class-processing loop */

    /* If this is the first thing in the branch, there can be no first char
    setting, whatever the repeat count. Any reqcu setting must remain
    unchanged after any kind of repeat. */

    if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
    zerofirstcu = firstcu;
    zerofirstcuflags = firstcuflags;
    zeroreqcu = reqcu;
    zeroreqcuflags = reqcuflags;

    /* If there are characters with values > 255, or Unicode property settings
    (\p or \P), we have to compile an extended class, with its own opcode,
    unless there were no property settings and there was a negated special such
    as \S in the class, and PCRE2_UCP is not set, because in that case all
    characters > 255 are in or not in the class, so any that were explicitly
    given as well can be ignored.

    In the UCP case, if certain negated POSIX classes ([:^ascii:] or
    [^:xdigit:]) were present in a class, we either have to match or not match
    all wide characters (depending on whether the whole class is or is not
    negated). This requirement is indicated by match_all_or_no_wide_chars being
    true. We do this by including an explicit range, which works in both cases.

    If, when generating an xclass, there are no characters < 256, we can omit
    the bitmap in the actual compiled code. */

#ifdef SUPPORT_WIDE_CHARS
#ifdef SUPPORT_UNICODE
    if (xclass && (xclass_has_prop || !should_flip_negation ||
         (options & PCRE2_UCP) != 0))
#elif PCRE2_CODE_UNIT_WIDTH != 8
    if (xclass && (xclass_has_prop || !should_flip_negation))
#endif
      {
      if (match_all_or_no_wide_chars)
        {
        *class_uchardata++ = XCL_RANGE;
        class_uchardata += PRIV(ord2utf)(0x100, class_uchardata);
        class_uchardata += PRIV(ord2utf)(MAX_UTF_CODE_POINT, class_uchardata);
        }
      *class_uchardata++ = XCL_END;    /* Marks the end of extra data */
      *code++ = OP_XCLASS;
      code += LINK_SIZE;
      *code = negate_class? XCL_NOT:0;
      if (xclass_has_prop) *code |= XCL_HASPROP;

      /* If the map is required, move up the extra data to make room for it;
      otherwise just move the code pointer to the end of the extra data. */

      if (class_has_8bitchar > 0)
        {
        *code++ |= XCL_MAP;
        memmove(code + (32 / sizeof(PCRE2_UCHAR)), code,
          CU2BYTES(class_uchardata - code));
        if (negate_class && !xclass_has_prop)
          for (c = 0; c < 32; c++) classbits[c] = ~classbits[c];
        memcpy(code, classbits, 32);
        code = class_uchardata + (32 / sizeof(PCRE2_UCHAR));
        }
      else code = class_uchardata;

      /* Now fill in the complete length of the item */

      PUT(previous, 1, (int)(code - previous));
      break;   /* End of class handling */
      }
#endif

    /* If there are no characters > 255, or they are all to be included or
    excluded, set the opcode to OP_CLASS or OP_NCLASS, depending on whether the
    whole class was negated and whether there were negative specials such as \S
    (non-UCP) in the class. Then copy the 32-byte map into the code vector,
    negating it if necessary. */

    *code++ = (negate_class == should_flip_negation) ? OP_CLASS : OP_NCLASS;
    if (lengthptr == NULL)    /* Save time in the pre-compile phase */
      {
      if (negate_class)
        for (c = 0; c < 32; c++) classbits[c] = ~classbits[c];
      memcpy(code, classbits, 32);
      }
    code += 32 / sizeof(PCRE2_UCHAR);

    END_CLASS:
    break;


    /* ===================================================================*/
    /* Various kinds of repeat; '{' is not necessarily a quantifier, but this
    has been tested above. */

    case CHAR_LEFT_CURLY_BRACKET:
    if (!is_quantifier) goto NORMAL_CHAR;
    ptr = read_repeat_counts(ptr+1, &repeat_min, &repeat_max, errorcodeptr);
    if (*errorcodeptr != 0) goto FAILED;
    goto REPEAT;

    case CHAR_ASTERISK:
    repeat_min = 0;
    repeat_max = -1;
    goto REPEAT;

    case CHAR_PLUS:
    repeat_min = 1;
    repeat_max = -1;
    goto REPEAT;

    case CHAR_QUESTION_MARK:
    repeat_min = 0;
    repeat_max = 1;

    REPEAT:
    if (previous == NULL)
      {
      *errorcodeptr = ERR9;
      goto FAILED;
      }

    if (repeat_min == 0)
      {
      firstcu = zerofirstcu;    /* Adjust for zero repeat */
      firstcuflags = zerofirstcuflags;
      reqcu = zeroreqcu;        /* Ditto */
      reqcuflags = zeroreqcuflags;
      }

    /* Remember whether this is a variable length repeat */

    reqvary = (repeat_min == repeat_max)? 0 : REQ_VARY;

    op_type = 0;                    /* Default single-char op codes */
    possessive_quantifier = FALSE;  /* Default not possessive quantifier */

    /* Save start of previous item, in case we have to move it up in order to
    insert something before it. */

    tempcode = previous;

    /* Before checking for a possessive quantifier, we must skip over
    whitespace and comments in extended mode because Perl allows white space at
    this point. */

    if ((options & PCRE2_EXTENDED) != 0)
      {
      ptr++;
      for (;;)
        {
        while (MAX_255(*ptr) && (cb->ctypes[*ptr] & ctype_space) != 0) ptr++;
        if (*ptr != CHAR_NUMBER_SIGN) break;
        ptr++;
        while (ptr < cb->end_pattern)
          {
          if (IS_NEWLINE(ptr))         /* For non-fixed-length newline cases, */
            {                        /* IS_NEWLINE sets cb->nllen. */
            ptr += cb->nllen;
            break;
            }
          ptr++;
#ifdef SUPPORT_UNICODE
          if (utf) FORWARDCHAR(ptr);
#endif
          }           /* Loop for comment characters */
        }             /* Loop for multiple comments */
      ptr--;          /* Last code unit of previous character. */
      }

    /* If the next character is '+', we have a possessive quantifier. This
    implies greediness, whatever the setting of the PCRE2_UNGREEDY option.
    If the next character is '?' this is a minimizing repeat, by default,
    but if PCRE2_UNGREEDY is set, it works the other way round. We change the
    repeat type to the non-default. */

    if (ptr[1] == CHAR_PLUS)
      {
      repeat_type = 0;                  /* Force greedy */
      possessive_quantifier = TRUE;
      ptr++;
      }
    else if (ptr[1] == CHAR_QUESTION_MARK)
      {
      repeat_type = greedy_non_default;
      ptr++;
      }
    else repeat_type = greedy_default;

    /* If the repeat is {1} we can ignore it. */

    if (repeat_max == 1 && repeat_min == 1) goto END_REPEAT;

    /* If previous was a recursion call, wrap it in atomic brackets so that
    previous becomes the atomic group. All recursions were so wrapped in the
    past, but it no longer happens for non-repeated recursions. In fact, the
    repeated ones could be re-implemented independently so as not to need this,
    but for the moment we rely on the code for repeating groups. */

    if (*previous == OP_RECURSE)
      {
      memmove(previous + 1 + LINK_SIZE, previous, CU2BYTES(1 + LINK_SIZE));
      *previous = OP_ONCE;
      PUT(previous, 1, 2 + 2*LINK_SIZE);
      previous[2 + 2*LINK_SIZE] = OP_KET;
      PUT(previous, 3 + 2*LINK_SIZE, 2 + 2*LINK_SIZE);
      code += 2 + 2 * LINK_SIZE;
      length_prevgroup = 3 + 3*LINK_SIZE;
      }

    /* Now handle repetition for the different types of item. */

    /* If previous was a character or negated character match, abolish the item
    and generate a repeat item instead. If a char item has a minimum of more
    than one, ensure that it is set in reqcu - it might not be if a sequence
    such as x{3} is the first thing in a branch because the x will have gone
    into firstcu instead.  */

    if (*previous == OP_CHAR || *previous == OP_CHARI
        || *previous == OP_NOT || *previous == OP_NOTI)
      {
      switch (*previous)
        {
        default: /* Make compiler happy. */
        case OP_CHAR:  op_type = OP_STAR - OP_STAR; break;
        case OP_CHARI: op_type = OP_STARI - OP_STAR; break;
        case OP_NOT:   op_type = OP_NOTSTAR - OP_STAR; break;
        case OP_NOTI:  op_type = OP_NOTSTARI - OP_STAR; break;
        }

      /* Deal with UTF characters that take up more than one code unit. It's
      easier to write this out separately than try to macrify it. Use c to
      hold the length of the character in code units, plus UTF_LENGTH to flag
      that it's a length rather than a small character. */

#ifdef MAYBE_UTF_MULTI
      if (utf && NOT_FIRSTCU(code[-1]))
        {
        PCRE2_UCHAR *lastchar = code - 1;
        BACKCHAR(lastchar);
        c = (int)(code - lastchar);               /* Length of UTF character */
        memcpy(utf_units, lastchar, CU2BYTES(c)); /* Save the char */
        c |= UTF_LENGTH;                          /* Flag c as a length */
        }
      else
#endif  /* MAYBE_UTF_MULTI */

      /* Handle the case of a single charater - either with no UTF support, or
      with UTF disabled, or for a single-code-unit UTF character. */
        {
        c = code[-1];
        if (*previous <= OP_CHARI && repeat_min > 1)
          {
          reqcu = c;
          reqcuflags = req_caseopt | cb->req_varyopt;
          }
        }

      goto OUTPUT_SINGLE_REPEAT;   /* Code shared with single character types */
      }

    /* If previous was a character type match (\d or similar), abolish it and
    create a suitable repeat item. The code is shared with single-character
    repeats by setting op_type to add a suitable offset into repeat_type. Note
    the the Unicode property types will be present only when SUPPORT_UNICODE is
    defined, but we don't wrap the little bits of code here because it just
    makes it horribly messy. */

    else if (*previous < OP_EODN)
      {
      PCRE2_UCHAR *oldcode;
      int prop_type, prop_value;
      op_type = OP_TYPESTAR - OP_STAR;      /* Use type opcodes */
      c = *previous;                        /* Save previous opcode */
      if (c == OP_PROP || c == OP_NOTPROP)
        {
        prop_type = previous[1];
        prop_value = previous[2];
        }
      else
        {
        /* Come here from just above with a character in c */
        OUTPUT_SINGLE_REPEAT:
        prop_type = prop_value = -1;
        }

      /* At this point we either have prop_type == prop_value == -1 and either
      a code point or a character type that is not OP_[NOT]PROP in c, or we
      have OP_[NOT]PROP in c and prop_type/prop_value not negative. */

      oldcode = code;                   /* Save where we were */
      code = previous;                  /* Usually overwrite previous item */

      /* If the maximum is zero then the minimum must also be zero; Perl allows
      this case, so we do too - by simply omitting the item altogether. */

      if (repeat_max == 0) goto END_REPEAT;

      /* Combine the op_type with the repeat_type */

      repeat_type += op_type;

      /* A minimum of zero is handled either as the special case * or ?, or as
      an UPTO, with the maximum given. */

      if (repeat_min == 0)
        {
        if (repeat_max == -1) *code++ = OP_STAR + repeat_type;
          else if (repeat_max == 1) *code++ = OP_QUERY + repeat_type;
        else
          {
          *code++ = OP_UPTO + repeat_type;
          PUT2INC(code, 0, repeat_max);
          }
        }

      /* A repeat minimum of 1 is optimized into some special cases. If the
      maximum is unlimited, we use OP_PLUS. Otherwise, the original item is
      left in place and, if the maximum is greater than 1, we use OP_UPTO with
      one less than the maximum. */

      else if (repeat_min == 1)
        {
        if (repeat_max == -1)
          *code++ = OP_PLUS + repeat_type;
        else
          {
          code = oldcode;                 /* Leave previous item in place */
          if (repeat_max == 1) goto END_REPEAT;
          *code++ = OP_UPTO + repeat_type;
          PUT2INC(code, 0, repeat_max - 1);
          }
        }

      /* The case {n,n} is just an EXACT, while the general case {n,m} is
      handled as an EXACT followed by an UPTO or STAR or QUERY. */

      else
        {
        *code++ = OP_EXACT + op_type;  /* NB EXACT doesn't have repeat_type */
        PUT2INC(code, 0, repeat_min);

        /* Unless repeat_max equals repeat_min, fill in the data for EXACT, and
        then generate the second opcode. In UTF mode, multi-code-unit
        characters have their length in c, with the UTF_LENGTH bit as a flag,
        and the code units in utf_units. For a repeated Unicode property match,
        there are two extra values that define the required property, and c
        never has the UTF_LENGTH bit set. */

        if (repeat_max != repeat_min)
          {
#ifdef MAYBE_UTF_MULTI
          if (utf && (c & UTF_LENGTH) != 0)
            {
            memcpy(code, utf_units, CU2BYTES(c & 7));
            code += c & 7;
            }
          else
#endif  /* MAYBE_UTF_MULTI */
            {
            *code++ = c;
            if (prop_type >= 0)
              {
              *code++ = prop_type;
              *code++ = prop_value;
              }
            }

          /* Now set up the following opcode */

          if (repeat_max < 0) *code++ = OP_STAR + repeat_type; else
            {
            repeat_max -= repeat_min;
            if (repeat_max == 1)
              {
              *code++ = OP_QUERY + repeat_type;
              }
            else
              {
              *code++ = OP_UPTO + repeat_type;
              PUT2INC(code, 0, repeat_max);
              }
            }
          }
        }

      /* Fill in the character or character type for the final opcode. */

#ifdef MAYBE_UTF_MULTI
      if (utf && (c & UTF_LENGTH) != 0)
        {
        memcpy(code, utf_units, CU2BYTES(c & 7));
        code += c & 7;
        }
      else
#endif  /* MAYBEW_UTF_MULTI */
        {
        *code++ = c;
        if (prop_type >= 0)
          {
          *code++ = prop_type;
          *code++ = prop_value;
          }
        }
      }

    /* If previous was a character class or a back reference, we put the repeat
    stuff after it, but just skip the item if the repeat was {0,0}. */

    else if (*previous == OP_CLASS || *previous == OP_NCLASS ||
#ifdef SUPPORT_WIDE_CHARS
             *previous == OP_XCLASS ||
#endif
             *previous == OP_REF   || *previous == OP_REFI ||
             *previous == OP_DNREF || *previous == OP_DNREFI)
      {
      if (repeat_max == 0)
        {
        code = previous;
        goto END_REPEAT;
        }

      if (repeat_min == 0 && repeat_max == -1)
        *code++ = OP_CRSTAR + repeat_type;
      else if (repeat_min == 1 && repeat_max == -1)
        *code++ = OP_CRPLUS + repeat_type;
      else if (repeat_min == 0 && repeat_max == 1)
        *code++ = OP_CRQUERY + repeat_type;
      else
        {
        *code++ = OP_CRRANGE + repeat_type;
        PUT2INC(code, 0, repeat_min);
        if (repeat_max == -1) repeat_max = 0;  /* 2-byte encoding for max */
        PUT2INC(code, 0, repeat_max);
        }
      }

    /* If previous was a bracket group, we may have to replicate it in certain
    cases. Note that at this point we can encounter only the "basic" bracket
    opcodes such as BRA and CBRA, as this is the place where they get converted
    into the more special varieties such as BRAPOS and SBRA. A test for >=
    OP_ASSERT and <= OP_COND includes ASSERT, ASSERT_NOT, ASSERTBACK,
    ASSERTBACK_NOT, ONCE, ONCE_NC, BRA, BRAPOS, CBRA, CBRAPOS, and COND.
    Originally, PCRE did not allow repetition of assertions, but now it does,
    for Perl compatibility. */

    else if (*previous >= OP_ASSERT && *previous <= OP_COND)
      {
      register int i;
      int len = (int)(code - previous);
      PCRE2_UCHAR *bralink = NULL;
      PCRE2_UCHAR *brazeroptr = NULL;

      /* Repeating a DEFINE group (or any group where the condition is always
      FALSE and there is only one branch) is pointless, but Perl allows the
      syntax, so we just ignore the repeat. */

      if (*previous == OP_COND && previous[LINK_SIZE+1] == OP_FALSE &&
          previous[GET(previous, 1)] != OP_ALT)
        goto END_REPEAT;

      /* There is no sense in actually repeating assertions. The only potential
      use of repetition is in cases when the assertion is optional. Therefore,
      if the minimum is greater than zero, just ignore the repeat. If the
      maximum is not zero or one, set it to 1. */

      if (*previous < OP_ONCE)    /* Assertion */
        {
        if (repeat_min > 0) goto END_REPEAT;
        if (repeat_max < 0 || repeat_max > 1) repeat_max = 1;
        }

      /* The case of a zero minimum is special because of the need to stick
      OP_BRAZERO in front of it, and because the group appears once in the
      data, whereas in other cases it appears the minimum number of times. For
      this reason, it is simplest to treat this case separately, as otherwise
      the code gets far too messy. There are several special subcases when the
      minimum is zero. */

      if (repeat_min == 0)
        {
        /* If the maximum is also zero, we used to just omit the group from the
        output altogether, like this:

        ** if (repeat_max == 0)
        **   {
        **   code = previous;
        **   goto END_REPEAT;
        **   }

        However, that fails when a group or a subgroup within it is referenced
        as a subroutine from elsewhere in the pattern, so now we stick in
        OP_SKIPZERO in front of it so that it is skipped on execution. As we
        don't have a list of which groups are referenced, we cannot do this
        selectively.

        If the maximum is 1 or unlimited, we just have to stick in the BRAZERO
        and do no more at this point. */

        if (repeat_max <= 1)    /* Covers 0, 1, and unlimited */
          {
          memmove(previous + 1, previous, CU2BYTES(len));
          code++;
          if (repeat_max == 0)
            {
            *previous++ = OP_SKIPZERO;
            goto END_REPEAT;
            }
          brazeroptr = previous;    /* Save for possessive optimizing */
          *previous++ = OP_BRAZERO + repeat_type;
          }

        /* If the maximum is greater than 1 and limited, we have to replicate
        in a nested fashion, sticking OP_BRAZERO before each set of brackets.
        The first one has to be handled carefully because it's the original
        copy, which has to be moved up. The remainder can be handled by code
        that is common with the non-zero minimum case below. We have to
        adjust the value or repeat_max, since one less copy is required. */

        else
          {
          int offset;
          memmove(previous + 2 + LINK_SIZE, previous, CU2BYTES(len));
          code += 2 + LINK_SIZE;
          *previous++ = OP_BRAZERO + repeat_type;
          *previous++ = OP_BRA;

          /* We chain together the bracket offset fields that have to be
          filled in later when the ends of the brackets are reached. */

          offset = (bralink == NULL)? 0 : (int)(previous - bralink);
          bralink = previous;
          PUTINC(previous, 0, offset);
          }

        repeat_max--;
        }

      /* If the minimum is greater than zero, replicate the group as many
      times as necessary, and adjust the maximum to the number of subsequent
      copies that we need. */

      else
        {
        if (repeat_min > 1)
          {
          /* In the pre-compile phase, we don't actually do the replication. We
          just adjust the length as if we had. Do some paranoid checks for
          potential integer overflow. The INT64_OR_DOUBLE type is a 64-bit
          integer type when available, otherwise double. */

          if (lengthptr != NULL)
            {
            size_t delta = (repeat_min - 1)*length_prevgroup;
            if ((INT64_OR_DOUBLE)(repeat_min - 1)*
                  (INT64_OR_DOUBLE)length_prevgroup >
                    (INT64_OR_DOUBLE)INT_MAX ||
                OFLOW_MAX - *lengthptr < delta)
              {
              *errorcodeptr = ERR20;
              goto FAILED;
              }
            *lengthptr += delta;
            }

          /* This is compiling for real. If there is a set first byte for
          the group, and we have not yet set a "required byte", set it. */

          else
            {
            if (groupsetfirstcu && reqcuflags < 0)
              {
              reqcu = firstcu;
              reqcuflags = firstcuflags;
              }
            for (i = 1; i < repeat_min; i++)
              {
              memcpy(code, previous, CU2BYTES(len));
              code += len;
              }
            }
          }

        if (repeat_max > 0) repeat_max -= repeat_min;
        }

      /* This code is common to both the zero and non-zero minimum cases. If
      the maximum is limited, it replicates the group in a nested fashion,
      remembering the bracket starts on a stack. In the case of a zero minimum,
      the first one was set up above. In all cases the repeat_max now specifies
      the number of additional copies needed. Again, we must remember to
      replicate entries on the forward reference list. */

      if (repeat_max >= 0)
        {
        /* In the pre-compile phase, we don't actually do the replication. We
        just adjust the length as if we had. For each repetition we must add 1
        to the length for BRAZERO and for all but the last repetition we must
        add 2 + 2*LINKSIZE to allow for the nesting that occurs. Do some
        paranoid checks to avoid integer overflow. The INT64_OR_DOUBLE type is
        a 64-bit integer type when available, otherwise double. */

        if (lengthptr != NULL && repeat_max > 0)
          {
          size_t delta = repeat_max*(length_prevgroup + 1 + 2 + 2*LINK_SIZE) -
                      2 - 2*LINK_SIZE;   /* Last one doesn't nest */
          if ((INT64_OR_DOUBLE)repeat_max *
                (INT64_OR_DOUBLE)(length_prevgroup + 1 + 2 + 2*LINK_SIZE)
                  > (INT64_OR_DOUBLE)INT_MAX ||
              OFLOW_MAX - *lengthptr < delta)
            {
            *errorcodeptr = ERR20;
            goto FAILED;
            }
          *lengthptr += delta;
          }

        /* This is compiling for real */

        else for (i = repeat_max - 1; i >= 0; i--)
          {
          *code++ = OP_BRAZERO + repeat_type;

          /* All but the final copy start a new nesting, maintaining the
          chain of brackets outstanding. */

          if (i != 0)
            {
            int offset;
            *code++ = OP_BRA;
            offset = (bralink == NULL)? 0 : (int)(code - bralink);
            bralink = code;
            PUTINC(code, 0, offset);
            }

          memcpy(code, previous, CU2BYTES(len));
          code += len;
          }

        /* Now chain through the pending brackets, and fill in their length
        fields (which are holding the chain links pro tem). */

        while (bralink != NULL)
          {
          int oldlinkoffset;
          int offset = (int)(code - bralink + 1);
          PCRE2_UCHAR *bra = code - offset;
          oldlinkoffset = GET(bra, 1);
          bralink = (oldlinkoffset == 0)? NULL : bralink - oldlinkoffset;
          *code++ = OP_KET;
          PUTINC(code, 0, offset);
          PUT(bra, 1, offset);
          }
        }

      /* If the maximum is unlimited, set a repeater in the final copy. For
      ONCE brackets, that's all we need to do. However, possessively repeated
      ONCE brackets can be converted into non-capturing brackets, as the
      behaviour of (?:xx)++ is the same as (?>xx)++ and this saves having to
      deal with possessive ONCEs specially.

      Otherwise, when we are doing the actual compile phase, check to see
      whether this group is one that could match an empty string. If so,
      convert the initial operator to the S form (e.g. OP_BRA -> OP_SBRA) so
      that runtime checking can be done. [This check is also applied to ONCE
      groups at runtime, but in a different way.]

      Then, if the quantifier was possessive and the bracket is not a
      conditional, we convert the BRA code to the POS form, and the KET code to
      KETRPOS. (It turns out to be convenient at runtime to detect this kind of
      subpattern at both the start and at the end.) The use of special opcodes
      makes it possible to reduce greatly the stack usage in pcre2_match(). If
      the group is preceded by OP_BRAZERO, convert this to OP_BRAPOSZERO.

      Then, if the minimum number of matches is 1 or 0, cancel the possessive
      flag so that the default action below, of wrapping everything inside
      atomic brackets, does not happen. When the minimum is greater than 1,
      there will be earlier copies of the group, and so we still have to wrap
      the whole thing. */

      else
        {
        PCRE2_UCHAR *ketcode = code - 1 - LINK_SIZE;
        PCRE2_UCHAR *bracode = ketcode - GET(ketcode, 1);

        /* Convert possessive ONCE brackets to non-capturing */

        if ((*bracode == OP_ONCE || *bracode == OP_ONCE_NC) &&
            possessive_quantifier) *bracode = OP_BRA;

        /* For non-possessive ONCE brackets, all we need to do is to
        set the KET. */

        if (*bracode == OP_ONCE || *bracode == OP_ONCE_NC)
          *ketcode = OP_KETRMAX + repeat_type;

        /* Handle non-ONCE brackets and possessive ONCEs (which have been
        converted to non-capturing above). */

        else
          {
          /* In the compile phase, check whether the group could match an empty
          string. */

          if (lengthptr == NULL)
            {
            PCRE2_UCHAR *scode = bracode;
            do
              {
              int count = 0;
              int rc = could_be_empty_branch(scode, ketcode, utf, cb, FALSE,
                NULL, &count);
              if (rc < 0)
                {
                *errorcodeptr = ERR86;
                goto FAILED;
                }
              if (rc > 0)
                {
                *bracode += OP_SBRA - OP_BRA;
                break;
                }
              scode += GET(scode, 1);
              }
            while (*scode == OP_ALT);

            /* A conditional group with only one branch has an implicit empty
            alternative branch. */

            if (*bracode == OP_COND && bracode[GET(bracode,1)] != OP_ALT)
              *bracode = OP_SCOND;
            }

          /* Handle possessive quantifiers. */

          if (possessive_quantifier)
            {
            /* For COND brackets, we wrap the whole thing in a possessively
            repeated non-capturing bracket, because we have not invented POS
            versions of the COND opcodes. */

            if (*bracode == OP_COND || *bracode == OP_SCOND)
              {
              int nlen = (int)(code - bracode);
              memmove(bracode + 1 + LINK_SIZE, bracode, CU2BYTES(nlen));
              code += 1 + LINK_SIZE;
              nlen += 1 + LINK_SIZE;
              *bracode = (*bracode == OP_COND)? OP_BRAPOS : OP_SBRAPOS;
              *code++ = OP_KETRPOS;
              PUTINC(code, 0, nlen);
              PUT(bracode, 1, nlen);
              }

            /* For non-COND brackets, we modify the BRA code and use KETRPOS. */

            else
              {
              *bracode += 1;              /* Switch to xxxPOS opcodes */
              *ketcode = OP_KETRPOS;
              }

            /* If the minimum is zero, mark it as possessive, then unset the
            possessive flag when the minimum is 0 or 1. */

            if (brazeroptr != NULL) *brazeroptr = OP_BRAPOSZERO;
            if (repeat_min < 2) possessive_quantifier = FALSE;
            }

          /* Non-possessive quantifier */

          else *ketcode = OP_KETRMAX + repeat_type;
          }
        }
      }

    /* If previous is OP_FAIL, it was generated by an empty class []
    (PCRE2_ALLOW_EMPTY_CLASS is set). The other ways in which OP_FAIL can be
    generated, that is by (*FAIL) or (?!), set previous to NULL, which gives a
    "nothing to repeat" error above. We can just ignore the repeat in empty
    class case. */

    else if (*previous == OP_FAIL) goto END_REPEAT;

    /* Else there's some kind of shambles */

    else
      {
      *errorcodeptr = ERR10;
      goto FAILED;
      }

    /* If the character following a repeat is '+', possessive_quantifier is
    TRUE. For some opcodes, there are special alternative opcodes for this
    case. For anything else, we wrap the entire repeated item inside OP_ONCE
    brackets. Logically, the '+' notation is just syntactic sugar, taken from
    Sun's Java package, but the special opcodes can optimize it.

    Some (but not all) possessively repeated subpatterns have already been
    completely handled in the code just above. For them, possessive_quantifier
    is always FALSE at this stage. Note that the repeated item starts at
    tempcode, not at previous, which might be the first part of a string whose
    (former) last char we repeated. */

    if (possessive_quantifier)
      {
      int len;

      /* Possessifying an EXACT quantifier has no effect, so we can ignore it.
      However, QUERY, STAR, or UPTO may follow (for quantifiers such as {5,6},
      {5,}, or {5,10}). We skip over an EXACT item; if the length of what
      remains is greater than zero, there's a further opcode that can be
      handled. If not, do nothing, leaving the EXACT alone. */

      switch(*tempcode)
        {
        case OP_TYPEEXACT:
        tempcode += PRIV(OP_lengths)[*tempcode] +
          ((tempcode[1 + IMM2_SIZE] == OP_PROP
          || tempcode[1 + IMM2_SIZE] == OP_NOTPROP)? 2 : 0);
        break;

        /* CHAR opcodes are used for exacts whose count is 1. */

        case OP_CHAR:
        case OP_CHARI:
        case OP_NOT:
        case OP_NOTI:
        case OP_EXACT:
        case OP_EXACTI:
        case OP_NOTEXACT:
        case OP_NOTEXACTI:
        tempcode += PRIV(OP_lengths)[*tempcode];
#ifdef SUPPORT_UNICODE
        if (utf && HAS_EXTRALEN(tempcode[-1]))
          tempcode += GET_EXTRALEN(tempcode[-1]);
#endif
        break;

        /* For the class opcodes, the repeat operator appears at the end;
        adjust tempcode to point to it. */

        case OP_CLASS:
        case OP_NCLASS:
        tempcode += 1 + 32/sizeof(PCRE2_UCHAR);
        break;

#ifdef SUPPORT_WIDE_CHARS
        case OP_XCLASS:
        tempcode += GET(tempcode, 1);
        break;
#endif
        }

      /* If tempcode is equal to code (which points to the end of the repeated
      item), it means we have skipped an EXACT item but there is no following
      QUERY, STAR, or UPTO; the value of len will be 0, and we do nothing. In
      all other cases, tempcode will be pointing to the repeat opcode, and will
      be less than code, so the value of len will be greater than 0. */

      len = (int)(code - tempcode);
      if (len > 0)
        {
        unsigned int repcode = *tempcode;

        /* There is a table for possessifying opcodes, all of which are less
        than OP_CALLOUT. A zero entry means there is no possessified version.
        */

        if (repcode < OP_CALLOUT && opcode_possessify[repcode] > 0)
          *tempcode = opcode_possessify[repcode];

        /* For opcode without a special possessified version, wrap the item in
        ONCE brackets. */

        else
          {
          memmove(tempcode + 1 + LINK_SIZE, tempcode, CU2BYTES(len));
          code += 1 + LINK_SIZE;
          len += 1 + LINK_SIZE;
          tempcode[0] = OP_ONCE;
          *code++ = OP_KET;
          PUTINC(code, 0, len);
          PUT(tempcode, 1, len);
          }
        }
      }

    /* In all case we no longer have a previous item. We also set the
    "follows varying string" flag for subsequently encountered reqcus if
    it isn't already set and we have just passed a varying length item. */

    END_REPEAT:
    previous = NULL;
    cb->req_varyopt |= reqvary;
    break;


    /* ===================================================================*/
    /* Start of nested parenthesized sub-expression, or lookahead or lookbehind
    or option setting or condition or all the other extended parenthesis forms.
    We must save the current high-water-mark for the forward reference list so
    that we know where they start for this group. However, because the list may
    be extended when there are very many forward references (usually the result
    of a replicated inner group), we must use an offset rather than an absolute
    address. Note that (?# comments are dealt with at the top of the loop;
    they do not get this far. */

    case CHAR_LEFT_PARENTHESIS:
    ptr++;

    /* Deal with various "verbs" that can be introduced by '*'. */

    if (ptr[0] == CHAR_ASTERISK && (ptr[1] == ':'
         || (MAX_255(ptr[1]) && ((cb->ctypes[ptr[1]] & ctype_letter) != 0))))
      {
      int i, namelen;
      int arglen = 0;
      const char *vn = verbnames;
      PCRE2_SPTR name = ptr + 1;
      PCRE2_SPTR arg = NULL;
      previous = NULL;
      ptr++;

      /* Increment ptr, set namelen, check length */

      READ_NAME(ctype_letter, ERR60, *errorcodeptr);

      /* It appears that Perl allows any characters whatsoever, other than
      a closing parenthesis, to appear in arguments, so we no longer insist on
      letters, digits, and underscores. Perl does not, however, do any
      interpretation within arguments, and has no means of including a closing
      parenthesis. PCRE supports escape processing but only when it is
      requested by an option. Note that check_escape() will not return values
      greater than the code unit maximum when not in UTF mode. */

      if (*ptr == CHAR_COLON)
        {
        arg = ++ptr;

        if ((options & PCRE2_ALT_VERBNAMES) == 0)
          {
          arglen = 0;
          while (ptr < cb->end_pattern && *ptr != CHAR_RIGHT_PARENTHESIS)
            {
            ptr++;                                /* Check length as we go */
            arglen++;                             /* along, to avoid the   */
            if ((unsigned int)arglen > MAX_MARK)  /* possibility of overflow. */
              {
              *errorcodeptr = ERR76;
              goto FAILED;
              }
            }
          }
        else
          {
          /* The length check is in process_verb_names() */
          arglen = process_verb_name(&ptr, NULL, errorcodeptr, options,
            utf, cb);
          if (arglen < 0) goto FAILED;
          }
        }

      if (*ptr != CHAR_RIGHT_PARENTHESIS)
        {
        *errorcodeptr = ERR60;
        goto FAILED;
        }

      /* Scan the table of verb names */

      for (i = 0; i < verbcount; i++)
        {
        if (namelen == verbs[i].len &&
            PRIV(strncmp_c8)(name, vn, namelen) == 0)
          {
          int setverb;

          /* Check for open captures before ACCEPT and convert it to
          ASSERT_ACCEPT if in an assertion. */

          if (verbs[i].op == OP_ACCEPT)
            {
            open_capitem *oc;
            if (arglen != 0)
              {
              *errorcodeptr = ERR59;
              goto FAILED;
              }
            cb->had_accept = TRUE;

            /* In the first pass, just accumulate the length required;
            otherwise hitting (*ACCEPT) inside many nested parentheses can
            cause workspace overflow. */

            for (oc = cb->open_caps; oc != NULL; oc = oc->next)
              {
              if (lengthptr != NULL)
                {
                *lengthptr += CU2BYTES(1) + IMM2_SIZE;
                }
              else
                {
                *code++ = OP_CLOSE;
                PUT2INC(code, 0, oc->number);
                }
              }
            setverb = *code++ =
              (cb->assert_depth > 0)? OP_ASSERT_ACCEPT : OP_ACCEPT;

            /* Do not set firstcu after *ACCEPT */
            if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
            }

          /* Handle other cases with/without an argument */

          else if (arglen == 0)    /* There is no argument */
            {
            if (verbs[i].op < 0)   /* Argument is mandatory */
              {
              *errorcodeptr = ERR66;
              goto FAILED;
              }
            setverb = *code++ = verbs[i].op;
            }

          else                        /* An argument is present */
            {
            if (verbs[i].op_arg < 0)  /* Argument is forbidden */
              {
              *errorcodeptr = ERR59;
              goto FAILED;
              }
            setverb = *code++ = verbs[i].op_arg;

            /* Arguments can be very long, especially in 16- and 32-bit modes,
            and can overflow the workspace in the first pass. Instead of
            putting the argument into memory, we just update the length counter
            and set up an empty argument. */

            if (lengthptr != NULL)
              {
              *lengthptr += arglen;
              *code++ = 0;
              }
            else
              {
              *code++ = arglen;
              if ((options & PCRE2_ALT_VERBNAMES) != 0)
                {
                PCRE2_UCHAR *memcode = code;  /* code is "register" */
                (void)process_verb_name(&arg, &memcode, errorcodeptr, options,
                  utf, cb);
                code = memcode;
                }
              else   /* No argument processing */
                {
                memcpy(code, arg, CU2BYTES(arglen));
                code += arglen;
                }
              }

            *code++ = 0;
            }

          switch (setverb)
            {
            case OP_THEN:
            case OP_THEN_ARG:
            cb->external_flags |= PCRE2_HASTHEN;
            break;

            case OP_PRUNE:
            case OP_PRUNE_ARG:
            case OP_SKIP:
            case OP_SKIP_ARG:
            cb->had_pruneorskip = TRUE;
            break;
            }

          break;  /* Found verb, exit loop */
          }

        vn += verbs[i].len + 1;
        }

      if (i < verbcount) continue;    /* Successfully handled a verb */
      *errorcodeptr = ERR60;          /* Verb not recognized */
      goto FAILED;
      }

    /* Initialization for "real" parentheses */

    newoptions = options;
    skipunits = 0;
    bravalue = OP_CBRA;
    reset_bracount = FALSE;

    /* Deal with the extended parentheses; all are introduced by '?', and the
    appearance of any of them means that this is not a capturing group. */

    if (*ptr == CHAR_QUESTION_MARK)
      {
      int i, count;
      int namelen;                /* Must be signed */
      uint32_t index;
      uint32_t set, unset, *optset;
      named_group *ng;
      PCRE2_SPTR name;
      PCRE2_UCHAR *slot;

      switch (*(++ptr))
        {
        /* ------------------------------------------------------------ */
        case CHAR_VERTICAL_LINE:  /* Reset capture count for each branch */
        reset_bracount = TRUE;
        /* Fall through */

        /* ------------------------------------------------------------ */
        case CHAR_COLON:          /* Non-capturing bracket */
        bravalue = OP_BRA;
        ptr++;
        break;

        /* ------------------------------------------------------------ */
        case CHAR_LEFT_PARENTHESIS:
        bravalue = OP_COND;       /* Conditional group */
        tempptr = ptr;

        /* A condition can be an assertion, a number (referring to a numbered
        group's having been set), a name (referring to a named group), or 'R',
        referring to recursion. R<digits> and R&name are also permitted for
        recursion tests.

        There are ways of testing a named group: (?(name)) is used by Python;
        Perl 5.10 onwards uses (?(<name>) or (?('name')).

        There is one unfortunate ambiguity, caused by history. 'R' can be the
        recursive thing or the name 'R' (and similarly for 'R' followed by
        digits). We look for a name first; if not found, we try the other case.

        For compatibility with auto-callouts, we allow a callout to be
        specified before a condition that is an assertion. First, check for the
        syntax of a callout; if found, adjust the temporary pointer that is
        used to check for an assertion condition. That's all that is needed! */

        if (ptr[1] == CHAR_QUESTION_MARK && ptr[2] == CHAR_C)
          {
          if (IS_DIGIT(ptr[3]) || ptr[3] == CHAR_RIGHT_PARENTHESIS)
            {
            for (i = 3;; i++) if (!IS_DIGIT(ptr[i])) break;
            if (ptr[i] == CHAR_RIGHT_PARENTHESIS)
              tempptr += i + 1;
            }
          else
            {
            uint32_t delimiter = 0;
            for (i = 0; PRIV(callout_start_delims)[i] != 0; i++)
              {
              if (ptr[3] == PRIV(callout_start_delims)[i])
                {
                delimiter = PRIV(callout_end_delims)[i];
                break;
                }
              }
            if (delimiter != 0)
              {
              for (i = 4; ptr + i < cb->end_pattern; i++)
                {
                if (ptr[i] == delimiter)
                  {
                  if (ptr[i+1] == delimiter) i++;
                  else
                    {
                    if (ptr[i+1] == CHAR_RIGHT_PARENTHESIS) tempptr += i + 2;
                    break;
                    }
                  }
                }
              }
            }

          /* tempptr should now be pointing to the opening parenthesis of the
          assertion condition. */

          if (*tempptr != CHAR_LEFT_PARENTHESIS)
            {
            *errorcodeptr = ERR28;
            goto FAILED;
            }
          }

        /* For conditions that are assertions, check the syntax, and then exit
        the switch. This will take control down to where bracketed groups
        are processed. The assertion will be handled as part of the group,
        but we need to identify this case because the conditional assertion may
        not be quantifier. */

        if (tempptr[1] == CHAR_QUESTION_MARK &&
              (tempptr[2] == CHAR_EQUALS_SIGN ||
               tempptr[2] == CHAR_EXCLAMATION_MARK ||
                 (tempptr[2] == CHAR_LESS_THAN_SIGN &&
                   (tempptr[3] == CHAR_EQUALS_SIGN ||
                    tempptr[3] == CHAR_EXCLAMATION_MARK))))
          {
          cb->iscondassert = TRUE;
          break;
          }

        /* Other conditions use OP_CREF/OP_DNCREF/OP_RREF/OP_DNRREF, and all
        need to skip at least 1+IMM2_SIZE bytes at the start of the group. */

        code[1+LINK_SIZE] = OP_CREF;
        skipunits = 1+IMM2_SIZE;
        refsign = -1;     /* => not a number */
        namelen = -1;     /* => not a name; must set to avoid warning */
        name = NULL;      /* Always set to avoid warning */
        recno = 0;        /* Always set to avoid warning */

        /* Point at character after (?( */

        ptr++;

        /* Check for (?(VERSION[>]=n.m), which is a facility whereby indirect
        users of PCRE2 via an application can discover which release of PCRE2
        is being used. */

        if (PRIV(strncmp_c8)(ptr, STRING_VERSION, 7) == 0 &&
            ptr[7] != CHAR_RIGHT_PARENTHESIS)
          {
          BOOL ge = FALSE;
          int major = 0;
          int minor = 0;

          ptr += 7;
          if (*ptr == CHAR_GREATER_THAN_SIGN)
            {
            ge = TRUE;
            ptr++;
            }

          /* NOTE: cannot write IS_DIGIT(*(++ptr)) here because IS_DIGIT
          references its argument twice. */

          if (*ptr != CHAR_EQUALS_SIGN || (ptr++, !IS_DIGIT(*ptr)))
            {
            *errorcodeptr = ERR79;
            goto FAILED;
            }

          while (IS_DIGIT(*ptr)) major = major * 10 + *ptr++ - '0';
          if (*ptr == CHAR_DOT)
            {
            ptr++;
            while (IS_DIGIT(*ptr)) minor = minor * 10 + *ptr++ - '0';
            if (minor < 10) minor *= 10;
            }

          if (*ptr != CHAR_RIGHT_PARENTHESIS || minor > 99)
            {
            *errorcodeptr = ERR79;
            goto FAILED;
            }

          if (ge)
            code[1+LINK_SIZE] = ((PCRE2_MAJOR > major) ||
              (PCRE2_MAJOR == major && PCRE2_MINOR >= minor))?
                OP_TRUE : OP_FALSE;
          else
            code[1+LINK_SIZE] = (PCRE2_MAJOR == major && PCRE2_MINOR == minor)?
              OP_TRUE : OP_FALSE;

          ptr++;
          skipunits = 1;
          break;  /* End of condition processing */
          }

        /* Check for a test for recursion in a named group. */

        if (*ptr == CHAR_R && ptr[1] == CHAR_AMPERSAND)
          {
          terminator = -1;
          ptr += 2;
          code[1+LINK_SIZE] = OP_RREF;    /* Change the type of test */
          }

        /* Check for a test for a named group's having been set, using the Perl
        syntax (?(<name>) or (?('name'), and also allow for the original PCRE
        syntax of (?(name) or for (?(+n), (?(-n), and just (?(n). */

        else if (*ptr == CHAR_LESS_THAN_SIGN)
          {
          terminator = CHAR_GREATER_THAN_SIGN;
          ptr++;
          }
        else if (*ptr == CHAR_APOSTROPHE)
          {
          terminator = CHAR_APOSTROPHE;
          ptr++;
          }
        else
          {
          terminator = CHAR_NULL;
          if (*ptr == CHAR_MINUS || *ptr == CHAR_PLUS) refsign = *ptr++;
            else if (IS_DIGIT(*ptr)) refsign = 0;
          }

        /* Handle a number */

        if (refsign >= 0)
          {
          while (IS_DIGIT(*ptr))
            {
            if (recno > INT_MAX / 10 - 1)  /* Integer overflow */
              {
              while (IS_DIGIT(*ptr)) ptr++;
              *errorcodeptr = ERR61;
              goto FAILED;
              }
            recno = recno * 10 + (int)(*ptr - CHAR_0);
            ptr++;
            }
          }

        /* Otherwise we expect to read a name; anything else is an error. When
        the referenced name is one of a number of duplicates, a different
        opcode is used and it needs more memory. Unfortunately we cannot tell
        whether this is the case in the first pass, so we have to allow for
        more memory always. In the second pass, the additional to skipunits
        happens later. */

        else
          {
          if (IS_DIGIT(*ptr))
            {
            *errorcodeptr = ERR44;  /* Group name must start with non-digit */
            goto FAILED;
            }
          if (!MAX_255(*ptr) || (cb->ctypes[*ptr] & ctype_word) == 0)
            {
            *errorcodeptr = ERR28;   /* Assertion expected */
            goto FAILED;
            }
          name = ptr;
          /* Increment ptr, set namelen, check length */
          READ_NAME(ctype_word, ERR48, *errorcodeptr);
          if (lengthptr != NULL) skipunits += IMM2_SIZE;
          }

        /* Check the terminator */

        if ((terminator > 0 && *ptr++ != (PCRE2_UCHAR)terminator) ||
            *ptr++ != CHAR_RIGHT_PARENTHESIS)
          {
          ptr--;                  /* Error offset */
          *errorcodeptr = ERR26;  /* Malformed number or name */
          goto FAILED;
          }

        /* Do no further checking in the pre-compile phase. */

        if (lengthptr != NULL) break;

        /* In the real compile we do the work of looking for the actual
        reference. If refsign is not negative, it means we have a number in
        recno. */

        if (refsign >= 0)
          {
          if (recno <= 0)
            {
            *errorcodeptr = ERR35;
            goto FAILED;
            }
          if (refsign != 0) recno = (refsign == CHAR_MINUS)?
            (cb->bracount + 1) - recno : recno + cb->bracount;
          if (recno <= 0 || (uint32_t)recno > cb->final_bracount)
            {
            *errorcodeptr = ERR15;
            goto FAILED;
            }
          PUT2(code, 2+LINK_SIZE, recno);
          if ((uint32_t)recno > cb->top_backref) cb->top_backref = recno;
          break;
          }

        /* Otherwise look for the name. */

        slot = cb->name_table;
        for (i = 0; i < cb->names_found; i++)
          {
          if (PRIV(strncmp)(name, slot+IMM2_SIZE, namelen) == 0) break;
          slot += cb->name_entry_size;
          }

        /* Found the named subpattern. If the name is duplicated, add one to
        the opcode to change CREF/RREF into DNCREF/DNRREF and insert
        appropriate data values. Otherwise, just insert the unique subpattern
        number. */

        if (i < cb->names_found)
          {
          int offset = i;            /* Offset of first name found */

          count = 0;
          for (;;)
            {
            recno = GET2(slot, 0);   /* Number for last found */
            if ((uint32_t)recno > cb->top_backref) cb->top_backref = recno;
            count++;
            if (++i >= cb->names_found) break;
            slot += cb->name_entry_size;
            if (PRIV(strncmp)(name, slot+IMM2_SIZE, namelen) != 0 ||
              (slot+IMM2_SIZE)[namelen] != 0) break;
            }

          if (count > 1)
            {
            PUT2(code, 2+LINK_SIZE, offset);
            PUT2(code, 2+LINK_SIZE+IMM2_SIZE, count);
            skipunits += IMM2_SIZE;
            code[1+LINK_SIZE]++;
            }
          else  /* Not a duplicated name */
            {
            PUT2(code, 2+LINK_SIZE, recno);
            }
          }

        /* If terminator == CHAR_NULL it means that the name followed directly
        after the opening parenthesis [e.g. (?(abc)...] and in this case there
        are some further alternatives to try. For the cases where terminator !=
        CHAR_NULL [things like (?(<name>... or (?('name')... or (?(R&name)... ]
        we have now checked all the possibilities, so give an error. */

        else if (terminator != CHAR_NULL)
          {
          *errorcodeptr = ERR15;
          goto FAILED;
          }

        /* Check for (?(R) for recursion. Allow digits after R to specify a
        specific group number. */

        else if (*name == CHAR_R)
          {
          recno = 0;
          for (i = 1; i < namelen; i++)
            {
            if (!IS_DIGIT(name[i]))
              {
              *errorcodeptr = ERR15;        /* Non-existent subpattern */
              goto FAILED;
              }
            if (recno > INT_MAX / 10 - 1)   /* Integer overflow */
              {
              *errorcodeptr = ERR61;
              goto FAILED;
              }
            recno = recno * 10 + name[i] - CHAR_0;
            }
          if (recno == 0) recno = RREF_ANY;
          code[1+LINK_SIZE] = OP_RREF;      /* Change test type */
          PUT2(code, 2+LINK_SIZE, recno);
          }

        /* Similarly, check for the (?(DEFINE) "condition", which is always
        false. During compilation we set OP_DEFINE to distinguish this from
        other OP_FALSE conditions so that it can be checked for having only one
        branch, but after that the opcode is changed to OP_FALSE. */

        else if (namelen == 6 && PRIV(strncmp_c8)(name, STRING_DEFINE, 6) == 0)
          {
          code[1+LINK_SIZE] = OP_DEFINE;
          skipunits = 1;
          }

        /* Reference to an unidentified subpattern. */

        else
          {
          *errorcodeptr = ERR15;
          goto FAILED;
          }
        break;


        /* ------------------------------------------------------------ */
        case CHAR_EQUALS_SIGN:                 /* Positive lookahead */
        bravalue = OP_ASSERT;
        cb->assert_depth += 1;
        ptr++;
        break;

        /* Optimize (?!) to (*FAIL) unless it is quantified - which is a weird
        thing to do, but Perl allows all assertions to be quantified, and when
        they contain capturing parentheses there may be a potential use for
        this feature. Not that that applies to a quantified (?!) but we allow
        it for uniformity. */

        /* ------------------------------------------------------------ */
        case CHAR_EXCLAMATION_MARK:            /* Negative lookahead */
        ptr++;
        if (*ptr == CHAR_RIGHT_PARENTHESIS && ptr[1] != CHAR_ASTERISK &&
             ptr[1] != CHAR_PLUS && ptr[1] != CHAR_QUESTION_MARK &&
            (ptr[1] != CHAR_LEFT_CURLY_BRACKET || !is_counted_repeat(ptr+2)))
          {
          *code++ = OP_FAIL;
          previous = NULL;
          continue;
          }
        bravalue = OP_ASSERT_NOT;
        cb->assert_depth += 1;
        break;


        /* ------------------------------------------------------------ */
        case CHAR_LESS_THAN_SIGN:              /* Lookbehind or named define */
        switch (ptr[1])
          {
          case CHAR_EQUALS_SIGN:               /* Positive lookbehind */
          bravalue = OP_ASSERTBACK;
          cb->assert_depth += 1;
          ptr += 2;
          break;

          case CHAR_EXCLAMATION_MARK:          /* Negative lookbehind */
          bravalue = OP_ASSERTBACK_NOT;
          cb->assert_depth += 1;
          ptr += 2;
          break;

          /* Must be a name definition - as the syntax was checked in the
          pre-pass, we can assume here that it is valid. Skip over the name
          and go to handle the numbered group. */

          default:
          while (*(++ptr) != CHAR_GREATER_THAN_SIGN);
          ptr++;
          goto NUMBERED_GROUP;
          }
        break;


        /* ------------------------------------------------------------ */
        case CHAR_GREATER_THAN_SIGN:           /* One-time brackets */
        bravalue = OP_ONCE;
        ptr++;
        break;


        /* ------------------------------------------------------------ */
        case CHAR_C:                 /* Callout */
        previous_callout = code;     /* Save for later completion */
        after_manual_callout = 1;    /* Skip one item before completing */
        ptr++;                       /* Character after (?C */

        /* A callout may have a string argument, delimited by one of a fixed
        number of characters, or an undelimited numerical argument, or no
        argument, which is the same as (?C0). Different opcodes are used for
        the two cases. */

        if (*ptr != CHAR_RIGHT_PARENTHESIS && !IS_DIGIT(*ptr))
          {
          uint32_t delimiter = 0;

          for (i = 0; PRIV(callout_start_delims)[i] != 0; i++)
            {
            if (*ptr == PRIV(callout_start_delims)[i])
              {
              delimiter = PRIV(callout_end_delims)[i];
              break;
              }
            }

          if (delimiter == 0)
            {
            *errorcodeptr = ERR82;
            goto FAILED;
            }

          /* During the pre-compile phase, we parse the string and update the
          length. There is no need to generate any code. (In fact, the string
          has already been parsed in the pre-pass that looks for named
          parentheses, but it does no harm to leave this code in.) */

          if (lengthptr != NULL)     /* Only check the string */
            {
            PCRE2_SPTR start = ptr;
            do
              {
              if (++ptr >= cb->end_pattern)
                {
                *errorcodeptr = ERR81;
                ptr = start;   /* To give a more useful message */
                goto FAILED;
                }
              if (ptr[0] == delimiter && ptr[1] == delimiter) ptr += 2;
              }
            while (ptr[0] != delimiter);

            /* Start points to the opening delimiter, ptr points to the
            closing delimiter. We must allow for including the delimiter and
            for the terminating zero. Any doubled delimiters within the string
            make this an overestimate, but it is not worth bothering about. */

            (*lengthptr) += (ptr - start) + 2 + (1 + 4*LINK_SIZE);
            }

          /* In the real compile we can copy the string, knowing that it is
          syntactically OK. The starting delimiter is included so that the
          client can discover it if they want. We also pass the start offset to
          help a script language give better error messages. */

          else
            {
            PCRE2_UCHAR *callout_string = code + (1 + 4*LINK_SIZE);
            *callout_string++ = *ptr++;
            PUT(code, 1 + 3*LINK_SIZE, (int)(ptr - cb->start_pattern)); /* Start offset */
            for(;;)
              {
              if (*ptr == delimiter)
                {
                if (ptr[1] == delimiter) ptr++; else break;
                }
              *callout_string++ = *ptr++;
              }
            *callout_string++ = CHAR_NULL;
            code[0] = OP_CALLOUT_STR;
            PUT(code, 1, (int)(ptr + 2 - cb->start_pattern)); /* Next offset */
            PUT(code, 1 + LINK_SIZE, 0);      /* Default length */
            PUT(code, 1 + 2*LINK_SIZE,        /* Compute size */
                (int)(callout_string - code));
            code = callout_string;
            }

          /* Advance to what should be the closing parenthesis, which is
          checked below. */

          ptr++;
          }

        /* Handle a callout with an optional numerical argument, which must be
        less than or equal to 255. A missing argument gives 0. */

        else
          {
          int n = 0;
          code[0] = OP_CALLOUT;     /* Numerical callout */
          while (IS_DIGIT(*ptr))
            {
            n = n * 10 + *ptr++ - CHAR_0;
            if (n > 255)
              {
              *errorcodeptr = ERR38;
              goto FAILED;
              }
            }
          PUT(code, 1, (int)(ptr - cb->start_pattern + 1));  /* Next offset */
          PUT(code, 1 + LINK_SIZE, 0);                    /* Default length */
          code[1 + 2*LINK_SIZE] = n;                      /* Callout number */
          code += PRIV(OP_lengths)[OP_CALLOUT];
          }

        /* Both formats must have a closing parenthesis */

        if (*ptr != CHAR_RIGHT_PARENTHESIS)
          {
          *errorcodeptr = ERR39;
          goto FAILED;
          }

        /* Callouts cannot be quantified. */

        previous = NULL;
        continue;


        /* ------------------------------------------------------------ */
        case CHAR_P:              /* Python-style named subpattern handling */
        if (*(++ptr) == CHAR_EQUALS_SIGN ||
            *ptr == CHAR_GREATER_THAN_SIGN)  /* Reference or recursion */
          {
          is_recurse = *ptr == CHAR_GREATER_THAN_SIGN;
          terminator = CHAR_RIGHT_PARENTHESIS;
          goto NAMED_REF_OR_RECURSE;
          }
        else if (*ptr != CHAR_LESS_THAN_SIGN)  /* Test for Python-style defn */
          {
          *errorcodeptr = ERR41;
          goto FAILED;
          }
        /* Fall through to handle (?P< as (?< is handled */


        /* ------------------------------------------------------------ */
        case CHAR_APOSTROPHE:   /* Define a name - note fall through above */

        /* The syntax was checked and the list of names was set up in the
        pre-pass, so there is nothing to be done now except to skip over the
        name. */

        terminator = (*ptr == CHAR_LESS_THAN_SIGN)?
                  CHAR_GREATER_THAN_SIGN : CHAR_APOSTROPHE;
        while (*(++ptr) != (unsigned int)terminator);
        ptr++;
        goto NUMBERED_GROUP;      /* Set up numbered group */


        /* ------------------------------------------------------------ */
        case CHAR_AMPERSAND:            /* Perl recursion/subroutine syntax */
        terminator = CHAR_RIGHT_PARENTHESIS;
        is_recurse = TRUE;
        /* Fall through */

        /* We come here from the Python syntax above that handles both
        references (?P=name) and recursion (?P>name), as well as falling
        through from the Perl recursion syntax (?&name). We also come here from
        the Perl \k<name> or \k'name' back reference syntax and the \k{name}
        .NET syntax, and the Oniguruma \g<...> and \g'...' subroutine syntax. */

        NAMED_REF_OR_RECURSE:
        name = ++ptr;
        if (IS_DIGIT(*ptr))
          {
          *errorcodeptr = ERR44;   /* Group name must start with non-digit */
          goto FAILED;
          }
        /* Increment ptr, set namelen, check length */
        READ_NAME(ctype_word, ERR48, *errorcodeptr);

        /* In the pre-compile phase, do a syntax check. */

        if (lengthptr != NULL)
          {
          if (namelen == 0)
            {
            *errorcodeptr = ERR62;
            goto FAILED;
            }
          if (*ptr != (PCRE2_UCHAR)terminator)
            {
            *errorcodeptr = ERR42;
            goto FAILED;
            }
          }

        /* Scan the list of names generated in the pre-pass in order to get
        a number and whether or not this name is duplicated. */

        recno = 0;
        is_dupname = FALSE;
        ng = cb->named_groups;

        for (i = 0; i < cb->names_found; i++, ng++)
          {
          if (namelen == ng->length &&
              PRIV(strncmp)(name, ng->name, namelen) == 0)
            {
            open_capitem *oc;
            is_dupname = ng->isdup;
            recno = ng->number;

            /* For a recursion, that's all that is needed. We can now go to the
            code that handles numerical recursion. */

            if (is_recurse) goto HANDLE_RECURSION;

            /* For a back reference, update the back reference map and the
            maximum back reference. Then for each group we must check to see if
            it is recursive, that is, it is inside the group that it
            references. A flag is set so that the group can be made atomic. */

            cb->backref_map |= (recno < 32)? (1u << recno) : 1;
            if ((uint32_t)recno > cb->top_backref) cb->top_backref = recno;

            for (oc = cb->open_caps; oc != NULL; oc = oc->next)
              {
              if (oc->number == recno)
                {
                oc->flag = TRUE;
                break;
                }
              }
            }
          }

        /* If the name was not found we have a bad reference. */

        if (recno == 0)
          {
          *errorcodeptr = ERR15;
          goto FAILED;
          }

        /* If a back reference name is not duplicated, we can handle it as a
        numerical reference. */

        if (!is_dupname) goto HANDLE_REFERENCE;

        /* If a back reference name is duplicated, we generate a different
        opcode to a numerical back reference. In the second pass we must search
        for the index and count in the final name table. */

        count = 0;
        index = 0;

        if (lengthptr == NULL)
          {
          slot = cb->name_table;
          for (i = 0; i < cb->names_found; i++)
            {
            if (PRIV(strncmp)(name, slot+IMM2_SIZE, namelen) == 0 &&
                slot[IMM2_SIZE+namelen] == 0)
              {
              if (count == 0) index = i;
              count++;
              }
            slot += cb->name_entry_size;
            }

          if (count == 0)
            {
            *errorcodeptr = ERR15;
            goto FAILED;
            }
          }

        if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
        previous = code;
        *code++ = ((options & PCRE2_CASELESS) != 0)? OP_DNREFI : OP_DNREF;
        PUT2INC(code, 0, index);
        PUT2INC(code, 0, count);
        continue;  /* End of back ref handling */


        /* ------------------------------------------------------------ */
        case CHAR_R:              /* Recursion, same as (?0) */
        recno = 0;
        if (*(++ptr) != CHAR_RIGHT_PARENTHESIS)
          {
          *errorcodeptr = ERR29;
          goto FAILED;
          }
        goto HANDLE_RECURSION;


        /* ------------------------------------------------------------ */
        case CHAR_MINUS: case CHAR_PLUS:  /* Recursion or subroutine */
        case CHAR_0: case CHAR_1: case CHAR_2: case CHAR_3: case CHAR_4:
        case CHAR_5: case CHAR_6: case CHAR_7: case CHAR_8: case CHAR_9:
          {
          terminator = CHAR_RIGHT_PARENTHESIS;

          /* Come here from the \g<...> and \g'...' code (Oniguruma
          compatibility). However, the syntax has been checked to ensure that
          the ... are a (signed) number, so that neither ERR63 nor ERR29 will
          be called on this path, nor with the jump to OTHER_CHAR_AFTER_QUERY
          ever be taken. */

          HANDLE_NUMERICAL_RECURSION:

          if ((refsign = *ptr) == CHAR_PLUS)
            {
            ptr++;
            if (!IS_DIGIT(*ptr))
              {
              *errorcodeptr = ERR63;
              goto FAILED;
              }
            }
          else if (refsign == CHAR_MINUS)
            {
            if (!IS_DIGIT(ptr[1]))
              goto OTHER_CHAR_AFTER_QUERY;
            ptr++;
            }

          recno = 0;
          while (IS_DIGIT(*ptr))
            {
            if (recno > INT_MAX / 10 - 1) /* Integer overflow */
              {
              while (IS_DIGIT(*ptr)) ptr++;
              *errorcodeptr = ERR61;
              goto FAILED;
              }
            recno = recno * 10 + *ptr++ - CHAR_0;
            }

          if (*ptr != (PCRE2_UCHAR)terminator)
            {
            *errorcodeptr = ERR29;
            goto FAILED;
            }

          if (refsign == CHAR_MINUS)
            {
            if (recno == 0)
              {
              *errorcodeptr = ERR58;
              goto FAILED;
              }
            recno = (int)(cb->bracount + 1) - recno;
            if (recno <= 0)
              {
              *errorcodeptr = ERR15;
              goto FAILED;
              }
            }
          else if (refsign == CHAR_PLUS)
            {
            if (recno == 0)
              {
              *errorcodeptr = ERR58;
              goto FAILED;
              }
            recno += cb->bracount;
            }

          if ((uint32_t)recno > cb->final_bracount)
            {
            *errorcodeptr = ERR15;
            goto FAILED;
            }

          /* Come here from code above that handles a named recursion.
          We insert the number of the called group after OP_RECURSE. At the
          end of compiling the pattern is scanned and these numbers are
          replaced by offsets within the pattern. It is done like this to avoid
          problems with forward references and adjusting offsets when groups
          are duplicated and moved (as discovered in previous implementations).
          Note that a recursion does not have a set first character (relevant
          if it is repeated, because it will then be wrapped with ONCE
          brackets). */

          HANDLE_RECURSION:
          previous = code;
          *code = OP_RECURSE;
          PUT(code, 1, recno);
          code += 1 + LINK_SIZE;
          groupsetfirstcu = FALSE;
          cb->had_recurse = TRUE;
          }

        /* Can't determine a first byte now */

        if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
        continue;


        /* ------------------------------------------------------------ */
        default:              /* Other characters: check option setting */
        OTHER_CHAR_AFTER_QUERY:
        set = unset = 0;
        optset = &set;

        while (*ptr != CHAR_RIGHT_PARENTHESIS && *ptr != CHAR_COLON)
          {
          switch (*ptr++)
            {
            case CHAR_MINUS: optset = &unset; break;

            case CHAR_J:    /* Record that it changed in the external options */
            *optset |= PCRE2_DUPNAMES;
            cb->external_flags |= PCRE2_JCHANGED;
            break;

            case CHAR_i: *optset |= PCRE2_CASELESS; break;
            case CHAR_m: *optset |= PCRE2_MULTILINE; break;
            case CHAR_s: *optset |= PCRE2_DOTALL; break;
            case CHAR_x: *optset |= PCRE2_EXTENDED; break;
            case CHAR_U: *optset |= PCRE2_UNGREEDY; break;

            default:  *errorcodeptr = ERR11;
                      ptr--;    /* Correct the offset */
                      goto FAILED;
            }
          }

        /* Set up the changed option bits, but don't change anything yet. */

        newoptions = (options | set) & (~unset);

        /* If the options ended with ')' this is not the start of a nested
        group with option changes, so the options change at this level. They
        must also be passed back for use in subsequent branches. Reset the
        greedy defaults and the case value for firstcu and reqcu. */

        if (*ptr == CHAR_RIGHT_PARENTHESIS)
          {
          *optionsptr = options = newoptions;
          greedy_default = ((newoptions & PCRE2_UNGREEDY) != 0);
          greedy_non_default = greedy_default ^ 1;
          req_caseopt = ((newoptions & PCRE2_CASELESS) != 0)? REQ_CASELESS:0;
          previous = NULL;       /* This item can't be repeated */
          continue;              /* It is complete */
          }

        /* If the options ended with ':' we are heading into a nested group
        with possible change of options. Such groups are non-capturing and are
        not assertions of any kind. All we need to do is skip over the ':';
        the newoptions value is handled below. */

        bravalue = OP_BRA;
        ptr++;
        }     /* End of switch for character following (? */
      }       /* End of (? handling */

    /* Opening parenthesis not followed by '*' or '?'. If PCRE2_NO_AUTO_CAPTURE
    is set, all unadorned brackets become non-capturing and behave like (?:...)
    brackets. */

    else if ((options & PCRE2_NO_AUTO_CAPTURE) != 0)
      {
      bravalue = OP_BRA;
      }

    /* Else we have a capturing group. */

    else
      {
      NUMBERED_GROUP:
      cb->bracount += 1;
      PUT2(code, 1+LINK_SIZE, cb->bracount);
      skipunits = IMM2_SIZE;
      }

    /* Process nested bracketed regex. First check for parentheses nested too
    deeply. */

    if ((cb->parens_depth += 1) > (int)(cb->cx->parens_nest_limit))
      {
      *errorcodeptr = ERR19;
      goto FAILED;
      }

    /* All assertions used not to be repeatable, but this was changed for Perl
    compatibility. All kinds can now be repeated except for assertions that are
    conditions (Perl also forbids these to be repeated). We copy code into a
    non-register variable (tempcode) in order to be able to pass its address
    because some compilers complain otherwise. At the start of a conditional
    group whose condition is an assertion, cb->iscondassert is set. We unset it
    here so as to allow assertions later in the group to be quantified. */

    if (bravalue >= OP_ASSERT && bravalue <= OP_ASSERTBACK_NOT &&
        cb->iscondassert)
      {
      previous = NULL;
      cb->iscondassert = FALSE;
      }
    else
      {
      previous = code;
      }

    *code = bravalue;
    tempcode = code;
    tempreqvary = cb->req_varyopt;        /* Save value before bracket */
    tempbracount = cb->bracount;          /* Save value before bracket */
    length_prevgroup = 0;                 /* Initialize for pre-compile phase */

    if (!compile_regex(
         newoptions,                      /* The complete new option state */
         &tempcode,                       /* Where to put code (updated) */
         &ptr,                            /* Input pointer (updated) */
         errorcodeptr,                    /* Where to put an error message */
         (bravalue == OP_ASSERTBACK ||
          bravalue == OP_ASSERTBACK_NOT), /* TRUE if back assert */
         reset_bracount,                  /* True if (?| group */
         skipunits,                       /* Skip over bracket number */
         cond_depth +
           ((bravalue == OP_COND)?1:0),   /* Depth of condition subpatterns */
         &subfirstcu,                     /* For possible first char */
         &subfirstcuflags,
         &subreqcu,                       /* For possible last char */
         &subreqcuflags,
         bcptr,                           /* Current branch chain */
         cb,                              /* Compile data block */
         (lengthptr == NULL)? NULL :      /* Actual compile phase */
           &length_prevgroup              /* Pre-compile phase */
         ))
      goto FAILED;

    cb->parens_depth -= 1;

    /* If this was an atomic group and there are no capturing groups within it,
    generate OP_ONCE_NC instead of OP_ONCE. */

    if (bravalue == OP_ONCE && cb->bracount <= tempbracount)
      *code = OP_ONCE_NC;

    if (bravalue >= OP_ASSERT && bravalue <= OP_ASSERTBACK_NOT)
      cb->assert_depth -= 1;

    /* At the end of compiling, code is still pointing to the start of the
    group, while tempcode has been updated to point past the end of the group.
    The pattern pointer (ptr) is on the bracket.

    If this is a conditional bracket, check that there are no more than
    two branches in the group, or just one if it's a DEFINE group. We do this
    in the real compile phase, not in the pre-pass, where the whole group may
    not be available. */

    if (bravalue == OP_COND && lengthptr == NULL)
      {
      PCRE2_UCHAR *tc = code;
      int condcount = 0;

      do {
         condcount++;
         tc += GET(tc,1);
         }
      while (*tc != OP_KET);

      /* A DEFINE group is never obeyed inline (the "condition" is always
      false). It must have only one branch. Having checked this, change the
      opcode to OP_FALSE. */

      if (code[LINK_SIZE+1] == OP_DEFINE)
        {
        if (condcount > 1)
          {
          *errorcodeptr = ERR54;
          goto FAILED;
          }
        code[LINK_SIZE+1] = OP_FALSE;
        bravalue = OP_DEFINE;   /* Just a flag to suppress char handling below */
        }

      /* A "normal" conditional group. If there is just one branch, we must not
      make use of its firstcu or reqcu, because this is equivalent to an
      empty second branch. */

      else
        {
        if (condcount > 2)
          {
          *errorcodeptr = ERR27;
          goto FAILED;
          }
        if (condcount == 1) subfirstcuflags = subreqcuflags = REQ_NONE;
        }
      }

    /* At the end of a group, it's an error if we hit end of pattern or
    any non-closing parenthesis. This check also happens in the pre-scan,
    so should not trigger here, but leave this code as an insurance. */

    if (*ptr != CHAR_RIGHT_PARENTHESIS)
      {
      *errorcodeptr = ERR14;
      goto FAILED;
      }

    /* In the pre-compile phase, update the length by the length of the group,
    less the brackets at either end. Then reduce the compiled code to just a
    set of non-capturing brackets so that it doesn't use much memory if it is
    duplicated by a quantifier.*/

    if (lengthptr != NULL)
      {
      if (OFLOW_MAX - *lengthptr < length_prevgroup - 2 - 2*LINK_SIZE)
        {
        *errorcodeptr = ERR20;
        goto FAILED;
        }
      *lengthptr += length_prevgroup - 2 - 2*LINK_SIZE;
      code++;   /* This already contains bravalue */
      PUTINC(code, 0, 1 + LINK_SIZE);
      *code++ = OP_KET;
      PUTINC(code, 0, 1 + LINK_SIZE);
      break;    /* No need to waste time with special character handling */
      }

    /* Otherwise update the main code pointer to the end of the group. */

    code = tempcode;

    /* For a DEFINE group, required and first character settings are not
    relevant. */

    if (bravalue == OP_DEFINE) break;

    /* Handle updating of the required and first characters for other types of
    group. Update for normal brackets of all kinds, and conditions with two
    branches (see code above). If the bracket is followed by a quantifier with
    zero repeat, we have to back off. Hence the definition of zeroreqcu and
    zerofirstcu outside the main loop so that they can be accessed for the
    back off. */

    zeroreqcu = reqcu;
    zeroreqcuflags = reqcuflags;
    zerofirstcu = firstcu;
    zerofirstcuflags = firstcuflags;
    groupsetfirstcu = FALSE;

    if (bravalue >= OP_ONCE)
      {
      /* If we have not yet set a firstcu in this branch, take it from the
      subpattern, remembering that it was set here so that a repeat of more
      than one can replicate it as reqcu if necessary. If the subpattern has
      no firstcu, set "none" for the whole branch. In both cases, a zero
      repeat forces firstcu to "none". */

      if (firstcuflags == REQ_UNSET && subfirstcuflags != REQ_UNSET)
        {
        if (subfirstcuflags >= 0)
          {
          firstcu = subfirstcu;
          firstcuflags = subfirstcuflags;
          groupsetfirstcu = TRUE;
          }
        else firstcuflags = REQ_NONE;
        zerofirstcuflags = REQ_NONE;
        }

      /* If firstcu was previously set, convert the subpattern's firstcu
      into reqcu if there wasn't one, using the vary flag that was in
      existence beforehand. */

      else if (subfirstcuflags >= 0 && subreqcuflags < 0)
        {
        subreqcu = subfirstcu;
        subreqcuflags = subfirstcuflags | tempreqvary;
        }

      /* If the subpattern set a required byte (or set a first byte that isn't
      really the first byte - see above), set it. */

      if (subreqcuflags >= 0)
        {
        reqcu = subreqcu;
        reqcuflags = subreqcuflags;
        }
      }

    /* For a forward assertion, we take the reqcu, if set. This can be
    helpful if the pattern that follows the assertion doesn't set a different
    char. For example, it's useful for /(?=abcde).+/. We can't set firstcu
    for an assertion, however because it leads to incorrect effect for patterns
    such as /(?=a)a.+/ when the "real" "a" would then become a reqcu instead
    of a firstcu. This is overcome by a scan at the end if there's no
    firstcu, looking for an asserted first char. */

    else if (bravalue == OP_ASSERT && subreqcuflags >= 0)
      {
      reqcu = subreqcu;
      reqcuflags = subreqcuflags;
      }
    break;     /* End of processing '(' */


    /* ===================================================================*/
    /* Handle metasequences introduced by \. For ones like \d, the ESC_ values
    are arranged to be the negation of the corresponding OP_values in the
    default case when PCRE2_UCP is not set. For the back references, the values
    are negative the reference number. Only back references and those types
    that consume a character may be repeated. We can test for values between
    ESC_b and ESC_Z for the latter; this may have to change if any new ones are
    ever created.

    Note: \Q and \E are handled at the start of the character-processing loop,
    not here. */

    case CHAR_BACKSLASH:
    tempptr = ptr;
    escape = PRIV(check_escape)(&ptr, cb->end_pattern, &ec, errorcodeptr,
      options, FALSE, cb);
    if (*errorcodeptr != 0) goto FAILED;

    if (escape == 0)                  /* The escape coded a single character */
      c = ec;
    else
      {
      /* For metasequences that actually match a character, we disable the
      setting of a first character if it hasn't already been set. */

      if (firstcuflags == REQ_UNSET && escape > ESC_b && escape < ESC_Z)
        firstcuflags = REQ_NONE;

      /* Set values to reset to if this is followed by a zero repeat. */

      zerofirstcu = firstcu;
      zerofirstcuflags = firstcuflags;
      zeroreqcu = reqcu;
      zeroreqcuflags = reqcuflags;

      /* \g<name> or \g'name' is a subroutine call by name and \g<n> or \g'n'
      is a subroutine call by number (Oniguruma syntax). In fact, the value
      ESC_g is returned only for these cases. So we don't need to check for <
      or ' if the value is ESC_g. For the Perl syntax \g{n} the value is
      -n, and for the Perl syntax \g{name} the result is ESC_k (as
      that is a synonym for a named back reference). */

      if (escape == ESC_g)
        {
        PCRE2_SPTR p;
        uint32_t cf;

        terminator = (*(++ptr) == CHAR_LESS_THAN_SIGN)?
          CHAR_GREATER_THAN_SIGN : CHAR_APOSTROPHE;

        /* These two statements stop the compiler for warning about possibly
        unset variables caused by the jump to HANDLE_NUMERICAL_RECURSION. In
        fact, because we do the check for a number below, the paths that
        would actually be in error are never taken. */

        skipunits = 0;
        reset_bracount = FALSE;

        /* If it's not a signed or unsigned number, treat it as a name. */

        cf = ptr[1];
        if (cf != CHAR_PLUS && cf != CHAR_MINUS && !IS_DIGIT(cf))
          {
          is_recurse = TRUE;
          goto NAMED_REF_OR_RECURSE;
          }

        /* Signed or unsigned number (cf = ptr[1]) is known to be plus or minus
        or a digit. */

        p = ptr + 2;
        while (IS_DIGIT(*p)) p++;
        if (*p != (PCRE2_UCHAR)terminator)
          {
          *errorcodeptr = ERR57;
          goto FAILED;
          }
        ptr++;
        goto HANDLE_NUMERICAL_RECURSION;
        }

      /* \k<name> or \k'name' is a back reference by name (Perl syntax).
      We also support \k{name} (.NET syntax).  */

      if (escape == ESC_k)
        {
        if ((ptr[1] != CHAR_LESS_THAN_SIGN &&
          ptr[1] != CHAR_APOSTROPHE && ptr[1] != CHAR_LEFT_CURLY_BRACKET))
          {
          *errorcodeptr = ERR69;
          goto FAILED;
          }
        is_recurse = FALSE;
        terminator = (*(++ptr) == CHAR_LESS_THAN_SIGN)?
          CHAR_GREATER_THAN_SIGN : (*ptr == CHAR_APOSTROPHE)?
          CHAR_APOSTROPHE : CHAR_RIGHT_CURLY_BRACKET;
        goto NAMED_REF_OR_RECURSE;
        }

      /* Back references are handled specially; must disable firstcu if
      not set to cope with cases like (?=(\w+))\1: which would otherwise set
      ':' later. */

      if (escape < 0)
        {
        open_capitem *oc;
        recno = -escape;

        /* Come here from named backref handling when the reference is to a
        single group (i.e. not to a duplicated name). */

        HANDLE_REFERENCE:
        if (recno > (int)cb->final_bracount)
          {
          *errorcodeptr = ERR15;
          goto FAILED;
          }
        if (firstcuflags == REQ_UNSET) firstcuflags = REQ_NONE;
        previous = code;
        *code++ = ((options & PCRE2_CASELESS) != 0)? OP_REFI : OP_REF;
        PUT2INC(code, 0, recno);
        cb->backref_map |= (recno < 32)? (1u << recno) : 1;
        if ((uint32_t)recno > cb->top_backref) cb->top_backref = recno;

        /* Check to see if this back reference is recursive, that it, it
        is inside the group that it references. A flag is set so that the
        group can be made atomic. */

        for (oc = cb->open_caps; oc != NULL; oc = oc->next)
          {
          if (oc->number == recno)
            {
            oc->flag = TRUE;
            break;
            }
          }
        }

      /* So are Unicode property matches, if supported. */

#ifdef SUPPORT_UNICODE
      else if (escape == ESC_P || escape == ESC_p)
        {
        BOOL negated;
        unsigned int ptype = 0, pdata = 0;
        if (!get_ucp(&ptr, &negated, &ptype, &pdata, errorcodeptr, cb))
          goto FAILED;
        previous = code;
        *code++ = ((escape == ESC_p) != negated)? OP_PROP : OP_NOTPROP;
        *code++ = ptype;
        *code++ = pdata;
        }
#else

      /* If Unicode properties are not supported, \X, \P, and \p are not
      allowed. */

      else if (escape == ESC_X || escape == ESC_P || escape == ESC_p)
        {
        *errorcodeptr = ERR45;
        goto FAILED;
        }
#endif

      /* The use of \C can be locked out. */

#ifdef NEVER_BACKSLASH_C
      else if (escape == ESC_C)
        {
        *errorcodeptr = ERR85;
        goto FAILED;
        }
#else
      else if (escape == ESC_C && (options & PCRE2_NEVER_BACKSLASH_C) != 0)
        {
        *errorcodeptr = ERR83;
        goto FAILED;
        }
#endif

      /* For the rest (including \X when Unicode properties are supported), we
      can obtain the OP value by negating the escape value in the default
      situation when PCRE2_UCP is not set. When it *is* set, we substitute
      Unicode property tests. Note that \b and \B do a one-character
      lookbehind, and \A also behaves as if it does. */

      else
        {
        if (escape == ESC_C) cb->external_flags |= PCRE2_HASBKC; /* Record */
        if ((escape == ESC_b || escape == ESC_B || escape == ESC_A) &&
             cb->max_lookbehind == 0)
          cb->max_lookbehind = 1;
#ifdef SUPPORT_UNICODE
        if (escape >= ESC_DU && escape <= ESC_wu)
          {
          cb->nestptr[1] = cb->nestptr[0];         /* Back up if at 2nd level */
          cb->nestptr[0] = ptr + 1;                /* Where to resume */
          ptr = substitutes[escape - ESC_DU] - 1;  /* Just before substitute */
          }
        else
#endif
        /* In non-UTF mode, and for both 32-bit modes, we turn \C into
        OP_ALLANY instead of OP_ANYBYTE so that it works in DFA mode and in
        lookbehinds. */

          {
          previous = (escape > ESC_b && escape < ESC_Z)? code : NULL;
#if PCRE2_CODE_UNIT_WIDTH == 32
          *code++ = (escape == ESC_C)? OP_ALLANY : escape;
#else
          *code++ = (!utf && escape == ESC_C)? OP_ALLANY : escape;
#endif
          }
        }
      continue;
      }

    /* We have a data character whose value is in c. In UTF-8 mode it may have
    a value > 127. We set its representation in the length/buffer, and then
    handle it as a data character. */

    mclength = PUTCHAR(c, mcbuffer);
    goto ONE_CHAR;


    /* ===================================================================*/
    /* Handle a literal character. It is guaranteed not to be whitespace or #
    when the extended flag is set. If we are in a UTF mode, it may be a
    multi-unit literal character. */

    default:
    NORMAL_CHAR:
    mclength = 1;
    mcbuffer[0] = c;

#ifdef SUPPORT_UNICODE
    if (utf && HAS_EXTRALEN(c))
      ACROSSCHAR(TRUE, ptr[1], mcbuffer[mclength++] = *(++ptr));
#endif

    /* At this point we have the character's bytes in mcbuffer, and the length
    in mclength. When not in UTF mode, the length is always 1. */

    ONE_CHAR:
    previous = code;

    /* For caseless UTF mode, check whether this character has more than one
    other case. If so, generate a special OP_PROP item instead of OP_CHARI. */

#ifdef SUPPORT_UNICODE
    if (utf && (options & PCRE2_CASELESS) != 0)
      {
      GETCHAR(c, mcbuffer);
      if ((c = UCD_CASESET(c)) != 0)
        {
        *code++ = OP_PROP;
        *code++ = PT_CLIST;
        *code++ = c;
        if (firstcuflags == REQ_UNSET)
          firstcuflags = zerofirstcuflags = REQ_NONE;
        break;
        }
      }
#endif

    /* Caseful matches, or not one of the multicase characters. */

    *code++ = ((options & PCRE2_CASELESS) != 0)? OP_CHARI : OP_CHAR;
    for (c = 0; c < mclength; c++) *code++ = mcbuffer[c];

    /* Remember if \r or \n were seen */

    if (mcbuffer[0] == CHAR_CR || mcbuffer[0] == CHAR_NL)
      cb->external_flags |= PCRE2_HASCRORLF;

    /* Set the first and required bytes appropriately. If no previous first
    byte, set it from this character, but revert to none on a zero repeat.
    Otherwise, leave the firstcu value alone, and don't change it on a zero
    repeat. */

    if (firstcuflags == REQ_UNSET)
      {
      zerofirstcuflags = REQ_NONE;
      zeroreqcu = reqcu;
      zeroreqcuflags = reqcuflags;

      /* If the character is more than one byte long, we can set firstcu
      only if it is not to be matched caselessly. */

      if (mclength == 1 || req_caseopt == 0)
        {
        firstcu = mcbuffer[0] | req_caseopt;
        firstcu = mcbuffer[0];
        firstcuflags = req_caseopt;

        if (mclength != 1)
          {
          reqcu = code[-1];
          reqcuflags = cb->req_varyopt;
          }
        }
      else firstcuflags = reqcuflags = REQ_NONE;
      }

    /* firstcu was previously set; we can set reqcu only if the length is
    1 or the matching is caseful. */

    else
      {
      zerofirstcu = firstcu;
      zerofirstcuflags = firstcuflags;
      zeroreqcu = reqcu;
      zeroreqcuflags = reqcuflags;
      if (mclength == 1 || req_caseopt == 0)
        {
        reqcu = code[-1];
        reqcuflags = req_caseopt | cb->req_varyopt;
        }
      }

    break;            /* End of literal character handling */
    }
  }                   /* end of big loop */

/* Control never reaches here by falling through, only by a goto for all the
error states. Pass back the position in the pattern so that it can be displayed
to the user for diagnosing the error. */

FAILED:
*ptrptr = ptr;
return FALSE;
}



/*************************************************
*   Compile regex: a sequence of alternatives    *
*************************************************/

/* On entry, ptr is pointing past the bracket character, but on return it
points to the closing bracket, or vertical bar, or end of string. The code
variable is pointing at the byte into which the BRA operator has been stored.
This function is used during the pre-compile phase when we are trying to find
out the amount of memory needed, as well as during the real compile phase. The
value of lengthptr distinguishes the two phases.

Arguments:
  options           option bits, including any changes for this subpattern
  codeptr           -> the address of the current code pointer
  ptrptr            -> the address of the current pattern pointer
  errorcodeptr      -> pointer to error code variable
  lookbehind        TRUE if this is a lookbehind assertion
  reset_bracount    TRUE to reset the count for each branch
  skipunits         skip this many code units at start (for brackets and OP_COND)
  cond_depth        depth of nesting for conditional subpatterns
  firstcuptr        place to put the first required code unit
  firstcuflagsptr   place to put the first code unit flags, or a negative number
  reqcuptr          place to put the last required code unit
  reqcuflagsptr     place to put the last required code unit flags, or a negative number
  bcptr             pointer to the chain of currently open branches
  cb                points to the data block with tables pointers etc.
  lengthptr         NULL during the real compile phase
                    points to length accumulator during pre-compile phase

Returns:            TRUE on success
*/

static BOOL
compile_regex(uint32_t options, PCRE2_UCHAR **codeptr, PCRE2_SPTR *ptrptr,
  int *errorcodeptr, BOOL lookbehind, BOOL reset_bracount, uint32_t skipunits,
  int cond_depth, uint32_t *firstcuptr, int32_t *firstcuflagsptr,
  uint32_t *reqcuptr, int32_t *reqcuflagsptr, branch_chain *bcptr,
  compile_block *cb, size_t *lengthptr)
{
PCRE2_SPTR ptr = *ptrptr;
PCRE2_UCHAR *code = *codeptr;
PCRE2_UCHAR *last_branch = code;
PCRE2_UCHAR *start_bracket = code;
PCRE2_UCHAR *reverse_count = NULL;
open_capitem capitem;
int capnumber = 0;
uint32_t firstcu, reqcu;
int32_t firstcuflags, reqcuflags;
uint32_t branchfirstcu, branchreqcu;
int32_t branchfirstcuflags, branchreqcuflags;
size_t length;
unsigned int orig_bracount;
unsigned int max_bracount;
branch_chain bc;

/* If set, call the external function that checks for stack availability. */

if (cb->cx->stack_guard != NULL &&
    cb->cx->stack_guard(cb->parens_depth, cb->cx->stack_guard_data))
  {
  *errorcodeptr= ERR33;
  return FALSE;
  }

/* Miscellaneous initialization */

bc.outer = bcptr;
bc.current_branch = code;

firstcu = reqcu = 0;
firstcuflags = reqcuflags = REQ_UNSET;

/* Accumulate the length for use in the pre-compile phase. Start with the
length of the BRA and KET and any extra code units that are required at the
beginning. We accumulate in a local variable to save frequent testing of
lengthptr for NULL. We cannot do this by looking at the value of 'code' at the
start and end of each alternative, because compiled items are discarded during
the pre-compile phase so that the work space is not exceeded. */

length = 2 + 2*LINK_SIZE + skipunits;

/* WARNING: If the above line is changed for any reason, you must also change
the code that abstracts option settings at the start of the pattern and makes
them global. It tests the value of length for (2 + 2*LINK_SIZE) in the
pre-compile phase to find out whether or not anything has yet been compiled.

If this is a capturing subpattern, add to the chain of open capturing items
so that we can detect them if (*ACCEPT) is encountered. This is also used to
detect groups that contain recursive back references to themselves. Note that
only OP_CBRA need be tested here; changing this opcode to one of its variants,
e.g. OP_SCBRAPOS, happens later, after the group has been compiled. */

if (*code == OP_CBRA)
  {
  capnumber = GET2(code, 1 + LINK_SIZE);
  capitem.number = capnumber;
  capitem.next = cb->open_caps;
  capitem.flag = FALSE;
  cb->open_caps = &capitem;
  }

/* Offset is set zero to mark that this bracket is still open */

PUT(code, 1, 0);
code += 1 + LINK_SIZE + skipunits;

/* Loop for each alternative branch */

orig_bracount = max_bracount = cb->bracount;

for (;;)
  {
  /* For a (?| group, reset the capturing bracket count so that each branch
  uses the same numbers. */

  if (reset_bracount) cb->bracount = orig_bracount;

  /* Set up dummy OP_REVERSE if lookbehind assertion */

  if (lookbehind)
    {
    *code++ = OP_REVERSE;
    reverse_count = code;
    PUTINC(code, 0, 0);
    length += 1 + LINK_SIZE;
    }

  /* Now compile the branch; in the pre-compile phase its length gets added
  into the length. */

  if (!compile_branch(&options, &code, &ptr, errorcodeptr, &branchfirstcu,
        &branchfirstcuflags, &branchreqcu, &branchreqcuflags, &bc,
        cond_depth, cb, (lengthptr == NULL)? NULL : &length))
    {
    *ptrptr = ptr;
    return FALSE;
    }

  /* Keep the highest bracket count in case (?| was used and some branch
  has fewer than the rest. */

  if (cb->bracount > max_bracount) max_bracount = cb->bracount;

  /* In the real compile phase, there is some post-processing to be done. */

  if (lengthptr == NULL)
    {
    /* If this is the first branch, the firstcu and reqcu values for the
    branch become the values for the regex. */

    if (*last_branch != OP_ALT)
      {
      firstcu = branchfirstcu;
      firstcuflags = branchfirstcuflags;
      reqcu = branchreqcu;
      reqcuflags = branchreqcuflags;
      }

    /* If this is not the first branch, the first char and reqcu have to
    match the values from all the previous branches, except that if the
    previous value for reqcu didn't have REQ_VARY set, it can still match,
    and we set REQ_VARY for the regex. */

    else
      {
      /* If we previously had a firstcu, but it doesn't match the new branch,
      we have to abandon the firstcu for the regex, but if there was
      previously no reqcu, it takes on the value of the old firstcu. */

      if (firstcuflags != branchfirstcuflags || firstcu != branchfirstcu)
        {
        if (firstcuflags >= 0)
          {
          if (reqcuflags < 0)
            {
            reqcu = firstcu;
            reqcuflags = firstcuflags;
            }
          }
        firstcuflags = REQ_NONE;
        }

      /* If we (now or from before) have no firstcu, a firstcu from the
      branch becomes a reqcu if there isn't a branch reqcu. */

      if (firstcuflags < 0 && branchfirstcuflags >= 0 &&
          branchreqcuflags < 0)
        {
        branchreqcu = branchfirstcu;
        branchreqcuflags = branchfirstcuflags;
        }

      /* Now ensure that the reqcus match */

      if (((reqcuflags & ~REQ_VARY) != (branchreqcuflags & ~REQ_VARY)) ||
          reqcu != branchreqcu)
        reqcuflags = REQ_NONE;
      else
        {
        reqcu = branchreqcu;
        reqcuflags |= branchreqcuflags; /* To "or" REQ_VARY */
        }
      }

    /* If lookbehind, check that this branch matches a fixed-length string, and
    put the length into the OP_REVERSE item. Temporarily mark the end of the
    branch with OP_END. If the branch contains OP_RECURSE, the result is
    FFL_LATER (a negative value) because there may be forward references that
    we can't check here. Set a flag to cause another lookbehind check at the
    end. Why not do it all at the end? Because common errors can be picked up
    here and the offset of the problem can be shown. */

    if (lookbehind)
      {
      int fixed_length;
      int count = 0;
      *code = OP_END;
      fixed_length = find_fixedlength(last_branch,  (options & PCRE2_UTF) != 0,
        FALSE, cb, NULL, &count);
      if (fixed_length == FFL_LATER)
        {
        cb->check_lookbehind = TRUE;
        }
      else if (fixed_length < 0)
        {
        *errorcodeptr = fixed_length_errors[-fixed_length];
        *ptrptr = ptr;
        return FALSE;
        }
      else
        {
        if (fixed_length > cb->max_lookbehind)
          cb->max_lookbehind = fixed_length;
        PUT(reverse_count, 0, fixed_length);
        }
      }
    }

  /* Reached end of expression, either ')' or end of pattern. In the real
  compile phase, go back through the alternative branches and reverse the chain
  of offsets, with the field in the BRA item now becoming an offset to the
  first alternative. If there are no alternatives, it points to the end of the
  group. The length in the terminating ket is always the length of the whole
  bracketed item. Return leaving the pointer at the terminating char. */

  if (*ptr != CHAR_VERTICAL_LINE)
    {
    if (lengthptr == NULL)
      {
      size_t branch_length = code - last_branch;
      do
        {
        size_t prev_length = GET(last_branch, 1);
        PUT(last_branch, 1, branch_length);
        branch_length = prev_length;
        last_branch -= branch_length;
        }
      while (branch_length > 0);
      }

    /* Fill in the ket */

    *code = OP_KET;
    PUT(code, 1, (int)(code - start_bracket));
    code += 1 + LINK_SIZE;

    /* If it was a capturing subpattern, check to see if it contained any
    recursive back references. If so, we must wrap it in atomic brackets. In
    any event, remove the block from the chain. */

    if (capnumber > 0)
      {
      if (cb->open_caps->flag)
        {
        memmove(start_bracket + 1 + LINK_SIZE, start_bracket,
          CU2BYTES(code - start_bracket));
        *start_bracket = OP_ONCE;
        code += 1 + LINK_SIZE;
        PUT(start_bracket, 1, (int)(code - start_bracket));
        *code = OP_KET;
        PUT(code, 1, (int)(code - start_bracket));
        code += 1 + LINK_SIZE;
        length += 2 + 2*LINK_SIZE;
        }
      cb->open_caps = cb->open_caps->next;
      }

    /* Retain the highest bracket number, in case resetting was used. */

    cb->bracount = max_bracount;

    /* Set values to pass back */

    *codeptr = code;
    *ptrptr = ptr;
    *firstcuptr = firstcu;
    *firstcuflagsptr = firstcuflags;
    *reqcuptr = reqcu;
    *reqcuflagsptr = reqcuflags;
    if (lengthptr != NULL)
      {
      if (OFLOW_MAX - *lengthptr < length)
        {
        *errorcodeptr = ERR20;
        return FALSE;
        }
      *lengthptr += length;
      }
    return TRUE;
    }

  /* Another branch follows. In the pre-compile phase, we can move the code
  pointer back to where it was for the start of the first branch. (That is,
  pretend that each branch is the only one.)

  In the real compile phase, insert an ALT node. Its length field points back
  to the previous branch while the bracket remains open. At the end the chain
  is reversed. It's done like this so that the start of the bracket has a
  zero offset until it is closed, making it possible to detect recursion. */

  if (lengthptr != NULL)
    {
    code = *codeptr + 1 + LINK_SIZE + skipunits;
    length += 1 + LINK_SIZE;
    }
  else
    {
    *code = OP_ALT;
    PUT(code, 1, (int)(code - last_branch));
    bc.current_branch = last_branch = code;
    code += 1 + LINK_SIZE;
    }

  /* Advance past the vertical bar */

  ptr++;
  }
/* Control never reaches here */
}



/*************************************************
*          Check for anchored pattern            *
*************************************************/

/* Try to find out if this is an anchored regular expression. Consider each
alternative branch. If they all start with OP_SOD or OP_CIRC, or with a bracket
all of whose alternatives start with OP_SOD or OP_CIRC (recurse ad lib), then
it's anchored. However, if this is a multiline pattern, then only OP_SOD will
be found, because ^ generates OP_CIRCM in that mode.

We can also consider a regex to be anchored if OP_SOM starts all its branches.
This is the code for \G, which means "match at start of match position, taking
into account the match offset".

A branch is also implicitly anchored if it starts with .* and DOTALL is set,
because that will try the rest of the pattern at all possible matching points,
so there is no point trying again.... er ....

.... except when the .* appears inside capturing parentheses, and there is a
subsequent back reference to those parentheses. We haven't enough information
to catch that case precisely.

At first, the best we could do was to detect when .* was in capturing brackets
and the highest back reference was greater than or equal to that level.
However, by keeping a bitmap of the first 31 back references, we can catch some
of the more common cases more precisely.

... A second exception is when the .* appears inside an atomic group, because
this prevents the number of characters it matches from being adjusted.

Arguments:
  code           points to start of the compiled pattern
  bracket_map    a bitmap of which brackets we are inside while testing; this
                   handles up to substring 31; after that we just have to take
                   the less precise approach
  cb             points to the compile data block
  atomcount      atomic group level

Returns:     TRUE or FALSE
*/

static BOOL
is_anchored(register PCRE2_SPTR code, unsigned int bracket_map,
  compile_block *cb, int atomcount)
{
do {
   PCRE2_SPTR scode = first_significant_code(
     code + PRIV(OP_lengths)[*code], FALSE);
   register int op = *scode;

   /* Non-capturing brackets */

   if (op == OP_BRA  || op == OP_BRAPOS ||
       op == OP_SBRA || op == OP_SBRAPOS)
     {
     if (!is_anchored(scode, bracket_map, cb, atomcount)) return FALSE;
     }

   /* Capturing brackets */

   else if (op == OP_CBRA  || op == OP_CBRAPOS ||
            op == OP_SCBRA || op == OP_SCBRAPOS)
     {
     int n = GET2(scode, 1+LINK_SIZE);
     int new_map = bracket_map | ((n < 32)? (1u << n) : 1);
     if (!is_anchored(scode, new_map, cb, atomcount)) return FALSE;
     }

   /* Positive forward assertions and conditions */

   else if (op == OP_ASSERT || op == OP_COND)
     {
     if (!is_anchored(scode, bracket_map, cb, atomcount)) return FALSE;
     }

   /* Atomic groups */

   else if (op == OP_ONCE || op == OP_ONCE_NC)
     {
     if (!is_anchored(scode, bracket_map, cb, atomcount + 1))
       return FALSE;
     }

   /* .* is not anchored unless DOTALL is set (which generates OP_ALLANY) and
   it isn't in brackets that are or may be referenced or inside an atomic
   group. There is also an option that disables auto-anchoring. */

   else if ((op == OP_TYPESTAR || op == OP_TYPEMINSTAR ||
             op == OP_TYPEPOSSTAR))
     {
     if (scode[1] != OP_ALLANY || (bracket_map & cb->backref_map) != 0 ||
         atomcount > 0 || cb->had_pruneorskip ||
         (cb->external_options & PCRE2_NO_DOTSTAR_ANCHOR) != 0)
       return FALSE;
     }

   /* Check for explicit anchoring */

   else if (op != OP_SOD && op != OP_SOM && op != OP_CIRC) return FALSE;

   code += GET(code, 1);
   }
while (*code == OP_ALT);   /* Loop for each alternative */
return TRUE;
}



/*************************************************
*         Check for starting with ^ or .*        *
*************************************************/

/* This is called to find out if every branch starts with ^ or .* so that
"first char" processing can be done to speed things up in multiline
matching and for non-DOTALL patterns that start with .* (which must start at
the beginning or after \n). As in the case of is_anchored() (see above), we
have to take account of back references to capturing brackets that contain .*
because in that case we can't make the assumption. Also, the appearance of .*
inside atomic brackets or in a pattern that contains *PRUNE or *SKIP does not
count, because once again the assumption no longer holds.

Arguments:
  code           points to start of the compiled pattern or a group
  bracket_map    a bitmap of which brackets we are inside while testing; this
                   handles up to substring 31; after that we just have to take
                   the less precise approach
  cb             points to the compile data
  atomcount      atomic group level

Returns:         TRUE or FALSE
*/

static BOOL
is_startline(PCRE2_SPTR code, unsigned int bracket_map, compile_block *cb,
  int atomcount)
{
do {
   PCRE2_SPTR scode = first_significant_code(
     code + PRIV(OP_lengths)[*code], FALSE);
   register int op = *scode;

   /* If we are at the start of a conditional assertion group, *both* the
   conditional assertion *and* what follows the condition must satisfy the test
   for start of line. Other kinds of condition fail. Note that there may be an
   auto-callout at the start of a condition. */

   if (op == OP_COND)
     {
     scode += 1 + LINK_SIZE;

     if (*scode == OP_CALLOUT) scode += PRIV(OP_lengths)[OP_CALLOUT];
       else if (*scode == OP_CALLOUT_STR) scode += GET(scode, 1 + 2*LINK_SIZE);

     switch (*scode)
       {
       case OP_CREF:
       case OP_DNCREF:
       case OP_RREF:
       case OP_DNRREF:
       case OP_FAIL:
       case OP_FALSE:
       case OP_TRUE:
       return FALSE;

       default:     /* Assertion */
       if (!is_startline(scode, bracket_map, cb, atomcount)) return FALSE;
       do scode += GET(scode, 1); while (*scode == OP_ALT);
       scode += 1 + LINK_SIZE;
       break;
       }
     scode = first_significant_code(scode, FALSE);
     op = *scode;
     }

   /* Non-capturing brackets */

   if (op == OP_BRA  || op == OP_BRAPOS ||
       op == OP_SBRA || op == OP_SBRAPOS)
     {
     if (!is_startline(scode, bracket_map, cb, atomcount)) return FALSE;
     }

   /* Capturing brackets */

   else if (op == OP_CBRA  || op == OP_CBRAPOS ||
            op == OP_SCBRA || op == OP_SCBRAPOS)
     {
     int n = GET2(scode, 1+LINK_SIZE);
     int new_map = bracket_map | ((n < 32)? (1u << n) : 1);
     if (!is_startline(scode, new_map, cb, atomcount)) return FALSE;
     }

   /* Positive forward assertions */

   else if (op == OP_ASSERT)
     {
     if (!is_startline(scode, bracket_map, cb, atomcount)) return FALSE;
     }

   /* Atomic brackets */

   else if (op == OP_ONCE || op == OP_ONCE_NC)
     {
     if (!is_startline(scode, bracket_map, cb, atomcount + 1)) return FALSE;
     }

   /* .* means "start at start or after \n" if it isn't in atomic brackets or
   brackets that may be referenced, as long as the pattern does not contain
   *PRUNE or *SKIP, because these break the feature. Consider, for example,
   /.*?a(*PRUNE)b/ with the subject "aab", which matches "ab", i.e. not at the
   start of a line. There is also an option that disables this optimization. */

   else if (op == OP_TYPESTAR || op == OP_TYPEMINSTAR || op == OP_TYPEPOSSTAR)
     {
     if (scode[1] != OP_ANY || (bracket_map & cb->backref_map) != 0 ||
         atomcount > 0 || cb->had_pruneorskip ||
         (cb->external_options & PCRE2_NO_DOTSTAR_ANCHOR) != 0)
       return FALSE;
     }

   /* Check for explicit circumflex; anything else gives a FALSE result. Note
   in particular that this includes atomic brackets OP_ONCE and OP_ONCE_NC
   because the number of characters matched by .* cannot be adjusted inside
   them. */

   else if (op != OP_CIRC && op != OP_CIRCM) return FALSE;

   /* Move on to the next alternative */

   code += GET(code, 1);
   }
while (*code == OP_ALT);  /* Loop for each alternative */
return TRUE;
}



/*************************************************
*    Check for asserted fixed first code unit    *
*************************************************/

/* During compilation, the "first code unit" settings from forward assertions
are discarded, because they can cause conflicts with actual literals that
follow. However, if we end up without a first code unit setting for an
unanchored pattern, it is worth scanning the regex to see if there is an
initial asserted first code unit. If all branches start with the same asserted
code unit, or with a non-conditional bracket all of whose alternatives start
with the same asserted code unit (recurse ad lib), then we return that code
unit, with the flags set to zero or REQ_CASELESS; otherwise return zero with
REQ_NONE in the flags.

Arguments:
  code       points to start of compiled pattern
  flags      points to the first code unit flags
  inassert   TRUE if in an assertion

Returns:     the fixed first code unit, or 0 with REQ_NONE in flags
*/

static uint32_t
find_firstassertedcu(PCRE2_SPTR code, int32_t *flags, BOOL inassert)
{
register uint32_t c = 0;
int cflags = REQ_NONE;

*flags = REQ_NONE;
do {
   uint32_t d;
   int dflags;
   int xl = (*code == OP_CBRA || *code == OP_SCBRA ||
             *code == OP_CBRAPOS || *code == OP_SCBRAPOS)? IMM2_SIZE:0;
   PCRE2_SPTR scode = first_significant_code(code + 1+LINK_SIZE + xl, TRUE);
   register PCRE2_UCHAR op = *scode;

   switch(op)
     {
     default:
     return 0;

     case OP_BRA:
     case OP_BRAPOS:
     case OP_CBRA:
     case OP_SCBRA:
     case OP_CBRAPOS:
     case OP_SCBRAPOS:
     case OP_ASSERT:
     case OP_ONCE:
     case OP_ONCE_NC:
     d = find_firstassertedcu(scode, &dflags, op == OP_ASSERT);
     if (dflags < 0)
       return 0;
     if (cflags < 0) { c = d; cflags = dflags; }
       else if (c != d || cflags != dflags) return 0;
     break;

     case OP_EXACT:
     scode += IMM2_SIZE;
     /* Fall through */

     case OP_CHAR:
     case OP_PLUS:
     case OP_MINPLUS:
     case OP_POSPLUS:
     if (!inassert) return 0;
     if (cflags < 0) { c = scode[1]; cflags = 0; }
       else if (c != scode[1]) return 0;
     break;

     case OP_EXACTI:
     scode += IMM2_SIZE;
     /* Fall through */

     case OP_CHARI:
     case OP_PLUSI:
     case OP_MINPLUSI:
     case OP_POSPLUSI:
     if (!inassert) return 0;
     if (cflags < 0) { c = scode[1]; cflags = REQ_CASELESS; }
       else if (c != scode[1]) return 0;
     break;
     }

   code += GET(code, 1);
   }
while (*code == OP_ALT);

*flags = cflags;
return c;
}



/*************************************************
*     Add an entry to the name/number table      *
*************************************************/

/* This function is called between compiling passes to add an entry to the
name/number table, maintaining alphabetical order. Checking for permitted
and forbidden duplicates has already been done.

Arguments:
  cb           the compile data block
  name         the name to add
  length       the length of the name
  groupno      the group number

Returns:       nothing
*/

static void
add_name_to_table(compile_block *cb, PCRE2_SPTR name, int length,
  unsigned int groupno)
{
int i;
PCRE2_UCHAR *slot = cb->name_table;

for (i = 0; i < cb->names_found; i++)
  {
  int crc = memcmp(name, slot+IMM2_SIZE, CU2BYTES(length));
  if (crc == 0 && slot[IMM2_SIZE+length] != 0)
    crc = -1; /* Current name is a substring */

  /* Make space in the table and break the loop for an earlier name. For a
  duplicate or later name, carry on. We do this for duplicates so that in the
  simple case (when ?(| is not used) they are in order of their numbers. In all
  cases they are in the order in which they appear in the pattern. */

  if (crc < 0)
    {
    memmove(slot + cb->name_entry_size, slot,
      CU2BYTES((cb->names_found - i) * cb->name_entry_size));
    break;
    }

  /* Continue the loop for a later or duplicate name */

  slot += cb->name_entry_size;
  }

PUT2(slot, 0, groupno);
memcpy(slot + IMM2_SIZE, name, CU2BYTES(length));
cb->names_found++;

/* Add a terminating zero and fill the rest of the slot with zeroes so that
the memory is all initialized. Otherwise valgrind moans about uninitialized
memory when saving serialized compiled patterns. */

memset(slot + IMM2_SIZE + length, 0,
  CU2BYTES(cb->name_entry_size - length - IMM2_SIZE));
}



/*************************************************
*     External function to compile a pattern     *
*************************************************/

/* This function reads a regular expression in the form of a string and returns
a pointer to a block of store holding a compiled version of the expression.

Arguments:
  pattern       the regular expression
  patlen        the length of the pattern, or PCRE2_ZERO_TERMINATED
  options       option bits
  errorptr      pointer to errorcode
  erroroffset   pointer to error offset
  ccontext      points to a compile context or is NULL

Returns:        pointer to compiled data block, or NULL on error,
                with errorcode and erroroffset set
*/

PCRE2_EXP_DEFN pcre2_code * PCRE2_CALL_CONVENTION
pcre2_compile(PCRE2_SPTR pattern, PCRE2_SIZE patlen, uint32_t options,
   int *errorptr, PCRE2_SIZE *erroroffset, pcre2_compile_context *ccontext)
{
BOOL utf;                               /* Set TRUE for UTF mode */
pcre2_real_code *re = NULL;             /* What we will return */
compile_block cb;                       /* "Static" compile-time data */
const uint8_t *tables;                  /* Char tables base pointer */

PCRE2_UCHAR *code;                      /* Current pointer in compiled code */
PCRE2_SPTR codestart;                   /* Start of compiled code */
PCRE2_SPTR ptr;                         /* Current pointer in pattern */

size_t length = 1;                      /* Allow or final END opcode */
size_t usedlength;                      /* Actual length used */
size_t re_blocksize;                    /* Size of memory block */

int32_t firstcuflags, reqcuflags;       /* Type of first/req code unit */
uint32_t firstcu, reqcu;                /* Value of first/req code unit */
uint32_t setflags = 0;                  /* NL and BSR set flags */

uint32_t skipatstart;                   /* When checking (*UTF) etc */
uint32_t limit_match = UINT32_MAX;      /* Unset match limits */
uint32_t limit_recursion = UINT32_MAX;

int newline = 0;                        /* Unset; can be set by the pattern */
int bsr = 0;                            /* Unset; can be set by the pattern */
int errorcode = 0;                      /* Initialize to avoid compiler warn */

/* Comments at the head of this file explain about these variables. */

PCRE2_UCHAR *copied_pattern = NULL;
PCRE2_UCHAR stack_copied_pattern[COPIED_PATTERN_SIZE];
named_group named_groups[NAMED_GROUP_LIST_SIZE];

/* The workspace is used in different ways in the different compiling phases.
It needs to be 16-bit aligned for the preliminary group scan, and 32-bit
aligned for the group information cache. */

uint32_t c32workspace[C32_WORK_SIZE];
PCRE2_UCHAR *cworkspace = (PCRE2_UCHAR *)c32workspace;


/* -------------- Check arguments and set up the pattern ----------------- */

/* There must be error code and offset pointers. */

if (errorptr == NULL || erroroffset == NULL) return NULL;
*errorptr = ERR0;
*erroroffset = 0;

/* There must be a pattern! */

if (pattern == NULL)
  {
  *errorptr = ERR16;
  return NULL;
  }

/* Check that all undefined public option bits are zero. */

if ((options & ~PUBLIC_COMPILE_OPTIONS) != 0)
  {
  *errorptr = ERR17;
  return NULL;
  }

/* A NULL compile context means "use a default context" */

if (ccontext == NULL)
  ccontext = (pcre2_compile_context *)(&PRIV(default_compile_context));

/* A zero-terminated pattern is indicated by the special length value
PCRE2_ZERO_TERMINATED. Otherwise, we make a copy of the pattern and add a zero,
to ensure that it is always possible to look one code unit beyond the end of
the pattern's characters. In both cases, check that the pattern is overlong. */

if (patlen == PCRE2_ZERO_TERMINATED)
  {
  patlen = PRIV(strlen)(pattern);
  if (patlen > ccontext->max_pattern_length)
    {
    *errorptr = ERR88;
    return NULL;
    }
  }
else
  {
  if (patlen > ccontext->max_pattern_length)
    {
    *errorptr = ERR88;
    return NULL;
    }
  if (patlen < COPIED_PATTERN_SIZE)
    copied_pattern = stack_copied_pattern;
  else
    {
    copied_pattern = ccontext->memctl.malloc(CU2BYTES(patlen + 1),
      ccontext->memctl.memory_data);
    if (copied_pattern == NULL)
      {
      *errorptr = ERR21;
      return NULL;
      }
    }
  memcpy(copied_pattern, pattern, CU2BYTES(patlen));
  copied_pattern[patlen] = 0;
  pattern = copied_pattern;
  }

/* ------------ Initialize the "static" compile data -------------- */

tables = (ccontext->tables != NULL)? ccontext->tables : PRIV(default_tables);

cb.lcc = tables + lcc_offset;          /* Individual */
cb.fcc = tables + fcc_offset;          /*   character */
cb.cbits = tables + cbits_offset;      /*      tables */
cb.ctypes = tables + ctypes_offset;

cb.assert_depth = 0;
cb.bracount = cb.final_bracount = 0;
cb.cx = ccontext;
cb.dupnames = FALSE;
cb.end_pattern = pattern + patlen;
cb.nestptr[0] = cb.nestptr[1] = NULL;
cb.external_flags = 0;
cb.external_options = options;
cb.groupinfo = c32workspace;
cb.had_recurse = FALSE;
cb.iscondassert = FALSE;
cb.max_lookbehind = 0;
cb.name_entry_size = 0;
cb.name_table = NULL;
cb.named_groups = named_groups;
cb.named_group_list_size = NAMED_GROUP_LIST_SIZE;
cb.names_found = 0;
cb.open_caps = NULL;
cb.parens_depth = 0;
cb.req_varyopt = 0;
cb.start_code = cworkspace;
cb.start_pattern = pattern;
cb.start_workspace = cworkspace;
cb.workspace_size = COMPILE_WORK_SIZE;

/* Maximum back reference and backref bitmap. The bitmap records up to 31 back
references to help in deciding whether (.*) can be treated as anchored or not.
*/

cb.top_backref = 0;
cb.backref_map = 0;

/* --------------- Start looking at the pattern --------------- */

/* Check for global one-time option settings at the start of the pattern, and
remember the offset to the actual regex. */

ptr = pattern;
skipatstart = 0;

while (ptr[skipatstart] == CHAR_LEFT_PARENTHESIS &&
       ptr[skipatstart+1] == CHAR_ASTERISK)
  {
  unsigned int i;
  for (i = 0; i < sizeof(pso_list)/sizeof(pso); i++)
    {
    pso *p = pso_list + i;

    if (PRIV(strncmp_c8)(ptr+skipatstart+2, (char *)(p->name), p->length) == 0)
      {
      uint32_t c, pp;

      skipatstart += p->length + 2;
      switch(p->type)
        {
        case PSO_OPT:
        cb.external_options |= p->value;
        break;

        case PSO_FLG:
        setflags |= p->value;
        break;

        case PSO_NL:
        newline = p->value;
        setflags |= PCRE2_NL_SET;
        break;

        case PSO_BSR:
        bsr = p->value;
        setflags |= PCRE2_BSR_SET;
        break;

        case PSO_LIMM:
        case PSO_LIMR:
        c = 0;
        pp = skipatstart;
        if (!IS_DIGIT(ptr[pp]))
          {
          errorcode = ERR60;
          ptr += pp;
          goto HAD_ERROR;
          }
        while (IS_DIGIT(ptr[pp]))
          {
          if (c > UINT32_MAX / 10 - 1) break;   /* Integer overflow */
          c = c*10 + (ptr[pp++] - CHAR_0);
          }
        if (ptr[pp++] != CHAR_RIGHT_PARENTHESIS)
          {
          errorcode = ERR60;
          ptr += pp;
          goto HAD_ERROR;
          }
        if (p->type == PSO_LIMM) limit_match = c;
          else limit_recursion = c;
        skipatstart += pp - skipatstart;
        break;
        }
      break;   /* Out of the table scan loop */
      }
    }
  if (i >= sizeof(pso_list)/sizeof(pso)) break;   /* Out of pso loop */
  }

/* End of pattern-start options; advance to start of real regex. */

ptr += skipatstart;

/* Can't support UTF or UCP unless PCRE2 has been compiled with UTF support. */

#ifndef SUPPORT_UNICODE
if ((cb.external_options & (PCRE2_UTF|PCRE2_UCP)) != 0)
  {
  errorcode = ERR32;
  goto HAD_ERROR;
  }
#endif

/* Check UTF. We have the original options in 'options', with that value as
modified by (*UTF) etc in cb->external_options. */

utf = (cb.external_options & PCRE2_UTF) != 0;
if (utf)
  {
  if ((options & PCRE2_NEVER_UTF) != 0)
    {
    errorcode = ERR74;
    goto HAD_ERROR;
    }
  if ((options & PCRE2_NO_UTF_CHECK) == 0 &&
       (errorcode = PRIV(valid_utf)(pattern, patlen, erroroffset)) != 0)
    goto HAD_UTF_ERROR;
  }

/* Check UCP lockout. */

if ((cb.external_options & (PCRE2_UCP|PCRE2_NEVER_UCP)) ==
    (PCRE2_UCP|PCRE2_NEVER_UCP))
  {
  errorcode = ERR75;
  goto HAD_ERROR;
  }

/* Process the BSR setting. */

if (bsr == 0) bsr = ccontext->bsr_convention;

/* Process the newline setting. */

if (newline == 0) newline = ccontext->newline_convention;
cb.nltype = NLTYPE_FIXED;
switch(newline)
  {
  case PCRE2_NEWLINE_CR:
  cb.nllen = 1;
  cb.nl[0] = CHAR_CR;
  break;

  case PCRE2_NEWLINE_LF:
  cb.nllen = 1;
  cb.nl[0] = CHAR_NL;
  break;

  case PCRE2_NEWLINE_CRLF:
  cb.nllen = 2;
  cb.nl[0] = CHAR_CR;
  cb.nl[1] = CHAR_NL;
  break;

  case PCRE2_NEWLINE_ANY:
  cb.nltype = NLTYPE_ANY;
  break;

  case PCRE2_NEWLINE_ANYCRLF:
  cb.nltype = NLTYPE_ANYCRLF;
  break;

  default:
  errorcode = ERR56;
  goto HAD_ERROR;
  }

/* Before we do anything else, do a pre-scan of the pattern in order to
discover the named groups and their numerical equivalents, so that this
information is always available for the remaining processing. */

errorcode = scan_for_captures(&ptr, cb.external_options, &cb);
if (errorcode != 0) goto HAD_ERROR;

/* For obscure debugging this code can be enabled. */

#if 0
  {
  int i;
  named_group *ng = cb.named_groups;
  fprintf(stderr, "+++Captures: %d\n", cb.final_bracount);
  for (i = 0; i < cb.names_found; i++, ng++)
    {
    fprintf(stderr, "+++%3d %.*s\n", ng->number, ng->length, ng->name);
    }
  }
#endif

/* Reset current bracket count to zero and current pointer to the start of the
pattern. */

cb.bracount = 0;
ptr = pattern + skipatstart;

/* Pretend to compile the pattern while actually just accumulating the amount
of memory required in the 'length' variable. This behaviour is triggered by
passing a non-NULL final argument to compile_regex(). We pass a block of
workspace (cworkspace) for it to compile parts of the pattern into; the
compiled code is discarded when it is no longer needed, so hopefully this
workspace will never overflow, though there is a test for its doing so.

On error, errorcode will be set non-zero, so we don't need to look at the
result of the function. The initial options have been put into the cb block so
that they can be changed if an option setting is found within the regex right
at the beginning. Bringing initial option settings outside can help speed up
starting point checks. We still have to pass a separate options variable (the
first argument) because that may change as the pattern is processed. */

code = cworkspace;
*code = OP_BRA;

(void)compile_regex(cb.external_options, &code, &ptr, &errorcode, FALSE,
  FALSE, 0, 0, &firstcu, &firstcuflags, &reqcu, &reqcuflags, NULL,
  &cb, &length);

if (errorcode != 0) goto HAD_ERROR;
if (length > MAX_PATTERN_SIZE)
  {
  errorcode = ERR20;
  goto HAD_ERROR;
  }

/* Compute the size of, and then get and initialize, the data block for storing
the compiled pattern and names table. Integer overflow should no longer be
possible because nowadays we limit the maximum value of cb.names_found and
cb.name_entry_size. */

re_blocksize = sizeof(pcre2_real_code) +
  CU2BYTES(length + cb.names_found * cb.name_entry_size);
re = (pcre2_real_code *)
  ccontext->memctl.malloc(re_blocksize, ccontext->memctl.memory_data);
if (re == NULL)
  {
  errorcode = ERR21;
  goto HAD_ERROR;
  }

re->memctl = ccontext->memctl;
re->tables = tables;
re->executable_jit = NULL;
memset(re->start_bitmap, 0, 32 * sizeof(uint8_t));
re->blocksize = re_blocksize;
re->magic_number = MAGIC_NUMBER;
re->compile_options = options;
re->overall_options = cb.external_options;
re->flags = PCRE2_CODE_UNIT_WIDTH/8 | cb.external_flags | setflags;
re->limit_match = limit_match;
re->limit_recursion = limit_recursion;
re->first_codeunit = 0;
re->last_codeunit = 0;
re->bsr_convention = bsr;
re->newline_convention = newline;
re->max_lookbehind = 0;
re->minlength = 0;
re->top_bracket = 0;
re->top_backref = 0;
re->name_entry_size = cb.name_entry_size;
re->name_count = cb.names_found;

/* The basic block is immediately followed by the name table, and the compiled
code follows after that. */

codestart = (PCRE2_SPTR)((uint8_t *)re + sizeof(pcre2_real_code)) +
  re->name_entry_size * re->name_count;

/* Workspace is needed to remember information about numbered groups: whether a
group can match an empty string and what its fixed length is. This is done to
avoid the possibility of recursive references causing very long compile times
when checking these features. Unnumbered groups do not have this exposure since
they cannot be referenced. We use an indexed vector for this purpose. If there
are sufficiently few groups, it can be the c32workspace vector, as set up
above. Otherwise we have to get/free a special vector. The vector must be
initialized to zero. */

if (cb.final_bracount >= C32_WORK_SIZE)
  {
  cb.groupinfo = ccontext->memctl.malloc(
    (cb.final_bracount + 1)*sizeof(uint32_t), ccontext->memctl.memory_data);
  if (cb.groupinfo == NULL)
    {
    errorcode = ERR21;
    goto HAD_ERROR;
    }
  }
memset(cb.groupinfo, 0, (cb.final_bracount + 1) * sizeof(uint32_t));

/* Update the compile data block for the actual compile. The starting points of
the name/number translation table and of the code are passed around in the
compile data block. The start/end pattern and initial options are already set
from the pre-compile phase, as is the name_entry_size field. Reset the bracket
count and the names_found field. */

cb.parens_depth = 0;
cb.assert_depth = 0;
cb.bracount = 0;
cb.max_lookbehind = 0;
cb.name_table = (PCRE2_UCHAR *)((uint8_t *)re + sizeof(pcre2_real_code));
cb.start_code = codestart;
cb.iscondassert = FALSE;
cb.req_varyopt = 0;
cb.had_accept = FALSE;
cb.had_pruneorskip = FALSE;
cb.check_lookbehind = FALSE;
cb.open_caps = NULL;

/* If any named groups were found, create the name/number table from the list
created in the pre-pass. */

if (cb.names_found > 0)
  {
  int i = cb.names_found;
  named_group *ng = cb.named_groups;
  cb.names_found = 0;
  for (; i > 0; i--, ng++)
    add_name_to_table(&cb, ng->name, ng->length, ng->number);
  }

/* Set up a starting, non-extracting bracket, then compile the expression. On
error, errorcode will be set non-zero, so we don't need to look at the result
of the function here. */

ptr = pattern + skipatstart;
code = (PCRE2_UCHAR *)codestart;
*code = OP_BRA;
(void)compile_regex(re->overall_options, &code, &ptr, &errorcode, FALSE, FALSE,
   0, 0, &firstcu, &firstcuflags, &reqcu, &reqcuflags, NULL, &cb, NULL);

re->top_bracket = cb.bracount;
re->top_backref = cb.top_backref;
re->max_lookbehind = cb.max_lookbehind;

if (cb.had_accept)
  {
  reqcu = 0;              /* Must disable after (*ACCEPT) */
  reqcuflags = REQ_NONE;
  }

/* Fill in the final opcode and check for disastrous overflow. If no overflow,
but the estimated length exceeds the really used length, adjust the value of
re->blocksize, and if valgrind support is configured, mark the extra allocated
memory as unaddressable, so that any out-of-bound reads can be detected. */

*code++ = OP_END;
usedlength = code - codestart;
if (usedlength > length) errorcode = ERR23; else
  {
  re->blocksize -= CU2BYTES(length - usedlength);
#ifdef SUPPORT_VALGRIND
  VALGRIND_MAKE_MEM_NOACCESS(code, CU2BYTES(length - usedlength));
#endif
  }

/* Scan the pattern for recursion/subroutine calls and convert the group
numbers into offsets. Maintain a small cache so that repeated groups containing
recursions are efficiently handled. */

#define RSCAN_CACHE_SIZE 8

if (errorcode == 0 && cb.had_recurse)
  {
  PCRE2_UCHAR *rcode;
  PCRE2_SPTR rgroup;
  int ccount = 0;
  int start = RSCAN_CACHE_SIZE;
  recurse_cache rc[RSCAN_CACHE_SIZE];

  for (rcode = (PCRE2_UCHAR *)find_recurse(codestart, utf);
       rcode != NULL;
       rcode = (PCRE2_UCHAR *)find_recurse(rcode + 1 + LINK_SIZE, utf))
    {
    int i, p, recno;

    recno = (int)GET(rcode, 1);
    if (recno == 0) rgroup = codestart; else
      {
      PCRE2_SPTR search_from = codestart;
      rgroup = NULL;
      for (i = 0, p = start; i < ccount; i++, p = (p + 1) & 7)
        {
        if (recno == rc[p].recno)
          {
          rgroup = rc[p].group;
          break;
          }

        /* Group n+1 must always start to the right of group n, so we can save
        search time below when the new group number is greater than any of the
        previously found groups. */

        if (recno > rc[p].recno) search_from = rc[p].group;
        }

      if (rgroup == NULL)
        {
        rgroup = PRIV(find_bracket)(search_from, utf, recno);
        if (rgroup == NULL)
          {
          errorcode = ERR53;
          break;
          }
        if (--start < 0) start = RSCAN_CACHE_SIZE - 1;
        rc[start].recno = recno;
        rc[start].group = rgroup;
        if (ccount < RSCAN_CACHE_SIZE) ccount++;
        }
      }

    PUT(rcode, 1, rgroup - codestart);
    }
  }

/* In rare debugging situations we sometimes need to look at the compiled code
at this stage. */

#ifdef CALL_PRINTINT
pcre2_printint(re, stderr, TRUE);
fprintf(stderr, "Length=%lu Used=%lu\n", length, usedlength);
#endif

/* After a successful compile, give an error if there's back reference to a
non-existent capturing subpattern. Then, unless disabled, check whether any
single character iterators can be auto-possessified. The function overwrites
the appropriate opcode values, so the type of the pointer must be cast. NOTE:
the intermediate variable "temp" is used in this code because at least one
compiler gives a warning about loss of "const" attribute if the cast
(PCRE2_UCHAR *)codestart is used directly in the function call. */

if (errorcode == 0)
  {
  if (re->top_backref > re->top_bracket) errorcode = ERR15;
  else if ((re->overall_options & PCRE2_NO_AUTO_POSSESS) == 0)
    {
    PCRE2_UCHAR *temp = (PCRE2_UCHAR *)codestart;
    if (PRIV(auto_possessify)(temp, utf, &cb) != 0) errorcode = ERR80;
    }
  }

/* If there were any lookbehind assertions that contained OP_RECURSE
(recursions or subroutine calls), a flag is set for them to be checked here,
because they may contain forward references. Actual recursions cannot be fixed
length, but subroutine calls can. It is done like this so that those without
OP_RECURSE that are not fixed length get a diagnosic with a useful offset. The
exceptional ones forgo this. We scan the pattern to check that they are fixed
length, and set their lengths. */

if (errorcode == 0 && cb.check_lookbehind)
  {
  PCRE2_UCHAR *cc = (PCRE2_UCHAR *)codestart;

  /* Loop, searching for OP_REVERSE items, and process those that do not have
  their length set. (Actually, it will also re-process any that have a length
  of zero, but that is a pathological case, and it does no harm.) When we find
  one, we temporarily terminate the branch it is in while we scan it. Note that
  calling find_bracket() with a negative group number returns a pointer to the
  OP_REVERSE item, not the actual lookbehind. */

  for (cc = (PCRE2_UCHAR *)PRIV(find_bracket)(codestart, utf, -1);
       cc != NULL;
       cc = (PCRE2_UCHAR *)PRIV(find_bracket)(cc, utf, -1))
    {
    if (GET(cc, 1) == 0)
      {
      int fixed_length;
      int count = 0;
      PCRE2_UCHAR *be = cc - 1 - LINK_SIZE + GET(cc, -LINK_SIZE);
      int end_op = *be;
      *be = OP_END;
      fixed_length = find_fixedlength(cc, utf, TRUE, &cb, NULL, &count);
      *be = end_op;
      if (fixed_length < 0)
        {
        errorcode = fixed_length_errors[-fixed_length];
        break;
        }
      if (fixed_length > cb.max_lookbehind) cb.max_lookbehind = fixed_length;
      PUT(cc, 1, fixed_length);
      }
    cc += 1 + LINK_SIZE;
    }

  /* The previous value of the maximum lookbehind was transferred to the
  compiled regex block above. We could have updated this value in the loop
  above, but keep the two values in step, just in case some later code below
  uses the cb value. */

  re->max_lookbehind = cb.max_lookbehind;
  }

/* Failed to compile, or error while post-processing. Earlier errors get here
via the dreaded goto. */

if (errorcode != 0)
  {
  HAD_ERROR:
  *erroroffset = (int)(ptr - pattern);
  HAD_UTF_ERROR:
  *errorptr = errorcode;
  pcre2_code_free(re);
  re = NULL;
  goto EXIT;
  }

/* Successful compile. If the anchored option was not passed, set it if
we can determine that the pattern is anchored by virtue of ^ characters or \A
or anything else, such as starting with non-atomic .* when DOTALL is set and
there are no occurrences of *PRUNE or *SKIP (though there is an option to
disable this case). */

if ((re->overall_options & PCRE2_ANCHORED) == 0 &&
     is_anchored(codestart, 0, &cb, 0))
  re->overall_options |= PCRE2_ANCHORED;

/* If the pattern is still not anchored and we do not have a first code unit,
see if there is one that is asserted (these are not saved during the compile
because they can cause conflicts with actual literals that follow). This code
need not be obeyed if PCRE2_NO_START_OPTIMIZE is set, as the data it would
create will not be used. */

if ((re->overall_options & (PCRE2_ANCHORED|PCRE2_NO_START_OPTIMIZE)) == 0)
  {
  if (firstcuflags < 0)
    firstcu = find_firstassertedcu(codestart, &firstcuflags, FALSE);

  /* Save the data for a first code unit. */

  if (firstcuflags >= 0)
    {
    re->first_codeunit = firstcu;
    re->flags |= PCRE2_FIRSTSET;

    /* Handle caseless first code units. */

    if ((firstcuflags & REQ_CASELESS) != 0)
      {
      if (firstcu < 128 || (!utf && firstcu < 255))
        {
        if (cb.fcc[firstcu] != firstcu) re->flags |= PCRE2_FIRSTCASELESS;
        }

      /* The first code unit is > 128 in UTF mode, or > 255 otherwise. In
      8-bit UTF mode, codepoints in the range 128-255 are introductory code
      points and cannot have another case. In 16-bit and 32-bit modes, we can
      check wide characters when UTF (and therefore UCP) is supported. */

#if defined SUPPORT_UNICODE && PCRE2_CODE_UNIT_WIDTH != 8
      else if (firstcu <= MAX_UTF_CODE_POINT &&
               UCD_OTHERCASE(firstcu) != firstcu)
        re->flags |= PCRE2_FIRSTCASELESS;
#endif
      }
    }

  /* When there is no first code unit, see if we can set the PCRE2_STARTLINE
  flag. This is helpful for multiline matches when all branches start with ^
  and also when all branches start with non-atomic .* for non-DOTALL matches
  when *PRUNE and SKIP are not present. (There is an option that disables this
  case.) */

  else if (is_startline(codestart, 0, &cb, 0)) re->flags |= PCRE2_STARTLINE;
  }

/* Handle the "required code unit", if one is set. In the case of an anchored
pattern, do this only if it follows a variable length item in the pattern.
Again, skip this if PCRE2_NO_START_OPTIMIZE is set. */

if (reqcuflags >= 0 &&
     ((re->overall_options & (PCRE2_ANCHORED|PCRE2_NO_START_OPTIMIZE)) == 0 ||
      (reqcuflags & REQ_VARY) != 0))
  {
  re->last_codeunit = reqcu;
  re->flags |= PCRE2_LASTSET;

  /* Handle caseless required code units as for first code units (above). */

  if ((reqcuflags & REQ_CASELESS) != 0)
    {
    if (reqcu < 128 || (!utf && reqcu < 255))
      {
      if (cb.fcc[reqcu] != reqcu) re->flags |= PCRE2_LASTCASELESS;
      }
#if defined SUPPORT_UNICODE && PCRE2_CODE_UNIT_WIDTH != 8
    else if (reqcu <= MAX_UTF_CODE_POINT && UCD_OTHERCASE(reqcu) != reqcu)
      re->flags |= PCRE2_LASTCASELESS;
#endif
    }
  }

/* Check for a pattern than can match an empty string, so that this information
can be provided to applications. */

do
  {
  int count = 0;
  int rc = could_be_empty_branch(codestart, code, utf, &cb, TRUE, NULL, &count);
  if (rc < 0)
    {
    errorcode = ERR86;
    goto HAD_ERROR;
    }
  if (rc > 0)
    {
    re->flags |= PCRE2_MATCH_EMPTY;
    break;
    }
  codestart += GET(codestart, 1);
  }
while (*codestart == OP_ALT);

/* Finally, unless PCRE2_NO_START_OPTIMIZE is set, study the compiled pattern
to set up information such as a bitmap of starting code units and a minimum
matching length. */

if ((re->overall_options & PCRE2_NO_START_OPTIMIZE) == 0 &&
    PRIV(study)(re) != 0)
  {
  errorcode = ERR31;
  goto HAD_ERROR;
  }

/* Control ends up here in all cases. If memory was obtained for a
zero-terminated copy of the pattern, remember to free it before returning. Also
free the list of named groups if a larger one had to be obtained, and likewise
the group information vector. */

EXIT:
if (copied_pattern != stack_copied_pattern)
  ccontext->memctl.free(copied_pattern, ccontext->memctl.memory_data);
if (cb.named_group_list_size > NAMED_GROUP_LIST_SIZE)
  ccontext->memctl.free((void *)cb.named_groups, ccontext->memctl.memory_data);
if (cb.groupinfo != c32workspace)
  ccontext->memctl.free((void *)cb.groupinfo, ccontext->memctl.memory_data);

return re;    /* Will be NULL after an error */
}

/* End of pcre2_compile.c */
