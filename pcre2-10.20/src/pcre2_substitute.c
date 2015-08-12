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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pcre2_internal.h"


/*************************************************
*              Match and substitute              *
*************************************************/

/* This function applies a compiled re to a subject string and creates a new
string with substitutions. The first 7 arguments are the same as for
pcre2_match(). Either string length may be PCRE2_ZERO_TERMINATED.

Arguments:
  code            points to the compiled expression
  subject         points to the subject string
  length          length of subject string (may contain binary zeros)
  start_offset    where to start in the subject string
  options         option bits
  match_data      points to a match_data block, or is NULL
  context         points a PCRE2 context
  replacement     points to the replacement string
  rlength         length of replacement string
  buffer          where to put the substituted string
  blength         points to length of buffer; updated to length of string

Returns:          >= 0 number of substitutions made
                  < 0 an error code
                  PCRE2_ERROR_BADREPLACEMENT means invalid use of $
*/

PCRE2_EXP_DEFN int PCRE2_CALL_CONVENTION
pcre2_substitute(const pcre2_code *code, PCRE2_SPTR subject, PCRE2_SIZE length,
  PCRE2_SIZE start_offset, uint32_t options, pcre2_match_data *match_data,
  pcre2_match_context *mcontext, PCRE2_SPTR replacement, PCRE2_SIZE rlength,
  PCRE2_UCHAR *buffer, PCRE2_SIZE *blength)
{
int rc;
int subs;
uint32_t ovector_count;
uint32_t goptions = 0;
BOOL match_data_created = FALSE;
BOOL global = FALSE;
PCRE2_SIZE buff_offset, lengthleft, fraglength;
PCRE2_SIZE *ovector;

/* Partial matching is not valid. */

if ((options & (PCRE2_PARTIAL_HARD|PCRE2_PARTIAL_SOFT)) != 0)
  return PCRE2_ERROR_BADOPTION;

/* If no match data block is provided, create one. */

if (match_data == NULL)
  {
  pcre2_general_context *gcontext = (mcontext == NULL)?
    (pcre2_general_context *)code :
    (pcre2_general_context *)mcontext;
  match_data = pcre2_match_data_create_from_pattern(code, gcontext);
  if (match_data == NULL) return PCRE2_ERROR_NOMEMORY;
  match_data_created = TRUE;
  }
ovector = pcre2_get_ovector_pointer(match_data);
ovector_count = pcre2_get_ovector_count(match_data);

/* Check UTF replacement string if necessary. */

#ifdef SUPPORT_UNICODE
if ((code->overall_options & PCRE2_UTF) != 0 &&
    (options & PCRE2_NO_UTF_CHECK) == 0)
  {
  rc = PRIV(valid_utf)(replacement, rlength, &(match_data->rightchar));
  if (rc != 0)
    {
    match_data->leftchar = 0;
    goto EXIT;
    }
  }
#endif  /* SUPPORT_UNICODE */

/* Notice the global option and remove it from the options that are passed to
pcre2_match(). */

if ((options & PCRE2_SUBSTITUTE_GLOBAL) != 0)
  {
  options &= ~PCRE2_SUBSTITUTE_GLOBAL;
  global = TRUE;
  }

/* Find lengths of zero-terminated strings. */

if (length == PCRE2_ZERO_TERMINATED) length = PRIV(strlen)(subject);
if (rlength == PCRE2_ZERO_TERMINATED) rlength = PRIV(strlen)(replacement);

/* Copy up to the start offset */

if (start_offset > *blength) goto NOROOM;
memcpy(buffer, subject, start_offset * (PCRE2_CODE_UNIT_WIDTH/8));
buff_offset = start_offset;
lengthleft = *blength - start_offset;

/* Loop for global substituting. */

subs = 0;
do
  {
  PCRE2_SIZE i;

  rc = pcre2_match(code, subject, length, start_offset, options|goptions,
    match_data, mcontext);

  /* Any error other than no match returns the error code. No match when not
  doing the special after-empty-match global rematch, or when at the end of the
  subject, breaks the global loop. Otherwise, advance the starting point by one
  character, copying it to the output, and try again. */

  if (rc < 0)
    {
    PCRE2_SIZE save_start;

    if (rc != PCRE2_ERROR_NOMATCH) goto EXIT;
    if (goptions == 0 || start_offset >= length) break;

    save_start = start_offset++;
    if ((code->overall_options & PCRE2_UTF) != 0)
      {
#if PCRE2_CODE_UNIT_WIDTH == 8
      while (start_offset < length && (subject[start_offset] & 0xc0) == 0x80)
        start_offset++;
#elif PCRE2_CODE_UNIT_WIDTH == 16
      while (start_offset < length &&
            (subject[start_offset] & 0xfc00) == 0xdc00)
        start_offset++;
#endif
      }

    fraglength = start_offset - save_start;
    if (lengthleft < fraglength) goto NOROOM;
    memcpy(buffer + buff_offset, subject + save_start,
      fraglength*(PCRE2_CODE_UNIT_WIDTH/8));
    buff_offset += fraglength;
    lengthleft -= fraglength;

    goptions = 0;
    continue;
    }

  /* Handle a successful match. */

  subs++;
  if (rc == 0) rc = ovector_count;
  fraglength = ovector[0] - start_offset;
  if (fraglength >= lengthleft) goto NOROOM;
  memcpy(buffer + buff_offset, subject + start_offset,
    fraglength*(PCRE2_CODE_UNIT_WIDTH/8));
  buff_offset += fraglength;
  lengthleft -= fraglength;

  for (i = 0; i < rlength; i++)
    {
    if (replacement[i] == CHAR_DOLLAR_SIGN)
      {
      int group, n;
      BOOL inparens;
      PCRE2_SIZE sublength;
      PCRE2_UCHAR next;
      PCRE2_UCHAR name[33];

      if (++i == rlength) goto BAD;
      if ((next = replacement[i]) == CHAR_DOLLAR_SIGN) goto LITERAL;

      group = -1;
      n = 0;
      inparens = FALSE;

      if (next == CHAR_LEFT_CURLY_BRACKET)
        {
        if (++i == rlength) goto BAD;
        next = replacement[i];
        inparens = TRUE;
        }

      if (next >= CHAR_0 && next <= CHAR_9)
        {
        group = next - CHAR_0;
        while (++i < rlength)
          {
          next = replacement[i];
          if (next < CHAR_0 || next > CHAR_9) break;
          group = group * 10 + next - CHAR_0;
          }
        }
      else
        {
        const uint8_t *ctypes = code->tables + ctypes_offset;
        while (MAX_255(next) && (ctypes[next] & ctype_word) != 0)
          {
          name[n++] = next;
          if (n > 32) goto BAD;
          if (i == rlength) break;
          next = replacement[++i];
          }
        if (n == 0) goto BAD;
        name[n] = 0;
        }

      if (inparens)
        {
        if (i == rlength || next != CHAR_RIGHT_CURLY_BRACKET) goto BAD;
        }
      else i--;   /* Last code unit of name/number */

      /* Have found a syntactically correct group number or name. */

      sublength = lengthleft;
      if (group < 0)
        rc = pcre2_substring_copy_byname(match_data, name,
          buffer + buff_offset, &sublength);
      else
        rc = pcre2_substring_copy_bynumber(match_data, group,
          buffer + buff_offset, &sublength);

      if (rc < 0) goto EXIT;
      buff_offset += sublength;
      lengthleft -= sublength;
      }

   /* Handle a literal code unit */

   else
      {
      LITERAL:
      if (lengthleft-- < 1) goto NOROOM;
      buffer[buff_offset++] = replacement[i];
      }
    }

  /* The replacement has been copied to the output. Update the start offset to
  point to the rest of the subject string. If we matched an empty string,
  do the magic for global matches. */

  start_offset = ovector[1];
  goptions = (ovector[0] != ovector[1])? 0 :
    PCRE2_ANCHORED|PCRE2_NOTEMPTY_ATSTART;
  } while (global);  /* Repeat "do" loop */

/* Copy the rest of the subject and return the number of substitutions. */

rc = subs;
fraglength = length - start_offset;
if (fraglength + 1 > lengthleft) goto NOROOM;
memcpy(buffer + buff_offset, subject + start_offset,
  fraglength*(PCRE2_CODE_UNIT_WIDTH/8));
buff_offset += fraglength;
buffer[buff_offset] = 0;
*blength = buff_offset;

EXIT:
if (match_data_created) pcre2_match_data_free(match_data);
  else match_data->rc = rc;
return rc;

NOROOM:
rc = PCRE2_ERROR_NOMEMORY;
goto EXIT;

BAD:
rc = PCRE2_ERROR_BADREPLACEMENT;
goto EXIT;
}

/* End of pcre2_substitute.c */
