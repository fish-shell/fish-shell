/** \file fish_tests.c
  Various bug and feature tests. Compiled and run by make test.
*/

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <signal.h>

#include <locale.h>
#include <dirent.h>
#include <time.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "proc.h"
#include "reader.h"
#include "builtin.h"
#include "function.h"
#include "autoload.h"
#include "complete.h"
#include "wutil.h"
#include "env.h"
#include "expand.h"
#include "parser.h"
#include "tokenizer.h"
#include "output.h"
#include "exec.h"
#include "event.h"
#include "path.h"
#include "history.h"
#include "highlight.h"
#include "iothread.h"
#include "postfork.h"
#include "signal.h"
#include "highlight.h"
#include "parse_util.h"

/**
   The number of tests to run
 */
//#define ESCAPE_TEST_COUNT 1000000
#define ESCAPE_TEST_COUNT 10000
/**
   The average length of strings to unescape
 */
#define ESCAPE_TEST_LENGTH 100
/**
   The higest character number of character to try and escape
 */
#define ESCAPE_TEST_CHAR 4000

/**
   Number of laps to run performance testing loop
*/
#define LAPS 50

/**
   The result of one of the test passes
*/
#define NUM_ANS L"-7 99999999 1234567 deadbeef DEADBEEFDEADBEEF"

/**
   Number of encountered errors
*/
static int err_count=0;

/**
   Print formatted output
*/
static void say(const wchar_t *blah, ...)
{
    va_list va;
    va_start(va, blah);
    vwprintf(blah, va);
    va_end(va);
    wprintf(L"\n");
}

/**
   Print formatted error string
*/
static void err(const wchar_t *blah, ...)
{
    va_list va;
    va_start(va, blah);
    err_count++;

    wprintf(L"Error: ");
    vwprintf(blah, va);
    va_end(va);
    wprintf(L"\n");
}

/**
   Test the escaping/unescaping code by escaping/unescaping random
   strings and verifying that the original string comes back.
*/
static void test_escape()
{
    int i;
    wcstring sb;

    say(L"Testing escaping and unescaping");

    for (i=0; i<ESCAPE_TEST_COUNT; i++)
    {
        const wchar_t *o, *e, *u;

        sb.clear();
        while (rand() % ESCAPE_TEST_LENGTH)
        {
            sb.push_back((rand() %ESCAPE_TEST_CHAR) +1);
        }
        o = (const wchar_t *)sb.c_str();
        e = escape(o, 1);
        u = unescape(e, 0);
        if (!o || !e || !u)
        {
            err(L"Escaping cycle of string %ls produced null pointer on %ls", o, e?L"unescaping":L"escaping");

        }


        if (wcscmp(o, u))
        {
            err(L"Escaping cycle of string %ls produced different string %ls", o, u);


        }
        free((void *)e);
        free((void *)u);

    }
}

static void test_format(void)
{
    say(L"Testing formatting functions");
    struct
    {
        unsigned long long val;
        const char *expected;
    } tests[] =
    {
        { 0, "empty" },
        { 1, "1B" },
        { 2, "2B" },
        { 1024, "1kB" },
        { 1870, "1.8kB" },
        { 4322911, "4.1MB" }
    };
    size_t i;
    for (i=0; i < sizeof tests / sizeof *tests; i++)
    {
        char buff[128];
        format_size_safe(buff, tests[i].val);
        assert(! strcmp(buff, tests[i].expected));
    }

    for (int j=-129; j <= 129; j++)
    {
        char buff1[128], buff2[128];
        format_long_safe(buff1, j);
        sprintf(buff2, "%d", j);
        assert(! strcmp(buff1, buff2));
    }

    long q = LONG_MIN;
    char buff1[128], buff2[128];
    format_long_safe(buff1, q);
    sprintf(buff2, "%ld", q);
    assert(! strcmp(buff1, buff2));

}

/**
   Test wide/narrow conversion by creating random strings and
   verifying that the original string comes back thorugh double
   conversion.
*/
static void test_convert()
{
    /*  char o[] =
        {
          -17, -128, -121, -68, 0
        }
      ;

      wchar_t *w = str2wcs(o);
      char *n = wcs2str(w);

      int i;

      for( i=0; o[i]; i++ )
      {
        bitprint(o[i]);;
        //wprintf(L"%d ", o[i]);
      }
      wprintf(L"\n");

      for( i=0; w[i]; i++ )
      {
        wbitprint(w[i]);;
        //wprintf(L"%d ", w[i]);
      }
      wprintf(L"\n");

      for( i=0; n[i]; i++ )
      {
        bitprint(n[i]);;
        //wprintf(L"%d ", n[i]);
      }
      wprintf(L"\n");

      return;
    */


    int i;
    std::vector<char> sb;

    say(L"Testing wide/narrow string conversion");

    for (i=0; i<ESCAPE_TEST_COUNT; i++)
    {
        const char *o, *n;

        char c;

        sb.clear();

        while (rand() % ESCAPE_TEST_LENGTH)
        {
            c = rand();
            sb.push_back(c);
        }
        c = 0;
        sb.push_back(c);

        o = &sb.at(0);
        const wcstring w = str2wcstring(o);
        n = wcs2str(w.c_str());

        if (!o || !n)
        {
            err(L"Line %d - Conversion cycle of string %s produced null pointer on %s", __LINE__, o, L"wcs2str");
        }

        if (strcmp(o, n))
        {
            err(L"Line %d - %d: Conversion cycle of string %s produced different string %s", __LINE__, i, o, n);
        }
        free((void *)n);

    }
}

/* Verify correct behavior with embedded nulls */
static void test_convert_nulls(void)
{
    say(L"Testing embedded nulls in string conversion");
    const wchar_t in[] = L"AAA\0BBB";
    const size_t in_len = (sizeof in / sizeof *in) - 1;
    const wcstring in_str = wcstring(in, in_len);
    std::string out_str = wcs2string(in_str);
    if (out_str.size() != in_len)
    {
        err(L"Embedded nulls mishandled in wcs2string");
    }
    for (size_t i=0; i < in_len; i++)
    {
        if (in[i] != out_str.at(i))
        {
            err(L"Embedded nulls mishandled in wcs2string at index %lu", (unsigned long)i);
        }
    }

    wcstring out_wstr = str2wcstring(out_str);
    if (out_wstr.size() != in_len)
    {
        err(L"Embedded nulls mishandled in str2wcstring");
    }
    for (size_t i=0; i < in_len; i++)
    {
        if (in[i] != out_wstr.at(i))
        {
            err(L"Embedded nulls mishandled in str2wcstring at index %lu", (unsigned long)i);
        }
    }

}

