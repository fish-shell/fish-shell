/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/

/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
     Original API code Copyright (c) 1997-2012 University of Cambridge
         New API code Copyright (c) 2014 University of Cambridge

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


/* This module is a wrapper that provides a POSIX API to the underlying PCRE2
functions. */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/* Ensure that the PCRE2POSIX_EXP_xxx macros are set appropriately for
compiling these functions. This must come before including pcre2posix.h, where
they are set for an application (using these functions) if they have not
previously been set. */

#if defined(_WIN32) && !defined(PCRE2_STATIC)
#  define PCRE2POSIX_EXP_DECL extern __declspec(dllexport)
#  define PCRE2POSIX_EXP_DEFN __declspec(dllexport)
#endif

/* We include pcre2.h before pcre2_internal.h so that the PCRE2 library
functions are declared as "import" for Windows by defining PCRE2_EXP_DECL as
"import". This is needed even though pcre2_internal.h itself includes pcre2.h,
because it does so after it has set PCRE2_EXP_DECL to "export" if it is not
already set. */

#include "pcre2.h"
#include "pcre2_internal.h"
#include "pcre2posix.h"

/* Table to translate PCRE2 compile time error codes into POSIX error codes.
Only a few PCRE2 errors with a value greater than 23 turn into special POSIX
codes: most go to REG_BADPAT. The second table lists, in pairs, those that
don't. */

static const int eint1[] = {
  0,           /* No error */
  REG_EESCAPE, /* \ at end of pattern */
  REG_EESCAPE, /* \c at end of pattern */
  REG_EESCAPE, /* unrecognized character follows \ */
  REG_BADBR,   /* numbers out of order in {} quantifier */
  /* 5 */
  REG_BADBR,   /* number too big in {} quantifier */
  REG_EBRACK,  /* missing terminating ] for character class */
  REG_ECTYPE,  /* invalid escape sequence in character class */
  REG_ERANGE,  /* range out of order in character class */
  REG_BADRPT,  /* nothing to repeat */
  /* 10 */
  REG_ASSERT,  /* internal error: unexpected repeat */
  REG_BADPAT,  /* unrecognized character after (? or (?- */
  REG_BADPAT,  /* POSIX named classes are supported only within a class */
  REG_BADPAT,  /* POSIX collating elements are not supported */
  REG_EPAREN,  /* missing ) */
  /* 15 */
  REG_ESUBREG, /* reference to non-existent subpattern */
  REG_INVARG,  /* pattern passed as NULL */
  REG_INVARG,  /* unknown compile-time option bit(s) */
  REG_EPAREN,  /* missing ) after (?# comment */
  REG_ESIZE,   /* parentheses nested too deeply */
  /* 20 */
  REG_ESIZE,   /* regular expression too large */
  REG_ESPACE,  /* failed to get memory */
  REG_EPAREN,  /* unmatched closing parenthesis */
  REG_ASSERT   /* internal error: code overflow */
  };

static const int eint2[] = {
  30, REG_ECTYPE,  /* unknown POSIX class name */
  32, REG_INVARG,  /* this version of PCRE does not have UTF or UCP support */
  37, REG_EESCAPE, /* PCRE2 does not support \L, \l, \N{name}, \U, or \u */
  56, REG_INVARG,  /* internal error: unknown newline setting */
};

/* Table of texts corresponding to POSIX error codes */

static const char *const pstring[] = {
  "",                                /* Dummy for value 0 */
  "internal error",                  /* REG_ASSERT */
  "invalid repeat counts in {}",     /* BADBR      */
  "pattern error",                   /* BADPAT     */
  "? * + invalid",                   /* BADRPT     */
  "unbalanced {}",                   /* EBRACE     */
  "unbalanced []",                   /* EBRACK     */
  "collation error - not relevant",  /* ECOLLATE   */
  "bad class",                       /* ECTYPE     */
  "bad escape sequence",             /* EESCAPE    */
  "empty expression",                /* EMPTY      */
  "unbalanced ()",                   /* EPAREN     */
  "bad range inside []",             /* ERANGE     */
  "expression too big",              /* ESIZE      */
  "failed to get memory",            /* ESPACE     */
  "bad back reference",              /* ESUBREG    */
  "bad argument",                    /* INVARG     */
  "match failed"                     /* NOMATCH    */
};




/*************************************************
*          Translate error code to string        *
*************************************************/

PCRE2POSIX_EXP_DEFN size_t PCRE2_CALL_CONVENTION
regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
const char *message, *addmessage;
size_t length, addlength;

message = (errcode >= (int)(sizeof(pstring)/sizeof(char *)))?
  "unknown error code" : pstring[errcode];
length = strlen(message) + 1;

addmessage = " at offset ";
addlength = (preg != NULL && (int)preg->re_erroffset != -1)?
  strlen(addmessage) + 6 : 0;

if (errbuf_size > 0)
  {
  if (addlength > 0 && errbuf_size >= length + addlength)
    sprintf(errbuf, "%s%s%-6d", message, addmessage, (int)preg->re_erroffset);
  else
    {
    strncpy(errbuf, message, errbuf_size - 1);
    errbuf[errbuf_size-1] = 0;
    }
  }

return length + addlength;
}




/*************************************************
*           Free store held by a regex           *
*************************************************/

PCRE2POSIX_EXP_DEFN void PCRE2_CALL_CONVENTION
regfree(regex_t *preg)
{
pcre2_match_data_free(preg->re_match_data);
pcre2_code_free(preg->re_pcre2_code);
}




/*************************************************
*            Compile a regular expression        *
*************************************************/

