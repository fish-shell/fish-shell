/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/

/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
     Original API code Copyright (c) 1997-2012 University of Cambridge
          New API code Copyright (c) 2016-2020 University of Cambridge

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


/* This is a freestanding support program to generate a file containing
character tables for PCRE2. The tables are built using the pcre2_maketables()
function, which is part of the PCRE2 API. By default, the system's "C" locale
is used rather than what the building user happens to have set, but the -L
option can be used to select the current locale from the LC_ALL environment
variable. By default, the tables are written in source form, but if -b is
given, they are written in binary. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#define PCRE2_CODE_UNIT_WIDTH 0   /* Must be set, but not relevant here */
#include "pcre2_internal.h"

#define PCRE2_DFTABLES            /* pcre2_maketables.c notices this */
#include "pcre2_maketables.c"


static const char *classlist[] =
  {
  "space", "xdigit", "digit", "upper", "lower",
  "word", "graph", "print", "punct", "cntrl"
  };



/*************************************************
*                  Usage                         *
*************************************************/

static void
usage(void)
{
(void)fprintf(stderr,
  "Usage: pcre2_dftables [options] <output file>\n"
  "  -b    Write output in binary (default is source code)\n"
  "  -L    Use locale from LC_ALL (default is \"C\" locale)\n"
  );
}



/*************************************************
*                Entry point                     *
*************************************************/