/**
   Test the tokenizer
*/
static void test_tok()
{

    say(L"Testing tokenizer");


    say(L"Testing invalid input");
    tokenizer_t t(NULL, 0);

    if (tok_last_type(&t) != TOK_ERROR)
    {
        err(L"Invalid input to tokenizer was undetected");
    }

    say(L"Testing use of broken tokenizer");
    if (!tok_has_next(&t))
    {
        err(L"tok_has_next() should return 1 once on broken tokenizer");
    }

    tok_next(&t);
    if (tok_last_type(&t) != TOK_ERROR)
    {
        err(L"Invalid input to tokenizer was undetected");
    }

    /*
      This should crash if there is a bug. No reliable way to detect otherwise.
    */
    say(L"Test destruction of broken tokenizer");
    {

        const wchar_t *str = L"string <redirection  2>&1 'nested \"quoted\" '(string containing subshells ){and,brackets}$as[$well (as variable arrays)] not_a_redirect^ ^ ^^is_a_redirect";
        const int types[] =
        {
            TOK_STRING, TOK_REDIRECT_IN, TOK_STRING, TOK_REDIRECT_FD, TOK_STRING, TOK_STRING, TOK_STRING, TOK_REDIRECT_OUT, TOK_REDIRECT_APPEND, TOK_STRING, TOK_END
        };

        say(L"Test correct tokenization");

        tokenizer_t t(str, 0);
        for (size_t i=0; i < sizeof types / sizeof *types; i++, tok_next(&t))
        {
            if (types[i] != tok_last_type(&t))
            {
                err(L"Tokenization error:");
                wprintf(L"Token number %d of string \n'%ls'\n, expected token type %ls, got token '%ls' of type %ls\n",
                        i+1,
                        str,
                        tok_get_desc(types[i]),
                        tok_last(&t),
                        tok_get_desc(tok_last_type(&t)));
            }
        }
    }
}

static int test_fork_helper(void *unused)
{
    size_t i;
    for (i=0; i < 1000; i++)
    {
        //delete [](new char[4 * 1024 * 1024]);
        for (int j=0; j < 1024; j++)
        {
            strerror(j);
        }
    }
    return 0;
}

static void test_fork(void)
{
    return;
    say(L"Testing fork");
    size_t i, max = 100;
    for (i=0; i < 100; i++)
    {
        printf("%lu / %lu\n", i+1, max);
        /* Do something horrible to try to trigger an error */
#define THREAD_COUNT 8
#define FORK_COUNT 10
#define FORK_LOOP_COUNT 16
        signal_block();
        for (size_t i=0; i < THREAD_COUNT; i++)
        {
            iothread_perform<void>(test_fork_helper, NULL, NULL);
        }
        for (size_t q = 0; q < FORK_LOOP_COUNT; q++)
        {
            pid_t pids[FORK_COUNT];
            for (size_t i=0; i < FORK_COUNT; i++)
            {
                pid_t pid = execute_fork(false);
                if (pid > 0)
                {
                    /* Parent */
                    pids[i] = pid;
                }
                else if (pid == 0)
                {
                    /* Child */
                    //new char[4 * 1024 * 1024];
                    for (size_t i=0; i < 1024 * 16; i++)
                    {
                        for (int j=0; j < 256; j++)
                        {
                            strerror(j);
                        }
                    }
                    exit_without_destructors(0);
                }
                else
                {
                    perror("fork");
                }
            }
            for (size_t i=0; i < FORK_COUNT; i++)
            {
                int status = 0;
                if (pids[i] != waitpid(pids[i], &status, 0))
                {
                    perror("waitpid");
                    assert(0);
                }
                assert(WIFEXITED(status) && 0 == WEXITSTATUS(status));
            }
        }
        iothread_drain_all();
        signal_unblock();
    }
#undef FORK_COUNT
}

/**
   Test the parser
*/
static void test_parser()
{
    say(L"Testing parser");

    parser_t parser(PARSER_TYPE_GENERAL, true);

    say(L"Testing null input to parser");
    if (!parser.test(NULL))
    {
        err(L"Null input to parser.test undetected");
    }

    say(L"Testing block nesting");
    if (!parser.test(L"if; end"))
    {
        err(L"Incomplete if statement undetected");
    }
    if (!parser.test(L"if test; echo"))
    {
        err(L"Missing end undetected");
    }
    if (!parser.test(L"if test; end; end"))
    {
        err(L"Unbalanced end undetected");
    }

    say(L"Testing detection of invalid use of builtin commands");
    if (!parser.test(L"case foo"))
    {
        err(L"'case' command outside of block context undetected");
    }
    if (!parser.test(L"switch ggg; if true; case foo;end;end"))
    {
        err(L"'case' command outside of switch block context undetected");
    }
    if (!parser.test(L"else"))
    {
        err(L"'else' command outside of conditional block context undetected");
    }
    if (!parser.test(L"else if"))
    {
        err(L"'else if' command outside of conditional block context undetected");
    }
    if (!parser.test(L"if false; else if; end"))
    {
        err(L"'else if' missing command undetected");
    }

    if (!parser.test(L"break"))
    {
        err(L"'break' command outside of loop block context undetected");
    }
    if (!parser.test(L"exec ls|less") || !parser.test(L"echo|return"))
    {
        err(L"Invalid pipe command undetected");
    }

    say(L"Testing basic evaluation");
#if 0
    /* This fails now since the parser takes a wcstring&, and NULL converts to wchar_t * converts to wcstring which crashes (thanks C++) */
    if (!parser.eval(0, 0, TOP))
    {
        err(L"Null input when evaluating undetected");
    }
#endif
    if (!parser.eval(L"ls", io_chain_t(), WHILE))
    {
        err(L"Invalid block mode when evaluating undetected");
    }
}