/*
Arguments:
  preg        points to a structure for recording the compiled expression
  pattern     the pattern to compile
  cflags      compilation flags

Returns:      0 on success
              various non-zero codes on failure
*/

PCRE2POSIX_EXP_DEFN int PCRE2_CALL_CONVENTION
regcomp(regex_t *preg, const char *pattern, int cflags)
{
PCRE2_SIZE erroffset;
int errorcode;
int options = 0;
int re_nsub = 0;

if ((cflags & REG_ICASE) != 0)    options |= PCRE2_CASELESS;
if ((cflags & REG_NEWLINE) != 0)  options |= PCRE2_MULTILINE;
if ((cflags & REG_DOTALL) != 0)   options |= PCRE2_DOTALL;
if ((cflags & REG_NOSUB) != 0)    options |= PCRE2_NO_AUTO_CAPTURE;
if ((cflags & REG_UTF) != 0)      options |= PCRE2_UTF;
if ((cflags & REG_UCP) != 0)      options |= PCRE2_UCP;
if ((cflags & REG_UNGREEDY) != 0) options |= PCRE2_UNGREEDY;

preg->re_pcre2_code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
   options, &errorcode, &erroffset, NULL);
preg->re_erroffset = erroffset;

if (preg->re_pcre2_code == NULL)
  {
  unsigned int i;
  if (errorcode < 0) return REG_BADPAT;   /* UTF error */
  errorcode -= COMPILE_ERROR_BASE;
  if (errorcode < (int)(sizeof(eint1)/sizeof(const int)))
    return eint1[errorcode];
  for (i = 0; i < sizeof(eint2)/(2*sizeof(const int)); i += 2)
    if (errorcode == eint2[i]) return eint2[i+1];
  return REG_BADPAT;
  }

(void)pcre2_pattern_info((const pcre2_code *)preg->re_pcre2_code,
  PCRE2_INFO_CAPTURECOUNT, &re_nsub);
preg->re_nsub = (size_t)re_nsub;
if ((options & PCRE2_NO_AUTO_CAPTURE) != 0) re_nsub = -1;
preg->re_match_data = pcre2_match_data_create(re_nsub + 1, NULL);
return 0;
}



/*************************************************
*              Match a regular expression        *
*************************************************/

/* A suitable match_data block, large enough to hold all possible captures, was
obtained when the pattern was compiled, to save having to allocate and free it
for each match. If REG_NOSUB was specified at compile time, the
PCRE_NO_AUTO_CAPTURE flag will be set. When this is the case, the nmatch and
pmatch arguments are ignored, and the only result is yes/no/error. */

PCRE2POSIX_EXP_DEFN int PCRE2_CALL_CONVENTION
regexec(const regex_t *preg, const char *string, size_t nmatch,
  regmatch_t pmatch[], int eflags)
{
int rc, so, eo;
int options = 0;
pcre2_match_data *md = (pcre2_match_data *)preg->re_match_data;

if ((eflags & REG_NOTBOL) != 0) options |= PCRE2_NOTBOL;
if ((eflags & REG_NOTEOL) != 0) options |= PCRE2_NOTEOL;
if ((eflags & REG_NOTEMPTY) != 0) options |= PCRE2_NOTEMPTY;

((regex_t *)preg)->re_erroffset = (size_t)(-1);  /* Only has meaning after compile */

/* When no string data is being returned, or no vector has been passed in which
to put it, ensure that nmatch is zero. */

if ((((pcre2_real_code *)(preg->re_pcre2_code))->compile_options &
  PCRE2_NO_AUTO_CAPTURE) != 0 || pmatch == NULL) nmatch = 0;

/* REG_STARTEND is a BSD extension, to allow for non-NUL-terminated strings.
The man page from OS X says "REG_STARTEND affects only the location of the
string, not how it is matched". That is why the "so" value is used to bump the
start location rather than being passed as a PCRE2 "starting offset". */

if ((eflags & REG_STARTEND) != 0)
  {
  so = pmatch[0].rm_so;
  eo = pmatch[0].rm_eo;
  }
else
  {
  so = 0;
  eo = (int)strlen(string);
  }

rc = pcre2_match((const pcre2_code *)preg->re_pcre2_code,
  (PCRE2_SPTR)string + so, (eo - so), 0, options, md, NULL);

/* Successful match */

if (rc >= 0)
  {
  size_t i;
  if ((size_t)rc > nmatch) rc = (int)nmatch;
  for (i = 0; i < (size_t)rc; i++)
    {
    pmatch[i].rm_so = md->ovector[i*2];
    pmatch[i].rm_eo = md->ovector[i*2+1];
    }
  for (; i < nmatch; i++) pmatch[i].rm_so = pmatch[i].rm_eo = -1;
  return 0;
  }

/* Unsuccessful match */

if (rc <= PCRE2_ERROR_UTF8_ERR1 && rc >= PCRE2_ERROR_UTF8_ERR21)
  return REG_INVARG;

switch(rc)
  {
  default: return REG_ASSERT;
  case PCRE2_ERROR_BADMODE: return REG_INVARG;
  case PCRE2_ERROR_BADMAGIC: return REG_INVARG;
  case PCRE2_ERROR_BADOPTION: return REG_INVARG;
  case PCRE2_ERROR_BADUTFOFFSET: return REG_INVARG;
  case PCRE2_ERROR_MATCHLIMIT: return REG_ESPACE;
  case PCRE2_ERROR_NOMATCH: return REG_NOMATCH;
  case PCRE2_ERROR_NOMEMORY: return REG_ESPACE;
  case PCRE2_ERROR_NULL: return REG_INVARG;
  }
}

/* End of pcre2posix.c */