int main(int argc, char **argv)
{
FILE *f;
int i;
int nclass = 0;
BOOL binary = FALSE;
char *env = (char *)"C";
const unsigned char *tables;
const unsigned char *base_of_tables;

/* Process options */

for (i = 1; i < argc; i++)
  {
  char *arg = argv[i];
  if (*arg != '-') break;

  if (strcmp(arg, "-help") == 0 || strcmp(arg, "--help") == 0)
    {
    usage();
    return 0;
    }

  else if (strcmp(arg, "-L") == 0)
    {
    if (setlocale(LC_ALL, "") == NULL)
      {
      (void)fprintf(stderr, "pcre2_dftables: setlocale() failed\n");
      return 1;
      }
    env = getenv("LC_ALL");
    }

  else if (strcmp(arg, "-b") == 0)
    binary = TRUE;

  else
    {
    (void)fprintf(stderr, "pcre2_dftables: unrecognized option %s\n", arg);
    return 1;
    }
  }

if (i != argc - 1)
  {
  (void)fprintf(stderr, "pcre2_dftables: one filename argument is required\n");
  return 1;
  }

/* Make the tables */

tables = maketables();
base_of_tables = tables;

f = fopen(argv[i], "wb");
if (f == NULL)
  {
  fprintf(stderr, "pcre2_dftables: failed to open %s for writing\n", argv[1]);
  return 1;
  }

/* If -b was specified, we write the tables in binary. */

if (binary)
  {
  int yield = 0;
  size_t len = fwrite(tables, 1, TABLES_LENGTH, f);
  if (len != TABLES_LENGTH)
    {
    (void)fprintf(stderr, "pcre2_dftables: fwrite() returned wrong length %d "
     "instead of %d\n", (int)len, TABLES_LENGTH);
    yield = 1;
    }
  fclose(f);
  free((void *)base_of_tables);
  return yield;
  }

/* Write the tables as source code for inclusion in the PCRE2 library. There
are several fprintf() calls here, because gcc in pedantic mode complains about
the very long string otherwise. */

(void)fprintf(f,
  "/*************************************************\n"
  "*      Perl-Compatible Regular Expressions       *\n"
  "*************************************************/\n\n"
  "/* This file was automatically written by the pcre2_dftables auxiliary\n"
  "program. It contains character tables that are used when no external\n"
  "tables are passed to PCRE2 by the application that calls it. The tables\n"
  "are used only for characters whose code values are less than 256. */\n\n");

(void)fprintf(f,
  "/* This set of tables was written in the %s locale. */\n\n", env);

(void)fprintf(f,
  "/* The pcre2_ftables program (which is distributed with PCRE2) can be used\n"
  "to build alternative versions of this file. This is necessary if you are\n"
  "running in an EBCDIC environment, or if you want to default to a different\n"
  "encoding, for example ISO-8859-1. When pcre2_dftables is run, it creates\n"
  "these tables in the \"C\" locale by default. This happens automatically if\n"
  "PCRE2 is configured with --enable-rebuild-chartables. However, you can run\n"
  "pcre2_dftables manually with the -L option to build tables using the LC_ALL\n"
  "locale. */\n\n");

/* Force config.h in z/OS */

#if defined NATIVE_ZOS
(void)fprintf(f,
  "/* For z/OS, config.h is forced */\n"
  "#ifndef HAVE_CONFIG_H\n"
  "#define HAVE_CONFIG_H 1\n"
  "#endif\n\n");
#endif

(void)fprintf(f,
  "/* The following #include is present because without it gcc 4.x may remove\n"
  "the array definition from the final binary if PCRE2 is built into a static\n"
  "library and dead code stripping is activated. This leads to link errors.\n"
  "Pulling in the header ensures that the array gets flagged as \"someone\n"
  "outside this compilation unit might reference this\" and so it will always\n"
  "be supplied to the linker. */\n\n");

(void)fprintf(f,
  "#ifdef HAVE_CONFIG_H\n"
  "#include \"config.h\"\n"
  "#endif\n\n"
  "#include \"pcre2_internal.h\"\n\n");

(void)fprintf(f,
  "const uint8_t PRIV(default_tables)[] = {\n\n"
  "/* This table is a lower casing table. */\n\n");

(void)fprintf(f, "  ");
for (i = 0; i < 256; i++)
  {
  if ((i & 7) == 0 && i != 0) fprintf(f, "\n  ");
  fprintf(f, "%3d", *tables++);
  if (i != 255) fprintf(f, ",");
  }
(void)fprintf(f, ",\n\n");

(void)fprintf(f, "/* This table is a case flipping table. */\n\n");

(void)fprintf(f, "  ");
for (i = 0; i < 256; i++)
  {
  if ((i & 7) == 0 && i != 0) fprintf(f, "\n  ");
  fprintf(f, "%3d", *tables++);
  if (i != 255) fprintf(f, ",");
  }
(void)fprintf(f, ",\n\n");

(void)fprintf(f,
  "/* This table contains bit maps for various character classes. Each map is 32\n"
  "bytes long and the bits run from the least significant end of each byte. The\n"
  "classes that have their own maps are: space, xdigit, digit, upper, lower, word,\n"
  "graph, print, punct, and cntrl. Other classes are built from combinations. */\n\n");

(void)fprintf(f, "  ");
for (i = 0; i < cbit_length; i++)
  {
  if ((i & 7) == 0 && i != 0)
    {
    if ((i & 31) == 0) (void)fprintf(f, "\n");
    if ((i & 24) == 8) (void)fprintf(f, "  /* %s */", classlist[nclass++]);
    (void)fprintf(f, "\n  ");
    }
  (void)fprintf(f, "0x%02x", *tables++);
  if (i != cbit_length - 1) (void)fprintf(f, ",");
  }
(void)fprintf(f, ",\n\n");

(void)fprintf(f,
  "/* This table identifies various classes of character by individual bits:\n"
  "  0x%02x   white space character\n"
  "  0x%02x   letter\n"
  "  0x%02x   lower case letter\n"
  "  0x%02x   decimal digit\n"
  "  0x%02x   alphanumeric or '_'\n*/\n\n",
  ctype_space, ctype_letter, ctype_lcletter, ctype_digit, ctype_word);

(void)fprintf(f, "  ");
for (i = 0; i < 256; i++)
  {
  if ((i & 7) == 0 && i != 0)
    {
    (void)fprintf(f, " /* ");
    if (isprint(i-8)) (void)fprintf(f, " %c -", i-8);
      else (void)fprintf(f, "%3d-", i-8);
    if (isprint(i-1)) (void)fprintf(f, " %c ", i-1);
      else (void)fprintf(f, "%3d", i-1);
    (void)fprintf(f, " */\n  ");
    }
  (void)fprintf(f, "0x%02x", *tables++);
  if (i != 255) (void)fprintf(f, ",");
  }

(void)fprintf(f, "};/* ");
if (isprint(i-8)) (void)fprintf(f, " %c -", i-8);
  else (void)fprintf(f, "%3d-", i-8);
if (isprint(i-1)) (void)fprintf(f, " %c ", i-1);
  else (void)fprintf(f, "%3d", i-1);
(void)fprintf(f, " */\n\n/* End of pcre2_chartables.c */\n");

fclose(f);
free((void *)base_of_tables);
return 0;
}

/* End of pcre2_dftables.c */