static void test_utils()
{
    say(L"Testing utils");
    const wchar_t *a = L"echo (echo (echo hi";
    
    const wchar_t *begin = NULL, *end = NULL;
    parse_util_cmdsubst_extent(a, 0, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    parse_util_cmdsubst_extent(a, 1, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    parse_util_cmdsubst_extent(a, 2, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    parse_util_cmdsubst_extent(a, 3, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    
    parse_util_cmdsubst_extent(a, 8, &begin, &end);
    if (begin != a + wcslen(L"echo (")) err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);

    parse_util_cmdsubst_extent(a, 17, &begin, &end);
    if (begin != a + wcslen(L"echo (echo (")) err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
}

class lru_node_test_t : public lru_node_t
{
public:
    lru_node_test_t(const wcstring &tmp) : lru_node_t(tmp) { }
};

class test_lru_t : public lru_cache_t<lru_node_test_t>
{
public:
    test_lru_t() : lru_cache_t<lru_node_test_t>(16) { }

    std::vector<lru_node_test_t *> evicted_nodes;

    virtual void node_was_evicted(lru_node_test_t *node)
    {
        assert(find(evicted_nodes.begin(), evicted_nodes.end(), node) == evicted_nodes.end());
        evicted_nodes.push_back(node);
    }
};

static void test_lru(void)
{
    say(L"Testing LRU cache");

    test_lru_t cache;
    std::vector<lru_node_test_t *> expected_evicted;
    size_t total_nodes = 20;
    for (size_t i=0; i < total_nodes; i++)
    {
        assert(cache.size() == std::min(i, (size_t)16));
        lru_node_test_t *node = new lru_node_test_t(to_string(i));
        if (i < 4) expected_evicted.push_back(node);
        // Adding the node the first time should work, and subsequent times should fail
        assert(cache.add_node(node));
        assert(! cache.add_node(node));
    }
    assert(cache.evicted_nodes == expected_evicted);
    cache.evict_all_nodes();
    assert(cache.evicted_nodes.size() == total_nodes);
    while (! cache.evicted_nodes.empty())
    {
        lru_node_t *node = cache.evicted_nodes.back();
        cache.evicted_nodes.pop_back();
        delete node;
    }
}

/**
   Perform parameter expansion and test if the output equals the zero-terminated parameter list supplied.

   \param in the string to expand
   \param flags the flags to send to expand_string
*/

static int expand_test(const wchar_t *in, int flags, ...)
{
    std::vector<completion_t> output;
    va_list va;
    size_t i=0;
    int res=1;
    wchar_t *arg;

    if (expand_string(in, output, flags))
    {

    }

#if 0
    for (size_t idx=0; idx < output.size(); idx++)
    {
        printf("%ls\n", output.at(idx).completion.c_str());
    }
#endif

    va_start(va, flags);

    while ((arg=va_arg(va, wchar_t *))!= 0)
    {
        if (output.size() == i)
        {
            res=0;
            break;
        }

        if (output.at(i).completion != arg)
        {
            res=0;
            break;
        }

        i++;
    }
    va_end(va);

    return res;

}

/**
   Test globbing and other parameter expansion
*/
static void test_expand()
{
    say(L"Testing parameter expansion");

    if (!expand_test(L"foo", 0, L"foo", 0))
    {
        err(L"Strings do not expand to themselves");
    }

    if (!expand_test(L"a{b,c,d}e", 0, L"abe", L"ace", L"ade", 0))
    {
        err(L"Bracket expansion is broken");
    }

    if (!expand_test(L"a*", EXPAND_SKIP_WILDCARDS, L"a*", 0))
    {
        err(L"Cannot skip wildcard expansion");
    }

    if (system("mkdir -p /tmp/fish_expand_test/")) err(L"mkdir failed");
    if (system("touch /tmp/fish_expand_test/.foo")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/bar")) err(L"touch failed");

    // This is checking that .* does NOT match . and .. (https://github.com/fish-shell/fish-shell/issues/270). But it does have to match literal components (e.g. "./*" has to match the same as "*"
    if (! expand_test(L"/tmp/fish_expand_test/.*", 0, L"/tmp/fish_expand_test/.foo", 0))
    {
        err(L"Expansion not correctly handling dotfiles");
    }
    if (! expand_test(L"/tmp/fish_expand_test/./.*", 0, L"/tmp/fish_expand_test/./.foo", 0))
    {
        err(L"Expansion not correctly handling literal path components in dotfiles");
    }

    system("rm -Rf /tmp/fish_expand_test");
}

static void test_fuzzy_match(void)
{
    say(L"Testing fuzzy string matching");

    if (string_fuzzy_match_string(L"", L"").type != fuzzy_match_exact) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"alpha", L"alpha").type != fuzzy_match_exact) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"alp", L"alpha").type != fuzzy_match_prefix) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"ALPHA!", L"alPhA!").type != fuzzy_match_case_insensitive) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"alPh", L"ALPHA!").type != fuzzy_match_prefix_case_insensitive) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"LPH", L"ALPHA!").type != fuzzy_match_substring) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"AA", L"ALPHA!").type != fuzzy_match_subsequence_insertions_only) err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"BB", L"ALPHA!").type != fuzzy_match_none) err(L"test_fuzzy_match failed on line %ld", __LINE__);
}

static void test_abbreviations(void)
{
    say(L"Testing abbreviations");

    const wchar_t *abbreviations =
        L"gc=git checkout" ARRAY_SEP_STR
        L"foo=" ARRAY_SEP_STR
        L"gc=something else" ARRAY_SEP_STR
        L"=" ARRAY_SEP_STR
        L"=foo" ARRAY_SEP_STR
        L"foo" ARRAY_SEP_STR
        L"foo=bar";

    env_push(true);

    int ret = env_set(USER_ABBREVIATIONS_VARIABLE_NAME, abbreviations, ENV_LOCAL);
    if (ret != 0) err(L"Unable to set abbreviation variable");

    wcstring result;
    if (expand_abbreviation(L"", &result)) err(L"Unexpected success with empty abbreviation");
    if (expand_abbreviation(L"nothing", &result)) err(L"Unexpected success with missing abbreviation");

    if (! expand_abbreviation(L"gc", &result)) err(L"Unexpected failure with gc abbreviation");
    if (result != L"git checkout") err(L"Wrong abbreviation result for gc");
    result.clear();

    if (! expand_abbreviation(L"foo", &result)) err(L"Unexpected failure with foo abbreviation");
    if (result != L"bar") err(L"Wrong abbreviation result for foo");

    bool expanded;
    expanded = reader_expand_abbreviation_in_command(L"just a command", 3, &result);
    if (expanded) err(L"Command wrongly expanded on line %ld", (long)__LINE__);
    expanded = reader_expand_abbreviation_in_command(L"gc somebranch", 0, &result);
    if (! expanded) err(L"Command not expanded on line %ld", (long)__LINE__);

    expanded = reader_expand_abbreviation_in_command(L"gc somebranch", wcslen(L"gc"), &result);
    if (! expanded) err(L"gc not expanded");
    if (result != L"git checkout somebranch") err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    expanded = reader_expand_abbreviation_in_command(L"echo hi ; gc somebranch", wcslen(L"echo hi ; g"), &result);
    if (! expanded) err(L"gc not expanded on line %ld", (long)__LINE__);
    if (result != L"echo hi ; git checkout somebranch") err(L"gc incorrectly expanded on line %ld", (long)__LINE__);

    expanded = reader_expand_abbreviation_in_command(L"echo (echo (echo (echo (gc ", wcslen(L"echo (echo (echo (echo (gc"), &result);
    if (! expanded) err(L"gc not expanded on line %ld", (long)__LINE__);
    if (result != L"echo (echo (echo (echo (git checkout ") err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    /* if commands should be expanded */
    expanded = reader_expand_abbreviation_in_command(L"if gc", wcslen(L"if gc"), &result);
    if (! expanded) err(L"gc not expanded on line %ld", (long)__LINE__);
    if (result != L"if git checkout") err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    /* others should not be */
    expanded = reader_expand_abbreviation_in_command(L"of gc", wcslen(L"of gc"), &result);
    if (expanded) err(L"gc incorrectly expanded on line %ld", (long)__LINE__);

    env_pop();
}

/** Test path functions */
static void test_path()
{
    say(L"Testing path functions");

    wcstring path = L"//foo//////bar/";
    path_make_canonical(path);
    if (path != L"/foo/bar")
    {
        err(L"Bug in canonical PATH code");
    }

    path = L"/";
    path_make_canonical(path);
    if (path != L"/")
    {
        err(L"Bug in canonical PATH code");
    }
    
    if (paths_are_equivalent(L"/foo/bar/baz", L"foo/bar/baz")) err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
    if (! paths_are_equivalent(L"///foo///bar/baz", L"/foo/bar////baz//")) err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
    if (! paths_are_equivalent(L"/foo/bar/baz", L"/foo/bar/baz")) err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
    if (! paths_are_equivalent(L"/", L"/")) err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
}

enum word_motion_t
{
    word_motion_left,
    word_motion_right
};
static void test_1_word_motion(word_motion_t motion, move_word_style_t style, const wcstring &test)
{
    wcstring command;
    std::set<size_t> stops;

    // Carets represent stops and should be cut out of the command
    for (size_t i=0; i < test.size(); i++)
    {
        wchar_t wc = test.at(i);
        if (wc == L'^')
        {
            stops.insert(command.size());
        }
        else
        {
            command.push_back(wc);
        }
    }

    size_t idx, end;
    if (motion == word_motion_left)
    {
        idx = command.size();
        end = 0;
    }
    else
    {
        idx = 0;
        end = command.size();
    }

    move_word_state_machine_t sm(style);
    while (idx != end)
    {
        size_t char_idx = (motion == word_motion_left ? idx - 1 : idx);
        wchar_t wc = command.at(char_idx);
        bool will_stop = ! sm.consume_char(wc);
        //printf("idx %lu, looking at %lu (%c): %d\n", idx, char_idx, (char)wc, will_stop);
        bool expected_stop = (stops.count(idx) > 0);
        if (will_stop != expected_stop)
        {
            wcstring tmp = command;
            tmp.insert(idx, L"^");
            const char *dir = (motion == word_motion_left ? "left" : "right");
            if (will_stop)
            {
                err(L"Word motion: moving %s, unexpected stop at idx %lu: '%ls'", dir, idx, tmp.c_str());
            }
            else if (! will_stop && expected_stop)
            {
                err(L"Word motion: moving %s, should have stopped at idx %lu: '%ls'", dir, idx, tmp.c_str());
            }
        }
        // We don't expect to stop here next time
        if (expected_stop)
        {
            stops.erase(idx);
        }
        if (will_stop)
        {
            sm.reset();
        }
        else
        {
            idx += (motion == word_motion_left ? -1 : 1);
        }
    }
}

/** Test word motion (forward-word, etc.). Carets represent cursor stops. */
static void test_word_motion()
{
    say(L"Testing word motion");
    test_1_word_motion(word_motion_left, move_word_style_punctuation, L"^echo ^hello_^world.^txt");
    test_1_word_motion(word_motion_right, move_word_style_punctuation, L"echo^ hello^_world^.txt^");

    test_1_word_motion(word_motion_left, move_word_style_punctuation, L"echo ^foo_^foo_^foo/^/^/^/^/^    ");
    test_1_word_motion(word_motion_right, move_word_style_punctuation, L"echo^ foo^_foo^_foo^/^/^/^/^/    ^");

    test_1_word_motion(word_motion_left, move_word_style_path_components, L"^/^foo/^bar/^baz/");
    test_1_word_motion(word_motion_left, move_word_style_path_components, L"^echo ^--foo ^--bar");
    test_1_word_motion(word_motion_left, move_word_style_path_components, L"^echo ^hi ^> /^dev/^null");

    test_1_word_motion(word_motion_left, move_word_style_path_components, L"^echo /^foo/^bar{^aaa,^bbb,^ccc}^bak/");
}

/** Test is_potential_path */
static void test_is_potential_path()
{
    say(L"Testing is_potential_path");
    if (system("rm -Rf /tmp/is_potential_path_test/"))
    {
        err(L"Failed to remove /tmp/is_potential_path_test/");
    }

    /* Directories */
    if (system("mkdir -p /tmp/is_potential_path_test/alpha/")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/is_potential_path_test/beta/")) err(L"mkdir failed");

    /* Files */
    if (system("touch /tmp/is_potential_path_test/aardvark")) err(L"touch failed");
    if (system("touch /tmp/is_potential_path_test/gamma")) err(L"touch failed");

    const wcstring wd = L"/tmp/is_potential_path_test/";
    const wcstring_list_t wds(1, wd);

    wcstring tmp;
    assert(is_potential_path(L"al", wds, PATH_REQUIRE_DIR, &tmp) && tmp == L"alpha/");
    assert(is_potential_path(L"alpha/", wds, PATH_REQUIRE_DIR, &tmp) && tmp == L"alpha/");
    assert(is_potential_path(L"aard", wds, 0, &tmp) && tmp == L"aardvark");

    assert(! is_potential_path(L"balpha/", wds, PATH_REQUIRE_DIR, &tmp));
    assert(! is_potential_path(L"aard", wds, PATH_REQUIRE_DIR, &tmp));
    assert(! is_potential_path(L"aarde", wds, PATH_REQUIRE_DIR, &tmp));
    assert(! is_potential_path(L"aarde", wds, 0, &tmp));

    assert(is_potential_path(L"/tmp/is_potential_path_test/aardvark", wds, 0, &tmp) && tmp == L"/tmp/is_potential_path_test/aardvark");
    assert(is_potential_path(L"/tmp/is_potential_path_test/al", wds, PATH_REQUIRE_DIR, &tmp) && tmp == L"/tmp/is_potential_path_test/alpha/");
    assert(is_potential_path(L"/tmp/is_potential_path_test/aardv", wds, 0, &tmp) && tmp == L"/tmp/is_potential_path_test/aardvark");

    assert(! is_potential_path(L"/tmp/is_potential_path_test/aardvark", wds, PATH_REQUIRE_DIR, &tmp));
    assert(! is_potential_path(L"/tmp/is_potential_path_test/al/", wds, 0, &tmp));
    assert(! is_potential_path(L"/tmp/is_potential_path_test/ar", wds, 0, &tmp));

    assert(is_potential_path(L"/usr", wds, PATH_REQUIRE_DIR, &tmp) && tmp == L"/usr/");

}

/** Test the 'test' builtin */
int builtin_test(parser_t &parser, wchar_t **argv);
static bool run_one_test_test(int expected, wcstring_list_t &lst, bool bracket)
{
    parser_t parser(PARSER_TYPE_GENERAL, true);
    size_t i, count = lst.size();
    wchar_t **argv = new wchar_t *[count+3];
    argv[0] = (wchar_t *)(bracket ? L"[" : L"test");
    for (i=0; i < count; i++)
    {
        argv[i+1] = (wchar_t *)lst.at(i).c_str();
    }
    if (bracket)
    {
        argv[i+1] = (wchar_t *)L"]";
        i++;
    }
    argv[i+1] = NULL;
    int result = builtin_test(parser, argv);
    delete[] argv;
    return expected == result;
}

static bool run_test_test(int expected, const wcstring &str)
{
    using namespace std;
    wcstring_list_t lst;

    wistringstream iss(str);
    copy(istream_iterator<wcstring, wchar_t, std::char_traits<wchar_t> >(iss),
         istream_iterator<wstring, wchar_t, std::char_traits<wchar_t> >(),
         back_inserter<vector<wcstring> >(lst));

    bool bracket = run_one_test_test(expected, lst, true);
    bool nonbracket = run_one_test_test(expected, lst, false);
    assert(bracket == nonbracket);
    return nonbracket;
}

static void test_test_brackets()
{
    // Ensure [ knows it needs a ]
    parser_t parser(PARSER_TYPE_GENERAL, true);

    const wchar_t *argv1[] = {L"[", L"foo", NULL};
    assert(builtin_test(parser, (wchar_t **)argv1) != 0);

    const wchar_t *argv2[] = {L"[", L"foo", L"]", NULL};
    assert(builtin_test(parser, (wchar_t **)argv2) == 0);

    const wchar_t *argv3[] = {L"[", L"foo", L"]", L"bar", NULL};
    assert(builtin_test(parser, (wchar_t **)argv3) != 0);

}

static void test_test()
{
    say(L"Testing test builtin");
    test_test_brackets();

    assert(run_test_test(0, L"5 -ne 6"));
    assert(run_test_test(0, L"5 -eq 5"));
    assert(run_test_test(0, L"0 -eq 0"));
    assert(run_test_test(0, L"-1 -eq -1"));
    assert(run_test_test(0, L"1 -ne -1"));
    assert(run_test_test(1, L"-1 -ne -1"));
    assert(run_test_test(0, L"abc != def"));
    assert(run_test_test(1, L"abc = def"));
    assert(run_test_test(0, L"5 -le 10"));
    assert(run_test_test(0, L"10 -le 10"));
    assert(run_test_test(1, L"20 -le 10"));
    assert(run_test_test(0, L"-1 -le 0"));
    assert(run_test_test(1, L"0 -le -1"));
    assert(run_test_test(0, L"15 -ge 10"));
    assert(run_test_test(0, L"15 -ge 10"));
    assert(run_test_test(1, L"! 15 -ge 10"));
    assert(run_test_test(0, L"! ! 15 -ge 10"));

    assert(run_test_test(0, L"0 -ne 1 -a 0 -eq 0"));
    assert(run_test_test(0, L"0 -ne 1 -a -n 5"));
    assert(run_test_test(0, L"-n 5 -a 10 -gt 5"));
    assert(run_test_test(0, L"-n 3 -a -n 5"));

    /* test precedence:
            '0 == 0 || 0 == 1 && 0 == 2'
        should be evaluated as:
            '0 == 0 || (0 == 1 && 0 == 2)'
        and therefore true. If it were
            '(0 == 0 || 0 == 1) && 0 == 2'
        it would be false. */
    assert(run_test_test(0, L"0 = 0 -o 0 = 1 -a 0 = 2"));
    assert(run_test_test(0, L"-n 5 -o 0 = 1 -a 0 = 2"));
    assert(run_test_test(1, L"( 0 = 0 -o  0 = 1 ) -a 0 = 2"));
    assert(run_test_test(0, L"0 = 0 -o ( 0 = 1 -a 0 = 2 )"));

    /* A few lame tests for permissions; these need to be a lot more complete. */
    assert(run_test_test(0, L"-e /bin/ls"));
    assert(run_test_test(1, L"-e /bin/ls_not_a_path"));
    assert(run_test_test(0, L"-x /bin/ls"));
    assert(run_test_test(1, L"-x /bin/ls_not_a_path"));
    assert(run_test_test(0, L"-d /bin/"));
    assert(run_test_test(1, L"-d /bin/ls"));

    /* This failed at one point */
    assert(run_test_test(1, L"-d /bin -a 5 -eq 3"));
    assert(run_test_test(0, L"-d /bin -o 5 -eq 3"));
    assert(run_test_test(0, L"-d /bin -a ! 5 -eq 3"));

    /* We didn't properly handle multiple "just strings" either */
    assert(run_test_test(0, L"foo"));
    assert(run_test_test(0, L"foo -a bar"));

    /* These should be errors */
    assert(run_test_test(1, L"foo bar"));
    assert(run_test_test(1, L"foo bar baz"));

    /* This crashed */
    assert(run_test_test(1, L"1 = 1 -a = 1"));

    /* Make sure we can treat -S as a parameter instead of an operator. https://github.com/fish-shell/fish-shell/issues/601 */
    assert(run_test_test(0, L"-S = -S"));
    assert(run_test_test(1, L"! ! ! A"));
}

/** Testing colors */
static void test_colors()
{
    say(L"Testing colors");
    assert(rgb_color_t(L"#FF00A0").is_rgb());
    assert(rgb_color_t(L"FF00A0").is_rgb());
    assert(rgb_color_t(L"#F30").is_rgb());
    assert(rgb_color_t(L"F30").is_rgb());
    assert(rgb_color_t(L"f30").is_rgb());
    assert(rgb_color_t(L"#FF30a5").is_rgb());
    assert(rgb_color_t(L"3f30").is_none());
    assert(rgb_color_t(L"##f30").is_none());
    assert(rgb_color_t(L"magenta").is_named());
    assert(rgb_color_t(L"MaGeNTa").is_named());
    assert(rgb_color_t(L"mooganta").is_none());
}

static void test_complete(void)
{
    say(L"Testing complete");
    const wchar_t *name_strs[] = {L"Foo1", L"Foo2", L"Foo3", L"Bar1", L"Bar2", L"Bar3"};
    size_t count = sizeof name_strs / sizeof *name_strs;
    const wcstring_list_t names(name_strs, name_strs + count);

    complete_set_variable_names(&names);

    std::vector<completion_t> completions;
    complete(L"$F", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 3);
    assert(completions.at(0).completion == L"oo1");
    assert(completions.at(1).completion == L"oo2");
    assert(completions.at(2).completion == L"oo3");

    completions.clear();
    complete(L"$1", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.empty());

    completions.clear();
    complete(L"$1", completions, COMPLETION_REQUEST_DEFAULT | COMPLETION_REQUEST_FUZZY_MATCH);
    assert(completions.size() == 2);
    assert(completions.at(0).completion == L"$Foo1");
    assert(completions.at(1).completion == L"$Bar1");


    complete_set_variable_names(NULL);
}

static void test_1_completion(wcstring line, const wcstring &completion, complete_flags_t flags, bool append_only, wcstring expected, long source_line)
{
    // str is given with a caret, which we use to represent the cursor position
    // find it
    const size_t in_cursor_pos = line.find(L'^');
    assert(in_cursor_pos != wcstring::npos);
    line.erase(in_cursor_pos, 1);

    const size_t out_cursor_pos = expected.find(L'^');
    assert(out_cursor_pos != wcstring::npos);
    expected.erase(out_cursor_pos, 1);

    size_t cursor_pos = in_cursor_pos;
    wcstring result = completion_apply_to_command_line(completion, flags, line, &cursor_pos, append_only);
    if (result != expected)
    {
        fprintf(stderr, "line %ld: %ls + %ls -> [%ls], expected [%ls]\n", source_line, line.c_str(), completion.c_str(), result.c_str(), expected.c_str());
    }
    assert(result == expected);
    assert(cursor_pos == out_cursor_pos);
}

static void test_completion_insertions()
{
#define TEST_1_COMPLETION(a, b, c, d, e) test_1_completion(a, b, c, d, e, __LINE__)
    say(L"Testing completion insertions");
    TEST_1_COMPLETION(L"foo^", L"bar", 0, false, L"foobar ^");
    TEST_1_COMPLETION(L"foo^ baz", L"bar", 0, false, L"foobar ^ baz"); //we really do want to insert two spaces here - otherwise it's hidden by the cursor
    TEST_1_COMPLETION(L"'foo^", L"bar", 0, false, L"'foobar' ^");
    TEST_1_COMPLETION(L"'foo'^", L"bar", 0, false, L"'foobar' ^");
    TEST_1_COMPLETION(L"'foo\\'^", L"bar", 0, false, L"'foo\\'bar' ^");
    TEST_1_COMPLETION(L"foo\\'^", L"bar", 0, false, L"foo\\'bar ^");

    // Test append only
    TEST_1_COMPLETION(L"foo^", L"bar", 0, true, L"foobar ^");
    TEST_1_COMPLETION(L"foo^ baz", L"bar", 0, true, L"foobar ^ baz");
    TEST_1_COMPLETION(L"'foo^", L"bar", 0, true, L"'foobar' ^");
    TEST_1_COMPLETION(L"'foo'^", L"bar", 0, true, L"'foo'bar ^");
    TEST_1_COMPLETION(L"'foo\\'^", L"bar", 0, true, L"'foo\\'bar' ^");
    TEST_1_COMPLETION(L"foo\\'^", L"bar", 0, true, L"foo\\'bar ^");

    TEST_1_COMPLETION(L"foo^", L"bar", COMPLETE_NO_SPACE, false, L"foobar^");
    TEST_1_COMPLETION(L"'foo^", L"bar", COMPLETE_NO_SPACE, false, L"'foobar^");
    TEST_1_COMPLETION(L"'foo'^", L"bar", COMPLETE_NO_SPACE, false, L"'foobar'^");
    TEST_1_COMPLETION(L"'foo\\'^", L"bar", COMPLETE_NO_SPACE, false, L"'foo\\'bar^");
    TEST_1_COMPLETION(L"foo\\'^", L"bar", COMPLETE_NO_SPACE, false, L"foo\\'bar^");

    TEST_1_COMPLETION(L"foo^", L"bar", COMPLETE_REPLACES_TOKEN, false, L"bar ^");
    TEST_1_COMPLETION(L"'foo^", L"bar", COMPLETE_REPLACES_TOKEN, false, L"bar ^");
}

static void perform_one_autosuggestion_test(const wcstring &command, const wcstring &wd, const wcstring &expected, long line)
{
    wcstring suggestion;
    bool success = autosuggest_suggest_special(command, wd, suggestion);
    if (! success)
    {
        printf("line %ld: autosuggest_suggest_special() failed for command %ls\n", line, command.c_str());
        assert(success);
    }
    if (suggestion != expected)
    {
        printf("line %ld: autosuggest_suggest_special() returned the wrong expected string for command %ls\n", line, command.c_str());
        printf("  actual: %ls\n", suggestion.c_str());
        printf("expected: %ls\n", expected.c_str());
        assert(suggestion == expected);
    }
}

/* Testing test_autosuggest_suggest_special, in particular for properly handling quotes and backslashes */
static void test_autosuggest_suggest_special()
{
    if (system("mkdir -p '/tmp/autosuggest_test/0foobar'")) err(L"mkdir failed");
    if (system("mkdir -p '/tmp/autosuggest_test/1foo bar'")) err(L"mkdir failed");
    if (system("mkdir -p '/tmp/autosuggest_test/2foo  bar'")) err(L"mkdir failed");
    if (system("mkdir -p '/tmp/autosuggest_test/3foo\\bar'")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/autosuggest_test/4foo\\'bar")) err(L"mkdir failed"); //a path with a single quote
    if (system("mkdir -p /tmp/autosuggest_test/5foo\\\"bar")) err(L"mkdir failed"); //a path with a double quote
    if (system("mkdir -p ~/test_autosuggest_suggest_special/")) err(L"mkdir failed"); //make sure tilde is handled

    const wcstring wd = L"/tmp/autosuggest_test/";
    perform_one_autosuggestion_test(L"cd /tmp/autosuggest_test/0", wd, L"cd /tmp/autosuggest_test/0foobar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"/tmp/autosuggest_test/0", wd, L"cd \"/tmp/autosuggest_test/0foobar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '/tmp/autosuggest_test/0", wd, L"cd '/tmp/autosuggest_test/0foobar/'", __LINE__);
    perform_one_autosuggestion_test(L"cd 0", wd, L"cd 0foobar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"0", wd, L"cd \"0foobar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '0", wd, L"cd '0foobar/'", __LINE__);

    perform_one_autosuggestion_test(L"cd /tmp/autosuggest_test/1", wd, L"cd /tmp/autosuggest_test/1foo\\ bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"/tmp/autosuggest_test/1", wd, L"cd \"/tmp/autosuggest_test/1foo bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '/tmp/autosuggest_test/1", wd, L"cd '/tmp/autosuggest_test/1foo bar/'", __LINE__);
    perform_one_autosuggestion_test(L"cd 1", wd, L"cd 1foo\\ bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"1", wd, L"cd \"1foo bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '1", wd, L"cd '1foo bar/'", __LINE__);

    perform_one_autosuggestion_test(L"cd /tmp/autosuggest_test/2", wd, L"cd /tmp/autosuggest_test/2foo\\ \\ bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"/tmp/autosuggest_test/2", wd, L"cd \"/tmp/autosuggest_test/2foo  bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '/tmp/autosuggest_test/2", wd, L"cd '/tmp/autosuggest_test/2foo  bar/'", __LINE__);
    perform_one_autosuggestion_test(L"cd 2", wd, L"cd 2foo\\ \\ bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"2", wd, L"cd \"2foo  bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '2", wd, L"cd '2foo  bar/'", __LINE__);

    perform_one_autosuggestion_test(L"cd /tmp/autosuggest_test/3", wd, L"cd /tmp/autosuggest_test/3foo\\\\bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"/tmp/autosuggest_test/3", wd, L"cd \"/tmp/autosuggest_test/3foo\\bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '/tmp/autosuggest_test/3", wd, L"cd '/tmp/autosuggest_test/3foo\\bar/'", __LINE__);
    perform_one_autosuggestion_test(L"cd 3", wd, L"cd 3foo\\\\bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"3", wd, L"cd \"3foo\\bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '3", wd, L"cd '3foo\\bar/'", __LINE__);

    perform_one_autosuggestion_test(L"cd /tmp/autosuggest_test/4", wd, L"cd /tmp/autosuggest_test/4foo\\'bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"/tmp/autosuggest_test/4", wd, L"cd \"/tmp/autosuggest_test/4foo'bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '/tmp/autosuggest_test/4", wd, L"cd '/tmp/autosuggest_test/4foo\\'bar/'", __LINE__);
    perform_one_autosuggestion_test(L"cd 4", wd, L"cd 4foo\\'bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"4", wd, L"cd \"4foo'bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '4", wd, L"cd '4foo\\'bar/'", __LINE__);

    perform_one_autosuggestion_test(L"cd /tmp/autosuggest_test/5", wd, L"cd /tmp/autosuggest_test/5foo\\\"bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"/tmp/autosuggest_test/5", wd, L"cd \"/tmp/autosuggest_test/5foo\\\"bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '/tmp/autosuggest_test/5", wd, L"cd '/tmp/autosuggest_test/5foo\"bar/'", __LINE__);
    perform_one_autosuggestion_test(L"cd 5", wd, L"cd 5foo\\\"bar/", __LINE__);
    perform_one_autosuggestion_test(L"cd \"5", wd, L"cd \"5foo\\\"bar/\"", __LINE__);
    perform_one_autosuggestion_test(L"cd '5", wd, L"cd '5foo\"bar/'", __LINE__);

    perform_one_autosuggestion_test(L"cd ~/test_autosuggest_suggest_specia", wd, L"cd ~/test_autosuggest_suggest_special/", __LINE__);

    // A single quote should defeat tilde expansion
    perform_one_autosuggestion_test(L"cd '~/test_autosuggest_suggest_specia'", wd, L"", __LINE__);

    system("rm -Rf '/tmp/autosuggest_test/'");
    system("rm -Rf ~/test_autosuggest_suggest_special/");
}

static void test_autosuggestion_combining()
{
    say(L"Testing autosuggestion combining");
    assert(combine_command_and_autosuggestion(L"alpha", L"alphabeta") == L"alphabeta");

    // when the last token contains no capital letters, we use the case of the autosuggestion
    assert(combine_command_and_autosuggestion(L"alpha", L"ALPHABETA") == L"ALPHABETA");

    // when the last token contains capital letters, we use its case
    assert(combine_command_and_autosuggestion(L"alPha", L"alphabeTa") == L"alPhabeTa");

    // if autosuggestion is not longer than input, use the input's case
    assert(combine_command_and_autosuggestion(L"alpha", L"ALPHAA") == L"ALPHAA");
    assert(combine_command_and_autosuggestion(L"alpha", L"ALPHA") == L"alpha");
}


/**
   Test speed of completion calculations
*/
void perf_complete()
{
    wchar_t c;
    std::vector<completion_t> out;
    long long t1, t2;
    int matches=0;
    double t;
    wchar_t str[3]=
    {
        0, 0, 0
    }
    ;
    int i;


    say(L"Testing completion performance");

    reader_push(L"");
    say(L"Here we go");

    t1 = get_time();


    for (c=L'a'; c<=L'z'; c++)
    {
        str[0]=c;
        reader_set_buffer(str, 0);

        complete(str, out, COMPLETION_REQUEST_DEFAULT, NULL);

        matches += out.size();
        out.clear();
    }
    t2=get_time();

    t = (double)(t2-t1)/(1000000*26);

    say(L"One letter command completion took %f seconds per completion, %f microseconds/match", t, (double)(t2-t1)/matches);

    matches=0;
    t1 = get_time();
    for (i=0; i<LAPS; i++)
    {
        str[0]='a'+(rand()%26);
        str[1]='a'+(rand()%26);

        reader_set_buffer(str, 0);

        complete(str, out, COMPLETION_REQUEST_DEFAULT, NULL);

        matches += out.size();
        out.clear();
    }
    t2=get_time();

    t = (double)(t2-t1)/(1000000*LAPS);

    say(L"Two letter command completion took %f seconds per completion, %f microseconds/match", t, (double)(t2-t1)/matches);

    reader_pop();

}

static void test_history_matches(history_search_t &search, size_t matches)
{
    size_t i;
    for (i=0; i < matches; i++)
    {
        assert(search.go_backwards());
        wcstring item = search.current_string();
    }
    assert(! search.go_backwards());

    for (i=1; i < matches; i++)
    {
        assert(search.go_forwards());
    }
    assert(! search.go_forwards());
}

static bool history_contains(history_t *history, const wcstring &txt)
{
    bool result = false;
    size_t i;
    for (i=1; ; i++)
    {
        history_item_t item = history->item_at_index(i);
        if (item.empty())
            break;

        if (item.str() == txt)
        {
            result = true;
            break;
        }
    }
    return result;
}

class history_tests_t
{
public:
    static void test_history(void);
    static void test_history_merge(void);
    static void test_history_formats(void);
    static void test_history_speed(void);

    static void test_history_races(void);
    static void test_history_races_pound_on_history();
};

static wcstring random_string(void)
{
    wcstring result;
    size_t max = 1 + rand() % 32;
    while (max--)
    {
        wchar_t c = 1 + rand()%ESCAPE_TEST_CHAR;
        result.push_back(c);
    }
    return result;
}

void history_tests_t::test_history(void)
{
    say(L"Testing history");

    history_t &history = history_t::history_with_name(L"test_history");
    history.clear();
    history.add(L"Gamma");
    history.add(L"Beta");
    history.add(L"Alpha");

    /* All three items match "a" */
    history_search_t search1(history, L"a");
    test_history_matches(search1, 3);
    assert(search1.current_string() == L"Alpha");

    /* One item matches "et" */
    history_search_t search2(history, L"et");
    test_history_matches(search2, 1);
    assert(search2.current_string() == L"Beta");

    /* Test item removal */
    history.remove(L"Alpha");
    history_search_t search3(history, L"Alpha");
    test_history_matches(search3, 0);

    /* Test history escaping and unescaping, yaml, etc. */
    std::vector<history_item_t> before, after;
    history.clear();
    size_t i, max = 100;
    for (i=1; i <= max; i++)
    {

        /* Generate a value */
        wcstring value = wcstring(L"test item ") + to_string(i);

        /* Maybe add some backslashes */
        if (i % 3 == 0)
            value.append(L"(slashies \\\\\\ slashies)");

        /* Generate some paths */
        path_list_t paths;
        size_t count = rand() % 6;
        while (count--)
        {
            paths.push_back(random_string());
        }

        /* Record this item */
        history_item_t item(value, time(NULL), paths);
        before.push_back(item);
        history.add(item);
    }
    history.save();

    /* Read items back in reverse order and ensure they're the same */
    for (i=100; i >= 1; i--)
    {
        history_item_t item = history.item_at_index(i);
        assert(! item.empty());
        after.push_back(item);
    }
    assert(before.size() == after.size());
    for (size_t i=0; i < before.size(); i++)
    {
        const history_item_t &bef = before.at(i), &aft = after.at(i);
        assert(bef.contents == aft.contents);
        assert(bef.creation_timestamp == aft.creation_timestamp);
        assert(bef.required_paths == aft.required_paths);
    }

    /* Clean up after our tests */
    history.clear();
}

// wait until the next second
static void time_barrier(void)
{
    time_t start = time(NULL);
    do
    {
        usleep(1000);
    }
    while (time(NULL) == start);
}

static wcstring_list_t generate_history_lines(int pid)
{
    wcstring_list_t result;
    long max = 256;
    result.reserve(max);
    for (long i=0; i < max; i++)
    {
        result.push_back(format_string(L"%ld %ld", (long)pid, i));
    }
    return result;
}

void history_tests_t::test_history_races_pound_on_history()
{
    /* Called in child process to modify history */
    history_t *hist = new history_t(L"race_test");
    hist->chaos_mode = true;
    const wcstring_list_t lines = generate_history_lines(getpid());
    for (size_t idx = 0; idx < lines.size(); idx++)
    {
        const wcstring &line = lines.at(idx);
        hist->add(line);
        hist->save();
    }
    delete hist;
}

void history_tests_t::test_history_races(void)
{
    say(L"Testing history race conditions");

    // Ensure history is clear
    history_t *hist = new history_t(L"race_test");
    hist->clear();
    delete hist;

    // Test concurrent history writing
#define RACE_COUNT 10
    pid_t children[RACE_COUNT];

    for (size_t i=0; i < RACE_COUNT; i++)
    {
        pid_t pid = fork();
        if (! pid)
        {
            // Child process
            setup_fork_guards();
            test_history_races_pound_on_history();
            exit_without_destructors(0);
        }
        else
        {
            // Parent process
            children[i] = pid;
        }
    }

    // Wait for all children
    for (size_t i=0; i < RACE_COUNT; i++)
    {
        int stat;
        waitpid(children[i], &stat, WUNTRACED);
    }

    // Compute the expected lines
    wcstring_list_t lines[RACE_COUNT];
    for (size_t i=0; i < RACE_COUNT; i++)
    {
        lines[i] = generate_history_lines(children[i]);
    }

    // Count total lines
    size_t line_count = 0;
    for (size_t i=0; i < RACE_COUNT; i++)
    {
        line_count += lines[i].size();
    }

    // Ensure we consider the lines that have been outputted as part of our history
    time_barrier();

    /* Ensure that we got sane, sorted results */
    hist = new history_t(L"race_test");
    hist->chaos_mode = true;
    size_t hist_idx;
    for (hist_idx = 1; ; hist_idx ++)
    {
        history_item_t item = hist->item_at_index(hist_idx);
        if (item.empty())
            break;

        // The item must be present in one of our 'lines' arrays
        // If it is present, then every item after it is assumed to be missed
        size_t i;
        for (i=0; i < RACE_COUNT; i++)
        {
            wcstring_list_t::iterator where = std::find(lines[i].begin(), lines[i].end(), item.str());
            if (where != lines[i].end())
            {
                // Delete everything from the found location onwards
                lines[i].resize(where - lines[i].begin());

                // Break because we found it
                break;
            }
        }
        if (i >= RACE_COUNT)
        {
            err(L"Line '%ls' found in history not found in some array", item.str().c_str());
        }
    }
    // every write should add at least one item
    assert(hist_idx >= RACE_COUNT);

    //hist->clear();
    delete hist;
}

void history_tests_t::test_history_merge(void)
{
    // In a single fish process, only one history is allowed to exist with the given name
    // But it's common to have multiple history instances with the same name active in different processes,
    // e.g. when you have multiple shells open.
    // We try to get that right and merge all their history together. Test that case.
    say(L"Testing history merge");
    const size_t count = 3;
    const wcstring name = L"merge_test";
    history_t *hists[count] = {new history_t(name), new history_t(name), new history_t(name)};
    wcstring texts[count] = {L"History 1", L"History 2", L"History 3"};

    /* Make sure history is clear */
    for (size_t i=0; i < count; i++)
    {
        hists[i]->clear();
    }

    /* Make sure we don't add an item in the same second as we created the history */
    time_barrier();

    /* Add a different item to each */
    for (size_t i=0; i < count; i++)
    {
        hists[i]->add(texts[i]);
    }

    /* Save them */
    for (size_t i=0; i < count; i++)
    {
        hists[i]->save();
    }

    /* Make sure each history contains what it ought to, but they have not leaked into each other */
    for (size_t i = 0; i < count; i++)
    {
        for (size_t j=0; j < count; j++)
        {
            bool does_contain = history_contains(hists[i], texts[j]);
            bool should_contain = (i == j);
            assert(should_contain == does_contain);
        }
    }

    /* Make a new history. It should contain everything. The time_barrier() is so that the timestamp is newer, since we only pick up items whose timestamp is before the birth stamp. */
    time_barrier();
    history_t *everything = new history_t(name);
    for (size_t i=0; i < count; i++)
    {
        assert(history_contains(everything, texts[i]));
    }

    /* Clean up */
    for (size_t i=0; i < 3; i++)
    {
        delete hists[i];
    }
    everything->clear();
    delete everything; //not as scary as it looks
}

static bool install_sample_history(const wchar_t *name)
{
    char command[512];
    snprintf(command, sizeof command, "cp tests/%ls ~/.config/fish/%ls_history", name, name);
    if (system(command))
    {
        err(L"Failed to copy sample history");
        return false;
    }
    return true;
}

/* Indicates whether the history is equal to the given null-terminated array of strings. */
static bool history_equals(history_t &hist, const wchar_t * const *strings)
{
    /* Count our expected items */
    size_t expected_count = 0;
    while (strings[expected_count])
    {
        expected_count++;
    }

    /* Ensure the contents are the same */
    size_t history_idx = 1;
    size_t array_idx = 0;
    for (;;)
    {
        const wchar_t *expected = strings[array_idx];
        history_item_t item = hist.item_at_index(history_idx);
        if (expected == NULL)
        {
            if (! item.empty())
            {
                err(L"Expected empty item at history index %lu", history_idx);
            }
            break;
        }
        else
        {
            if (item.str() != expected)
            {
                err(L"Expected '%ls', found '%ls' at index %lu", expected, item.str().c_str(), history_idx);
            }
        }
        history_idx++;
        array_idx++;
    }

    return true;
}

void history_tests_t::test_history_formats(void)
{
    const wchar_t *name;

    // Test inferring and reading legacy and bash history formats
    name = L"history_sample_fish_1_x";
    say(L"Testing %ls", name);
    if (! install_sample_history(name))
    {
        err(L"Couldn't open file tests/%ls", name);
    }
    else
    {
        /* Note: This is backwards from what appears in the file */
        const wchar_t * const expected[] =
        {
            L"#def",

            L"echo #abc",

            L"function yay\n"
            "echo hi\n"
            "end",

            L"cd foobar",

            L"ls /",

            NULL
        };

        history_t &test_history = history_t::history_with_name(name);
        if (! history_equals(test_history, expected))
        {
            err(L"test_history_formats failed for %ls\n", name);
        }
        test_history.clear();
    }

    name = L"history_sample_fish_2_0";
    say(L"Testing %ls", name);
    if (! install_sample_history(name))
    {
        err(L"Couldn't open file tests/%ls", name);
    }
    else
    {
        const wchar_t * const expected[] =
        {
            L"echo this has\\\nbackslashes",

            L"function foo\n"
            "echo bar\n"
            "end",

            L"echo alpha",

            NULL
        };

        history_t &test_history = history_t::history_with_name(name);
        if (! history_equals(test_history, expected))
        {
            err(L"test_history_formats failed for %ls\n", name);
        }
        test_history.clear();
    }

    say(L"Testing bash import");
    FILE *f = fopen("tests/history_sample_bash", "r");
    if (! f)
    {
        err(L"Couldn't open file tests/history_sample_bash");
    }
    else
    {
        // It should skip over the export command since that's a bash-ism
        const wchar_t *expected[] =
        {
            L"echo supsup",

            L"history --help",

            L"echo foo",

            NULL
        };
        history_t &test_history = history_t::history_with_name(L"bash_import");
        test_history.populate_from_bash(f);
        if (! history_equals(test_history, expected))
        {
            err(L"test_history_formats failed for bash import\n");
        }
        test_history.clear();
        fclose(f);
    }
}

void history_tests_t::test_history_speed(void)
{
    say(L"Testing history speed (pid is %d)", getpid());
    history_t *hist = new history_t(L"speed_test");
    wcstring item = L"History Speed Test - X";

    /* Test for 10 seconds */
    double start = timef();
    double end = start + 10;
    double stop = 0;
    size_t count = 0;
    for (;;)
    {
        item[item.size() - 1] = L'0' + (count % 10);
        hist->add(item);
        count++;

        stop = timef();
        if (stop >= end)
            break;
    }
    printf("%lu items - %.2f msec per item\n", (unsigned long)count, (stop - start) * 1E6 / count);
    hist->clear();
    delete hist;
}


/**
   Main test
*/
int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    srand(time(0));
    configure_thread_assertions_for_testing();

    program_name=L"(ignore)";

    say(L"Testing low-level functionality");
    say(L"Lines beginning with '(ignore):' are not errors, they are warning messages\ngenerated by the fish parser library when given broken input, and can be\nignored. All actual errors begin with 'Error:'.");
    set_main_thread();
    setup_fork_guards();
    proc_init();
    event_init();
    function_init();
    builtin_init();
    reader_init();
    env_init();

    test_format();
    test_escape();
    test_convert();
    test_convert_nulls();
    test_tok();
    test_fork();
    test_parser();
    test_utils();
    test_lru();
    test_expand();
    test_fuzzy_match();
    test_abbreviations();
    test_test();
    test_path();
    test_word_motion();
    test_is_potential_path();
    test_colors();
    test_complete();
    test_completion_insertions();
    test_autosuggestion_combining();
    test_autosuggest_suggest_special();
    history_tests_t::test_history();
    history_tests_t::test_history_merge();
    history_tests_t::test_history_races();
    history_tests_t::test_history_formats();
    //history_tests_t::test_history_speed();

    say(L"Encountered %d errors in low-level tests", err_count);

    /*
      Skip performance tests for now, since they seem to hang when running from inside make (?)
    */
//  say( L"Testing performance" );
//  perf_complete();

    env_destroy();
    reader_destroy();
    builtin_destroy();
    wutil_destroy();
    event_destroy();
    proc_destroy();

}
