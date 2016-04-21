/** \file fish_tests.c
  Various bug and feature tests. Compiled and run by make test.
*/
// IWYU pragma: no_include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <libgen.h>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <signal.h>
#include <locale.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <memory>
#include <set>
#include <vector>
#include <wctype.h>
#include <stdbool.h>
#include <sys/select.h>

#include "fallback.h"  // IWYU pragma: keep
#include "util.h"
#include "common.h"
#include "proc.h"
#include "reader.h"
#include "builtin.h"
#include "function.h"
#include "complete.h"
#include "wutil.h"  // IWYU pragma: keep
#include "env.h"
#include "expand.h"
#include "parser.h"
#include "tokenizer.h"
#include "event.h"
#include "path.h"
#include "history.h"
#include "highlight.h"
#include "iothread.h"
#include "signal.h"
#include "parse_tree.h"
#include "parse_util.h"
#include "pager.h"
#include "input.h"
#include "wildcard.h"
#include "utf8.h"
#include "env_universal_common.h"
#include "wcstringutil.h"
#include "color.h"
#include "io.h"
#include "lru.h"
#include "parse_constants.h"
#include "screen.h"
#include "input_common.h"

static const char * const * s_arguments;
static int s_test_run_count = 0;

/* Indicate if we should test the given function. Either we test everything (all arguments) or we run only tests that have a prefix in s_arguments */
static bool should_test_function(const char *func_name)
{
    /* No args, test everything */
    bool result = false;
    if (! s_arguments || ! s_arguments[0])
    {
        result = true;
    }
    else
    {
        for (size_t i=0; s_arguments[i] != NULL; i++)
        {
            if (! strncmp(func_name, s_arguments[i], strlen(s_arguments[i])))
            {
                /* Prefix match */
                result = true;
                break;
            }
        }
    }
    if (result)
        s_test_run_count++;
    return result;
}

/**
   The number of tests to run
 */
#define ESCAPE_TEST_COUNT 100000
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

    // Xcode's term doesn't support color (even though TERM claims it does)
    bool colorize = ! getenv("RUNNING_IN_XCODE");

    // show errors in red
    if (colorize)
    {
        fputs("\x1b[31m", stdout);
    }

    wprintf(L"Error: ");
    vwprintf(blah, va);
    va_end(va);

    // return to normal color
    if (colorize)
    {
        fputs("\x1b[0m", stdout);
    }

    wprintf(L"\n");
}

// Joins a wcstring_list_t via commas
static wcstring comma_join(const wcstring_list_t &lst)
{
    wcstring result;
    for (size_t i=0; i < lst.size(); i++)
    {
        if (i > 0)
        {
            result.push_back(L',');
        }
        result.append(lst.at(i));
    }
    return result;
}

/* Helper to chdir and then update $PWD */
static int chdir_set_pwd(const char *path)
{
    int ret = chdir(path);
    if (ret == 0)
    {
        env_set_pwd();
    }
    return ret;
}

#define do_test(e) do { if (! (e)) err(L"Test failed on line %lu: %s", __LINE__, #e); } while (0)

#define do_test1(e, msg) do { if (! (e)) err(L"Test failed on line %lu: %ls", __LINE__, (msg)); } while (0)

/* Test sane escapes */
static void test_unescape_sane()
{
    const struct test_t
    {
        const wchar_t * input;
        const wchar_t * expected;
    } tests[] =
    {
        {L"abcd", L"abcd"},
        {L"'abcd'", L"abcd"},
        {L"'abcd\\n'", L"abcd\\n"},
        {L"\"abcd\\n\"", L"abcd\\n"},
        {L"\"abcd\\n\"", L"abcd\\n"},
        {L"\\143", L"c"},
        {L"'\\143'", L"\\143"},
        {L"\\n", L"\n"} // \n normally becomes newline
    };
    wcstring output;
    for (size_t i=0; i < sizeof tests / sizeof *tests; i++)
    {
        bool ret = unescape_string(tests[i].input, &output, UNESCAPE_DEFAULT);
        if (! ret)
        {
            err(L"Failed to unescape '%ls'\n", tests[i].input);
        }
        else if (output != tests[i].expected)
        {
            err(L"In unescaping '%ls', expected '%ls' but got '%ls'\n", tests[i].input, tests[i].expected, output.c_str());
        }
    }

    // test for overflow
    if (unescape_string(L"echo \\UFFFFFF", &output, UNESCAPE_DEFAULT))
    {
        err(L"Should not have been able to unescape \\UFFFFFF\n");
    }
    if (unescape_string(L"echo \\U110000", &output, UNESCAPE_DEFAULT))
    {
        err(L"Should not have been able to unescape \\U110000\n");
    }
    if (! unescape_string(L"echo \\U10FFFF", &output, UNESCAPE_DEFAULT))
    {
        err(L"Should have been able to unescape \\U10FFFF\n");
    }


}

/**
   Test the escaping/unescaping code by escaping/unescaping random
   strings and verifying that the original string comes back.
*/

static void test_escape_crazy()
{
    say(L"Testing escaping and unescaping");
    wcstring random_string;
    wcstring escaped_string;
    wcstring unescaped_string;
    for (size_t i=0; i<ESCAPE_TEST_COUNT; i++)
    {
        random_string.clear();
        while (rand() % ESCAPE_TEST_LENGTH)
        {
            random_string.push_back((rand() % ESCAPE_TEST_CHAR) +1);
        }

        escaped_string = escape_string(random_string, ESCAPE_ALL);
        bool unescaped_success = unescape_string(escaped_string, &unescaped_string, UNESCAPE_DEFAULT);

        if (! unescaped_success)
        {
            err(L"Failed to unescape string <%ls>", escaped_string.c_str());
        }
        else if (unescaped_string != random_string)
        {
            err(L"Escaped and then unescaped string '%ls', but got back a different string '%ls'", random_string.c_str(), unescaped_string.c_str());
        }
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
        do_test(! strcmp(buff, tests[i].expected));
    }

    for (int j=-129; j <= 129; j++)
    {
        char buff1[128], buff2[128];
        format_long_safe(buff1, j);
        sprintf(buff2, "%d", j);
        do_test(! strcmp(buff1, buff2));

        wchar_t wbuf1[128], wbuf2[128];
        format_long_safe(wbuf1, j);
        swprintf(wbuf2, 128, L"%d", j);
        do_test(! wcscmp(wbuf1, wbuf2));

    }

    long q = LONG_MIN;
    char buff1[128], buff2[128];
    format_long_safe(buff1, q);
    sprintf(buff2, "%ld", q);
    do_test(! strcmp(buff1, buff2));
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
    {

        const wchar_t *str = L"string <redirection  2>&1 'nested \"quoted\" '(string containing subshells ){and,brackets}$as[$well (as variable arrays)] not_a_redirect^ ^ ^^is_a_redirect Compress_Newlines\n  \n\t\n   \nInto_Just_One";
        const int types[] =
        {
            TOK_STRING, TOK_REDIRECT_IN, TOK_STRING, TOK_REDIRECT_FD, TOK_STRING, TOK_STRING, TOK_STRING, TOK_REDIRECT_OUT, TOK_REDIRECT_APPEND, TOK_STRING, TOK_STRING, TOK_END, TOK_STRING
        };

        say(L"Test correct tokenization");

        tokenizer_t t(str, 0);
        tok_t token;
        size_t i = 0;
        while (t.next(&token))
        {
            if (i > sizeof types / sizeof *types)
            {
                err(L"Too many tokens returned from tokenizer");
                break;
            }
            if (types[i] != token.type)
            {
                err(L"Tokenization error:");
                wprintf(L"Token number %zu of string \n'%ls'\n, got token type %ld\n",
                        i + 1, str, (long)token.type);
            }
            i++;
        }
        if (i < sizeof types / sizeof *types)
        {
            err(L"Too few tokens returned from tokenizer");
        }
    }

    /* Test some errors */
    {
        tok_t token;
        tokenizer_t t(L"abc\\", 0);
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == TOK_UNTERMINATED_ESCAPE);
        do_test(token.error_offset == 3);
    }

    {
        tok_t token;
        tokenizer_t t(L"abc defg(hij (klm)", 0);
        do_test(t.next(&token));
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == TOK_UNTERMINATED_SUBSHELL);
        do_test(token.error_offset == 4);
    }

    {
        tok_t token;
        tokenizer_t t(L"abc defg[hij (klm)", 0);
        do_test(t.next(&token));
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == TOK_UNTERMINATED_SLICE);
        do_test(token.error_offset == 4);
    }

    /* Test redirection_type_for_string */
    if (redirection_type_for_string(L"<") != TOK_REDIRECT_IN) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"^") != TOK_REDIRECT_OUT) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L">") != TOK_REDIRECT_OUT) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>") != TOK_REDIRECT_OUT) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L">>") != TOK_REDIRECT_APPEND) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>>") != TOK_REDIRECT_APPEND) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>?") != TOK_REDIRECT_NOCLOB) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"9999999999999999>?") != TOK_NONE) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>&3") != TOK_REDIRECT_FD) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>|") != TOK_NONE) err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
}

// Little function that runs in the main thread
static int test_iothread_main_call(int *addr)
{
    *addr += 1;
    return *addr;
}

// Little function that runs in a background thread, bouncing to the main
static int test_iothread_thread_call(int *addr)
{
    int before = *addr;
    iothread_perform_on_main(test_iothread_main_call, addr);
    int after = *addr;

    // Must have incremented it at least once
    if (before >= after)
    {
        err(L"Failed to increment from background thread");
    }
    return after;
}

static void test_iothread(void)
{
    say(L"Testing iothreads");
    int *int_ptr = new int(0);
    int iterations = 50000;
    int max_achieved_thread_count = 0;
    double start = timef();
    for (int i=0; i < iterations; i++)
    {
        int thread_count = iothread_perform(test_iothread_thread_call, int_ptr);
        max_achieved_thread_count = std::max(max_achieved_thread_count, thread_count);
    }

    // Now wait until we're done
    iothread_drain_all();
    double end = timef();

    // Should have incremented it once per thread
    if (*int_ptr != iterations)
    {
        say(L"Expected int to be %d, but instead it was %d", iterations, *int_ptr);
    }

    say(L"    (%.02f msec, with max of %d threads)", (end - start) * 1000.0, max_achieved_thread_count);

    delete int_ptr;
}

static parser_test_error_bits_t detect_argument_errors(const wcstring &src)
{
    parse_node_tree_t tree;
    if (! parse_tree_from_string(src, parse_flag_none, &tree, NULL, symbol_argument_list))
    {
        return PARSER_TEST_ERROR;
    }

    assert(! tree.empty());
    const parse_node_t *first_arg = tree.next_node_in_node_list(tree.at(0), symbol_argument, NULL);
    assert(first_arg != NULL);
    return parse_util_detect_errors_in_argument(*first_arg, first_arg->get_source(src));
}

/**
   Test the parser
*/
static void test_parser()
{
    say(L"Testing parser");

    parser_t parser;

    say(L"Testing block nesting");
    if (!parse_util_detect_errors(L"if; end"))
    {
        err(L"Incomplete if statement undetected");
    }
    if (!parse_util_detect_errors(L"if test; echo"))
    {
        err(L"Missing end undetected");
    }
    if (!parse_util_detect_errors(L"if test; end; end"))
    {
        err(L"Unbalanced end undetected");
    }

    say(L"Testing detection of invalid use of builtin commands");
    if (!parse_util_detect_errors(L"case foo"))
    {
        err(L"'case' command outside of block context undetected");
    }
    if (!parse_util_detect_errors(L"switch ggg; if true; case foo;end;end"))
    {
        err(L"'case' command outside of switch block context undetected");
    }
    if (!parse_util_detect_errors(L"else"))
    {
        err(L"'else' command outside of conditional block context undetected");
    }
    if (!parse_util_detect_errors(L"else if"))
    {
        err(L"'else if' command outside of conditional block context undetected");
    }
    if (!parse_util_detect_errors(L"if false; else if; end"))
    {
        err(L"'else if' missing command undetected");
    }

    if (!parse_util_detect_errors(L"break"))
    {
        err(L"'break' command outside of loop block context undetected");
    }

    if (parse_util_detect_errors(L"break --help"))
    {
        err(L"'break --help' incorrectly marked as error");
    }

    if (! parse_util_detect_errors(L"while false ; function foo ; break ; end ; end "))
    {
        err(L"'break' command inside function allowed to break from loop outside it");
    }


    if (!parse_util_detect_errors(L"exec ls|less") || !parse_util_detect_errors(L"echo|return"))
    {
        err(L"Invalid pipe command undetected");
    }

    if (parse_util_detect_errors(L"for i in foo ; switch $i ; case blah ; break; end; end "))
    {
        err(L"'break' command inside switch falsely reported as error");
    }

    if (parse_util_detect_errors(L"or cat | cat") || parse_util_detect_errors(L"and cat | cat"))
    {
        err(L"boolean command at beginning of pipeline falsely reported as error");
    }

    if (! parse_util_detect_errors(L"cat | and cat"))
    {
        err(L"'and' command in pipeline not reported as error");
    }

    if (! parse_util_detect_errors(L"cat | or cat"))
    {
        err(L"'or' command in pipeline not reported as error");
    }

    if (! parse_util_detect_errors(L"cat | exec") || ! parse_util_detect_errors(L"exec | cat"))
    {
        err(L"'exec' command in pipeline not reported as error");
    }

    if (detect_argument_errors(L"foo"))
    {
        err(L"simple argument reported as error");
    }

    if (detect_argument_errors(L"''"))
    {
        err(L"Empty string reported as error");
    }


    if (!(detect_argument_errors(L"foo$$") & PARSER_TEST_ERROR))
    {
        err(L"Bad variable expansion not reported as error");
    }

    if (!(detect_argument_errors(L"foo$@") & PARSER_TEST_ERROR))
    {
        err(L"Bad variable expansion not reported as error");
    }

    /* Within command substitutions, we should be able to detect everything that parse_util_detect_errors can detect */
    if (!(detect_argument_errors(L"foo(cat | or cat)") & PARSER_TEST_ERROR))
    {
        err(L"Bad command substitution not reported as error");
    }

    if (!(detect_argument_errors(L"foo\\xFF9") & PARSER_TEST_ERROR))
    {
        err(L"Bad escape not reported as error");
    }

    if (!(detect_argument_errors(L"foo(echo \\xFF9)") & PARSER_TEST_ERROR))
    {
        err(L"Bad escape in command substitution not reported as error");
    }

    if (!(detect_argument_errors(L"foo(echo (echo (echo \\xFF9)))") & PARSER_TEST_ERROR))
    {
        err(L"Bad escape in nested command substitution not reported as error");
    }

    if (! parse_util_detect_errors(L"false & ; and cat"))
    {
        err(L"'and' command after background not reported as error");
    }

    if (! parse_util_detect_errors(L"true & ; or cat"))
    {
        err(L"'or' command after background not reported as error");
    }

    if (parse_util_detect_errors(L"true & ; not cat"))
    {
        err(L"'not' command after background falsely reported as error");
    }


    if (! parse_util_detect_errors(L"if true & ; end"))
    {
        err(L"backgrounded 'if' conditional not reported as error");
    }

    if (! parse_util_detect_errors(L"if false; else if true & ; end"))
    {
        err(L"backgrounded 'else if' conditional not reported as error");
    }

    if (! parse_util_detect_errors(L"while true & ; end"))
    {
        err(L"backgrounded 'while' conditional not reported as error");
    }

    say(L"Testing basic evaluation");

    /* Ensure that we don't crash on infinite self recursion and mutual recursion. These must use the principal parser because we cannot yet execute jobs on other parsers (!) */
    say(L"Testing recursion detection");
    parser_t::principal_parser().eval(L"function recursive ; recursive ; end ; recursive; ", io_chain_t(), TOP);
#if 0
    /* This is disabled since it produces a long backtrace. We should find a way to either visually compress the backtrace, or disable error spewing */
    parser_t::principal_parser().eval(L"function recursive1 ; recursive2 ; end ; function recursive2 ; recursive1 ; end ; recursive1; ", io_chain_t(), TOP);
#endif

    say(L"Testing empty function name");
    parser_t::principal_parser().eval(L"function '' ; echo fail; exit 42 ; end ; ''", io_chain_t(), TOP);

    say(L"Testing eval_args");
    completion_list_t comps;
    parser_t::expand_argument_list(L"alpha 'beta gamma' delta", 0, &comps);
    do_test(comps.size() == 3);
    do_test(comps.at(0).completion == L"alpha");
    do_test(comps.at(1).completion == L"beta gamma");
    do_test(comps.at(2).completion == L"delta");
}

/* Wait a while and then SIGINT the main thread */
struct test_cancellation_info_t
{
    pthread_t thread;
    double delay;
};

static int signal_main(test_cancellation_info_t *info)
{
    usleep(info->delay * 1E6);
    pthread_kill(info->thread, SIGINT);
    return 0;
}

static void test_1_cancellation(const wchar_t *src)
{
    shared_ptr<io_buffer_t> out_buff(io_buffer_t::create(STDOUT_FILENO, io_chain_t()));
    const io_chain_t io_chain(out_buff);
    test_cancellation_info_t ctx = {pthread_self(), 0.25 /* seconds */ };
    iothread_perform(signal_main, &ctx);
    parser_t::principal_parser().eval(src, io_chain, TOP);
    out_buff->read();
    if (out_buff->out_buffer_size() != 0)
    {
        err(L"Expected 0 bytes in out_buff, but instead found %lu bytes\n", out_buff->out_buffer_size());
    }
    iothread_drain_all();
}

static void test_cancellation()
{
    if (getenv("RUNNING_IN_XCODE")) {
        say(L"Skipping Ctrl-C cancellation test because we are running in Xcode debugger");
        return;
    }
    say(L"Testing Ctrl-C cancellation. If this hangs, that's a bug!");

    /* Enable fish's signal handling here. We need to make this interactive for fish to install its signal handlers */
    proc_push_interactive(1);
    signal_set_handlers();

    /* This tests that we can correctly ctrl-C out of certain loop constructs, and that nothing gets printed if we do */

    /* Here the command substitution is an infinite loop. echo never even gets its argument, so when we cancel we expect no output */
    test_1_cancellation(L"echo (while true ; echo blah ; end)");

    fprintf(stderr, ".");

    /* Nasty infinite loop that doesn't actually execute anything */
    test_1_cancellation(L"echo (while true ; end) (while true ; end) (while true ; end)");
    fprintf(stderr, ".");

    test_1_cancellation(L"while true ; end");
    fprintf(stderr, ".");

    test_1_cancellation(L"for i in (while true ; end) ; end");
    fprintf(stderr, ".");

    fprintf(stderr, "\n");

    /* Restore signal handling */
    proc_pop_interactive();
    signal_reset_handlers();

    /* Ensure that we don't think we should cancel */
    reader_reset_interrupted();
}

static void test_indents()
{
    say(L"Testing indents");

    // Here are the components of our source and the indents we expect those to be
    struct indent_component_t
    {
        const wchar_t *txt;
        int indent;
    };

    const indent_component_t components1[] =
    {
        {L"if foo", 0},
        {L"end", 0},
        {NULL, -1}
    };

    const indent_component_t components2[] =
    {
        {L"if foo", 0},
        {L"", 1}, //trailing newline!
        {NULL, -1}
    };

    const indent_component_t components3[] =
    {
        {L"if foo", 0},
        {L"foo", 1},
        {L"end", 0}, //trailing newline!
        {NULL, -1}
    };

    const indent_component_t components4[] =
    {
        {L"if foo", 0},
        {L"if bar", 1},
        {L"end", 1},
        {L"end", 0},
        {L"", 0},
        {NULL, -1}
    };

    const indent_component_t components5[] =
    {
        {L"if foo", 0},
        {L"if bar", 1},
        {L"", 2},
        {NULL, -1}
    };

    const indent_component_t components6[] =
    {
        {L"begin", 0},
        {L"foo", 1},
        {L"", 1},
        {NULL, -1}
    };

    const indent_component_t components7[] =
    {
        {L"begin", 0},
        {L";", 1},
        {L"end", 0},
        {L"foo", 0},
        {L"", 0},
        {NULL, -1}
    };

    const indent_component_t components8[] =
    {
        {L"if foo", 0},
        {L"if bar", 1},
        {L"baz", 2},
        {L"end", 1},
        {L"", 1},
        {NULL, -1}
    };

    const indent_component_t components9[] =
    {
        {L"switch foo", 0},
        {L"", 1},
        {NULL, -1}
    };

    const indent_component_t components10[] =
    {
        {L"switch foo", 0},
        {L"case bar", 1},
        {L"case baz", 1},
        {L"quux", 2},
        {L"", 2},
        {NULL, -1}
    };

    const indent_component_t components11[] =
    {
        {L"switch foo", 0},
        {L"cas", 1}, //parse error indentation handling
        {NULL, -1}
    };

    const indent_component_t components12[] =
    {
        {L"while false", 0},
        {L"# comment", 1}, //comment indentation handling
        {L"command", 1}, //comment indentation handling
        {L"# comment2", 1}, //comment indentation handling
        {NULL, -1}
    };


    const indent_component_t *tests[] = {components1, components2, components3, components4, components5, components6, components7, components8, components9, components10, components11, components12};
    for (size_t which = 0; which < sizeof tests / sizeof *tests; which++)
    {
        const indent_component_t *components = tests[which];
        // Count how many we have
        size_t component_count = 0;
        while (components[component_count].txt != NULL)
        {
            component_count++;
        }

        // Generate the expected indents
        wcstring text;
        std::vector<int> expected_indents;
        for (size_t i=0; i < component_count; i++)
        {
            if (i > 0)
            {
                text.push_back(L'\n');
                expected_indents.push_back(components[i].indent);
            }
            text.append(components[i].txt);
            expected_indents.resize(text.size(), components[i].indent);
        }
        do_test(expected_indents.size() == text.size());

        // Compute the indents
        std::vector<int> indents = parse_util_compute_indents(text);

        if (expected_indents.size() != indents.size())
        {
            err(L"Indent vector has wrong size! Expected %lu, actual %lu", expected_indents.size(), indents.size());
        }
        do_test(expected_indents.size() == indents.size());
        for (size_t i=0; i < text.size(); i++)
        {
            if (expected_indents.at(i) != indents.at(i))
            {
                err(L"Wrong indent at index %lu in test #%lu (expected %d, actual %d):\n%ls\n", i, which + 1, expected_indents.at(i), indents.at(i), text.c_str());
                break; //don't keep showing errors for the rest of the line
            }
        }

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

/* UTF8 tests taken from Alexey Vatchenko's utf8 library. See http://www.bsdua.org/libbsdua.html */

static void test_utf82wchar(const char *src, size_t slen, const wchar_t *dst, size_t dlen,
                            int flags, size_t res, const char *descr)
{
    size_t size;
    wchar_t *mem = NULL;

    /* Hack: if wchar is only UCS-2, and the UTF-8 input string contains astral characters, then tweak the expected size to 0 */
    if (src != NULL && is_wchar_ucs2())
    {
        /* A UTF-8 code unit may represent an astral code point if it has 4 or more leading 1s */
        const unsigned char astral_mask = 0xF0;
        for (size_t i=0; i < slen; i++)
        {
            if ((src[i] & astral_mask) == astral_mask)
            {
                /* Astral char. We expect this conversion to just fail. */
                res = 0;
                break;
            }
        }
    }

    if (dst != NULL)
    {
        mem = (wchar_t *)malloc(dlen * sizeof(*mem));
        if (mem == NULL)
        {
            err(L"u2w: %s: MALLOC FAILED\n", descr);
            return;
        }
    }

    do
    {
        if (mem == NULL)
        {
            size = utf8_to_wchar(src, slen, NULL, flags);
        }
        else
        {
            std::wstring buff;
            size = utf8_to_wchar(src, slen, &buff, flags);
            std::copy(buff.begin(), buff.begin() + std::min(dlen, buff.size()), mem);
        }
        if (res != size)
        {
            err(L"u2w: %s: FAILED (rv: %lu, must be %lu)", descr, size, res);
            break;
        }

        if (mem == NULL)
            break;		/* OK */

        if (memcmp(mem, dst, size * sizeof(*mem)) != 0)
        {
            err(L"u2w: %s: BROKEN", descr);
            break;
        }

    }
    while (0);

    free(mem);
}

// Annoying variant to handle uchar to avoid narrowing conversion warnings
static void test_utf82wchar(const unsigned char *usrc, size_t slen, const wchar_t *dst, size_t dlen,
                            int flags, size_t res, const char *descr) {
    const char *src = reinterpret_cast<const char *>(usrc);
    return test_utf82wchar(src, slen, dst, dlen, flags, res, descr);
}

static void test_wchar2utf8(const wchar_t *src, size_t slen, const char *dst, size_t dlen,
                            int flags, size_t res, const char *descr)
{
    size_t size;
    char *mem = NULL;

    /* Hack: if wchar is simulating UCS-2, and the wchar_t input string contains astral characters, then tweak the expected size to 0 */
    if (src != NULL && is_wchar_ucs2())
    {
        const uint32_t astral_mask = 0xFFFF0000U;
        for (size_t i=0; i < slen; i++)
        {
            if ((src[i] & astral_mask) != 0)
            {
                /* astral char */
                res = 0;
                break;
            }
        }
    }

    if (dst != NULL)
    {
        mem = (char *)malloc(dlen);
        if (mem == NULL)
        {
            err(L"w2u: %s: MALLOC FAILED", descr);
            return;
        }
    }

    size = wchar_to_utf8(src, slen, mem, dlen, flags);
    if (res != size)
    {
        err(L"w2u: %s: FAILED (rv: %lu, must be %lu)", descr, size, res);
        goto finish;
    }

    if (mem == NULL)
        goto finish;		/* OK */

    if (memcmp(mem, dst, size) != 0)
    {
        err(L"w2u: %s: BROKEN", descr);
        goto finish;
    }

    finish:
    free(mem);
}

// Annoying variant to handle uchar to avoid narrowing conversion warnings
static void test_wchar2utf8(const wchar_t *src, size_t slen, const unsigned char *udst, size_t dlen,
                            int flags, size_t res, const char *descr)
{
    const char *dst = reinterpret_cast<const char *>(udst);
    return test_wchar2utf8(src, slen, dst, dlen, flags, res, descr);
}

static void test_utf8()
{
    wchar_t w1[] = {0x54, 0x65, 0x73, 0x74};
    wchar_t w2[] = {0x0422, 0x0435, 0x0441, 0x0442};
    wchar_t w3[] = {0x800, 0x1e80, 0x98c4, 0x9910, 0xff00};
    wchar_t w4[] = {0x15555, 0xf7777, 0xa};
    wchar_t w5[] = {0x255555, 0x1fa04ff, 0xddfd04, 0xa};
    wchar_t w6[] = {0xf255555, 0x1dfa04ff, 0x7fddfd04, 0xa};
    wchar_t wb[] = {(wchar_t)-2, 0xa, (wchar_t)0xffffffff, 0x0441};
    wchar_t wm[] = {0x41, 0x0441, 0x3042, 0xff67, 0x9b0d, 0x2e05da67};
    wchar_t wb1[] = {0xa, 0x0422};
    wchar_t wb2[] = {0xd800, 0xda00, 0x41, 0xdfff, 0xa};
    wchar_t wbom[] = {0xfeff, 0x41, 0xa};
    wchar_t wbom2[] = {0x41, 0xa};
    wchar_t wbom22[] = {0xfeff, 0x41, 0xa};
    unsigned char u1[] = {0x54, 0x65, 0x73, 0x74};
    unsigned char u2[] = {0xd0, 0xa2, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82};
    unsigned char u3[] = {0xe0, 0xa0, 0x80, 0xe1, 0xba, 0x80, 0xe9, 0xa3, 0x84,
                 0xe9, 0xa4, 0x90, 0xef, 0xbc, 0x80
                };
    unsigned char u4[] = {0xf0, 0x95, 0x95, 0x95, 0xf3, 0xb7, 0x9d, 0xb7, 0xa};
    unsigned char u5[] = {0xf8, 0x89, 0x95, 0x95, 0x95, 0xf9, 0xbe, 0xa0, 0x93,
                 0xbf, 0xf8, 0xb7, 0x9f, 0xb4, 0x84, 0x0a
                };
    unsigned char u6[] = {0xfc, 0x8f, 0x89, 0x95, 0x95, 0x95, 0xfc, 0x9d, 0xbe,
                 0xa0, 0x93, 0xbf, 0xfd, 0xbf, 0xb7, 0x9f, 0xb4, 0x84, 0x0a
                };
    unsigned char ub[] = {0xa, 0xd1, 0x81};
    unsigned char um[] = {0x41, 0xd1, 0x81, 0xe3, 0x81, 0x82, 0xef, 0xbd, 0xa7,
                 0xe9, 0xac, 0x8d, 0xfc, 0xae, 0x81, 0x9d, 0xa9, 0xa7
                };
    unsigned char ub1[] = {0xa, 0xff, 0xd0, 0xa2, 0xfe, 0x8f, 0xe0, 0x80};
    unsigned char uc080[] = {0xc0, 0x80};
    unsigned char ub2[] = {0xed, 0xa1, 0x8c, 0xed, 0xbe, 0xb4, 0xa};
    unsigned char ubom[] = {0x41, 0xa};
    unsigned char ubom2[] = {0xef, 0xbb, 0xbf, 0x41, 0xa};

    /*
     * UTF-8 -> UCS-4 string.
     */
    test_utf82wchar(ubom2, sizeof(ubom2), wbom2,
                    sizeof(wbom2) / sizeof(*wbom2), UTF8_SKIP_BOM,
                    sizeof(wbom2) / sizeof(*wbom2), "skip BOM");
    test_utf82wchar(ubom2, sizeof(ubom2), wbom22,
                    sizeof(wbom22) / sizeof(*wbom22), 0,
                    sizeof(wbom22) / sizeof(*wbom22), "BOM");
    test_utf82wchar(uc080, sizeof(uc080), NULL, 0, 0, 0,
                    "c0 80 - forbitten by rfc3629");
    test_utf82wchar(ub2, sizeof(ub2), NULL, 0, 0, is_wchar_ucs2() ? 0 : 3,
                    "resulted in forbitten wchars (len)");
    test_utf82wchar(ub2, sizeof(ub2), wb2, sizeof(wb2) / sizeof(*wb2), 0, 0,
                    "resulted in forbitten wchars");
    test_utf82wchar(ub2, sizeof(ub2), L"\x0a", 1, UTF8_IGNORE_ERROR,
                    1, "resulted in ignored forbitten wchars");
    test_utf82wchar(u1, sizeof(u1), w1, sizeof(w1) / sizeof(*w1), 0,
                    sizeof(w1) / sizeof(*w1), "1 octet chars");
    test_utf82wchar(u2, sizeof(u2), w2, sizeof(w2) / sizeof(*w2), 0,
                    sizeof(w2) / sizeof(*w2), "2 octets chars");
    test_utf82wchar(u3, sizeof(u3), w3, sizeof(w3) / sizeof(*w3), 0,
                    sizeof(w3) / sizeof(*w3), "3 octets chars");
    test_utf82wchar(u4, sizeof(u4), w4, sizeof(w4) / sizeof(*w4), 0,
                    sizeof(w4) / sizeof(*w4), "4 octets chars");
    test_utf82wchar(u5, sizeof(u5), w5, sizeof(w5) / sizeof(*w5), 0,
                    sizeof(w5) / sizeof(*w5), "5 octets chars");
    test_utf82wchar(u6, sizeof(u6), w6, sizeof(w6) / sizeof(*w6), 0,
                    sizeof(w6) / sizeof(*w6), "6 octets chars");
    test_utf82wchar("\xff", 1, NULL, 0, 0, 0, "broken utf-8 0xff symbol");
    test_utf82wchar("\xfe", 1, NULL, 0, 0, 0, "broken utf-8 0xfe symbol");
    test_utf82wchar("\x8f", 1, NULL, 0, 0, 0,
                    "broken utf-8, start from 10 higher bits");
    if (! is_wchar_ucs2()) test_utf82wchar(ub1, sizeof(ub1), wb1, sizeof(wb1) / sizeof(*wb1),
                                               UTF8_IGNORE_ERROR, sizeof(wb1) / sizeof(*wb1), "ignore bad chars");
    test_utf82wchar(um, sizeof(um), wm, sizeof(wm) / sizeof(*wm), 0,
                    sizeof(wm) / sizeof(*wm), "mixed languages");
    // PCA this test was to ensure that if the output buffer was too small, we'd get 0
    // we no longer have statically sized result buffers, so this test is disabled
    //    test_utf82wchar(um, sizeof(um), wm, sizeof(wm) / sizeof(*wm) - 1, 0,
    //                    0, "boundaries -1");
    test_utf82wchar(um, sizeof(um), wm, sizeof(wm) / sizeof(*wm) + 1, 0,
                    sizeof(wm) / sizeof(*wm), "boundaries +1");
    test_utf82wchar(um, sizeof(um), NULL, 0, 0,
                    sizeof(wm) / sizeof(*wm), "calculate length");
    test_utf82wchar(ub1, sizeof(ub1), NULL, 0, 0,
                    0, "calculate length of bad chars");
    test_utf82wchar(ub1, sizeof(ub1), NULL, 0,
                    UTF8_IGNORE_ERROR, sizeof(wb1) / sizeof(*wb1),
                    "calculate length, ignore bad chars");
    test_utf82wchar((const char *)NULL, 0, NULL, 0, 0, 0, "invalid params, all 0");
    test_utf82wchar(u1, 0, NULL, 0, 0, 0,
                    "invalid params, src buf not NULL");
    test_utf82wchar((const char *)NULL, 10, NULL, 0, 0, 0,
                    "invalid params, src length is not 0");
    
    // PCA this test was to ensure that converting into a zero length output buffer would return 0
    // we no longer statically size output buffers, so the test is disabled
    //    test_utf82wchar(u1, sizeof(u1), w1, 0, 0, 0,
    //                    "invalid params, dst is not NULL");

    /*
     * UCS-4 -> UTF-8 string.
     */
    const char * const nullc = NULL;
    test_wchar2utf8(wbom, sizeof(wbom) / sizeof(*wbom), ubom, sizeof(ubom),
                    UTF8_SKIP_BOM, sizeof(ubom), "BOM");
    test_wchar2utf8(wb2, sizeof(wb2) / sizeof(*wb2), nullc, 0, 0,
                    0, "prohibited wchars");
    test_wchar2utf8(wb2, sizeof(wb2) / sizeof(*wb2), nullc, 0,
                    UTF8_IGNORE_ERROR, 2, "ignore prohibited wchars");
    test_wchar2utf8(w1, sizeof(w1) / sizeof(*w1), u1, sizeof(u1), 0,
                    sizeof(u1), "1 octet chars");
    test_wchar2utf8(w2, sizeof(w2) / sizeof(*w2), u2, sizeof(u2), 0,
                    sizeof(u2), "2 octets chars");
    test_wchar2utf8(w3, sizeof(w3) / sizeof(*w3), u3, sizeof(u3), 0,
                    sizeof(u3), "3 octets chars");
    test_wchar2utf8(w4, sizeof(w4) / sizeof(*w4), u4, sizeof(u4), 0,
                    sizeof(u4), "4 octets chars");
    test_wchar2utf8(w5, sizeof(w5) / sizeof(*w5), u5, sizeof(u5), 0,
                    sizeof(u5), "5 octets chars");
    test_wchar2utf8(w6, sizeof(w6) / sizeof(*w6), u6, sizeof(u6), 0,
                    sizeof(u6), "6 octets chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), ub, sizeof(ub), 0,
                    0, "bad chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), ub, sizeof(ub),
                    UTF8_IGNORE_ERROR, sizeof(ub), "ignore bad chars");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), um, sizeof(um), 0,
                    sizeof(um), "mixed languages");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), um, sizeof(um) - 1, 0,
                    0, "boundaries -1");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), um, sizeof(um) + 1, 0,
                    sizeof(um), "boundaries +1");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), nullc, 0, 0,
                    sizeof(um), "calculate length");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), nullc, 0, 0,
                    0, "calculate length of bad chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), nullc, 0,
                    UTF8_IGNORE_ERROR, sizeof(ub),
                    "calculate length, ignore bad chars");
    test_wchar2utf8(NULL, 0, nullc, 0, 0, 0, "invalid params, all 0");
    test_wchar2utf8(w1, 0, nullc, 0, 0, 0,
                    "invalid params, src buf not NULL");
    test_wchar2utf8(NULL, 10, nullc, 0, 0, 0,
                    "invalid params, src length is not 0");
    test_wchar2utf8(w1, sizeof(w1) / sizeof(*w1), u1, 0, 0, 0,
                    "invalid params, dst is not NULL");
}

static void test_escape_sequences(void)
{
    say(L"Testing escape codes");
    if (escape_code_length(L"") != 0) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"abcd") != 0) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b[2J") != 4) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b[38;5;123mABC") != strlen("\x1b[38;5;123m")) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b@") != 2) err(L"test_escape_sequences failed on line %d\n", __LINE__);

    // iTerm2 escape sequences
    if (escape_code_length(L"\x1b]50;CurrentDir=/tmp/foo\x07NOT_PART_OF_SEQUENCE") != 25) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b]50;SetMark\x07NOT_PART_OF_SEQUENCE") != 13) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b" L"]6;1;bg;red;brightness;255\x07NOT_PART_OF_SEQUENCE") != 28) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b]Pg4040ff\x1b\\NOT_PART_OF_SEQUENCE") != 12) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b]blahblahblah\x1b\\") != 16) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b]blahblahblah\x07") != 15) err(L"test_escape_sequences failed on line %d\n", __LINE__);
}

class lru_node_test_t : public lru_node_t
{
public:
    explicit lru_node_test_t(const wcstring &tmp) : lru_node_t(tmp) { }
};

class test_lru_t : public lru_cache_t<lru_node_test_t>
{
public:
    test_lru_t() : lru_cache_t<lru_node_test_t>(16) { }

    std::vector<lru_node_test_t *> evicted_nodes;

    virtual void node_was_evicted(lru_node_test_t *node)
    {
        do_test(find(evicted_nodes.begin(), evicted_nodes.end(), node) == evicted_nodes.end());
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
        do_test(cache.size() == std::min(i, (size_t)16));
        lru_node_test_t *node = new lru_node_test_t(to_string(i));
        if (i < 4) expected_evicted.push_back(node);
        // Adding the node the first time should work, and subsequent times should fail
        do_test(cache.add_node(node));
        do_test(! cache.add_node(node));
    }
    do_test(cache.evicted_nodes == expected_evicted);
    cache.evict_all_nodes();
    do_test(cache.evicted_nodes.size() == total_nodes);
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
   \param ... A zero-terminated parameter list of values to test.
              After the zero terminator comes one more arg, a string, which is the error
              message to print if the test fails.
*/

static bool expand_test(const wchar_t *in, expand_flags_t flags, ...)
{
    std::vector<completion_t> output;
    va_list va;
    bool res=true;
    wchar_t *arg;
    parse_error_list_t errors;

    if (expand_string(in, &output, flags, &errors) == EXPAND_ERROR)
    {
        if (errors.empty())
        {
            err(L"Bug: Parse error reported but no error text found.");
        }
        else
        {
            err(L"%ls", errors.at(0).describe(wcstring(in)).c_str());
        }
        return false;
    }

    wcstring_list_t expected;

    va_start(va, flags);
    while ((arg=va_arg(va, wchar_t *))!= 0)
    {
        expected.push_back(wcstring(arg));
    }
    va_end(va);

    std::set<wcstring> remaining(expected.begin(), expected.end());
    std::vector<completion_t>::const_iterator out_it = output.begin(), out_end = output.end();
    for (; out_it != out_end; ++out_it)
    {
        if (! remaining.erase(out_it->completion))
        {
            res = false;
            break;
        }
    }
    if (! remaining.empty())
    {
        res = false;
    }

    if (!res)
    {
        if ((arg = va_arg(va, wchar_t *)) != 0)
        {
            wcstring msg = L"Expected [";
            bool first = true;
            for (wcstring_list_t::const_iterator it = expected.begin(), end = expected.end(); it != end; ++it)
            {
                if (!first) msg += L", ";
                first = false;
                msg += '"';
                msg += *it;
                msg += '"';
            }
            msg += L"], found [";
            first = true;
            for (std::vector<completion_t>::const_iterator it = output.begin(), end = output.end(); it != end; ++it)
            {
                if (!first) msg += L", ";
                first = false;
                msg += '"';
                msg += it->completion;
                msg += '"';
            }
            msg += L"]";
            err(L"%ls\n%ls", arg, msg.c_str());
        }
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

    expand_test(L"foo", 0, L"foo", 0,
                L"Strings do not expand to themselves");

    expand_test(L"a{b,c,d}e", 0, L"abe", L"ace", L"ade", 0,
                L"Bracket expansion is broken");

    expand_test(L"a*", EXPAND_SKIP_WILDCARDS, L"a*", 0,
                L"Cannot skip wildcard expansion");

    expand_test(L"/bin/l\\0", EXPAND_FOR_COMPLETIONS, 0,
                L"Failed to handle null escape in expansion");

    expand_test(L"foo\\$bar", EXPAND_SKIP_VARIABLES, L"foo$bar", 0,
                L"Failed to handle dollar sign in variable-skipping expansion");


    /*
       b
          x
       bar
       baz
          xxx
          yyy
       bax
          xxx
       lol
          nub
             q
       .foo
     */

    if (system("mkdir -p /tmp/fish_expand_test/")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/fish_expand_test/b/")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/fish_expand_test/baz/")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/fish_expand_test/bax/")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/fish_expand_test/lol/nub/")) err(L"mkdir failed");
    if (system("touch /tmp/fish_expand_test/.foo")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/b/x")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/bar")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/bax/xxx")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/baz/xxx")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/baz/yyy")) err(L"touch failed");
    if (system("touch /tmp/fish_expand_test/lol/nub/q")) err(L"touch failed");

    // This is checking that .* does NOT match . and .. (https://github.com/fish-shell/fish-shell/issues/270). But it does have to match literal components (e.g. "./*" has to match the same as "*"
    const wchar_t * const wnull = NULL;
    expand_test(L"/tmp/fish_expand_test/.*", 0,
                L"/tmp/fish_expand_test/.foo", wnull,
                L"Expansion not correctly handling dotfiles");

    expand_test(L"/tmp/fish_expand_test/./.*", 0,
                L"/tmp/fish_expand_test/./.foo", wnull,
                L"Expansion not correctly handling literal path components in dotfiles");

    expand_test(L"/tmp/fish_expand_test/*/xxx", 0,
                L"/tmp/fish_expand_test/bax/xxx", L"/tmp/fish_expand_test/baz/xxx", wnull,
                L"Glob did the wrong thing 1");

    expand_test(L"/tmp/fish_expand_test/*z/xxx", 0,
                L"/tmp/fish_expand_test/baz/xxx", wnull,
                L"Glob did the wrong thing 2");

    expand_test(L"/tmp/fish_expand_test/**z/xxx", 0,
                L"/tmp/fish_expand_test/baz/xxx", wnull,
                L"Glob did the wrong thing 3");

    expand_test(L"/tmp/fish_expand_test/b**", 0,
                L"/tmp/fish_expand_test/b", L"/tmp/fish_expand_test/b/x", L"/tmp/fish_expand_test/bar", L"/tmp/fish_expand_test/bax", L"/tmp/fish_expand_test/bax/xxx", L"/tmp/fish_expand_test/baz", L"/tmp/fish_expand_test/baz/xxx", L"/tmp/fish_expand_test/baz/yyy", wnull,
                L"Glob did the wrong thing 4");
   
    // a trailing slash should only produce directories
    expand_test(L"/tmp/fish_expand_test/b*/", 0,
                L"/tmp/fish_expand_test/b/", L"/tmp/fish_expand_test/baz/", L"/tmp/fish_expand_test/bax/", wnull,
                L"Glob did the wrong thing 5");

    expand_test(L"/tmp/fish_expand_test/b**/", 0,
                L"/tmp/fish_expand_test/b/", L"/tmp/fish_expand_test/baz/", L"/tmp/fish_expand_test/bax/", wnull,
                L"Glob did the wrong thing 6");
 
    expand_test(L"/tmp/fish_expand_test/**/q", 0,
                L"/tmp/fish_expand_test/lol/nub/q", wnull,
                L"Glob did the wrong thing 7");
    
    expand_test(L"/tmp/fish_expand_test/BA", EXPAND_FOR_COMPLETIONS,
                L"/tmp/fish_expand_test/bar", L"/tmp/fish_expand_test/bax/",  L"/tmp/fish_expand_test/baz/", wnull,
                L"Case insensitive test did the wrong thing");

    expand_test(L"/tmp/fish_expand_test/BA", EXPAND_FOR_COMPLETIONS,
                L"/tmp/fish_expand_test/bar", L"/tmp/fish_expand_test/bax/",  L"/tmp/fish_expand_test/baz/", wnull,
                L"Case insensitive test did the wrong thing");

    expand_test(L"/tmp/fish_expand_test/b/yyy", EXPAND_FOR_COMPLETIONS,
                /* nothing! */ wnull,
                L"Wrong fuzzy matching 1");

    expand_test(L"/tmp/fish_expand_test/b/x", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                L"", wnull, // We just expect the empty string since this is an exact match
                L"Wrong fuzzy matching 2");

    // some vswprintfs refuse to append ANY_STRING in a format specifiers, so don't use format_string here
    const wcstring any_str_str(1, ANY_STRING);
    expand_test(L"/tmp/fish_expand_test/b/xx*", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                (L"/tmp/fish_expand_test/bax/xx" + any_str_str).c_str(), (L"/tmp/fish_expand_test/baz/xx" + any_str_str).c_str(), wnull,
                L"Wrong fuzzy matching 3");

    expand_test(L"/tmp/fish_expand_test/b/yyy", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                L"/tmp/fish_expand_test/baz/yyy", wnull,
                L"Wrong fuzzy matching 4");

    if (! expand_test(L"/tmp/fish_expand_test/.*", 0, L"/tmp/fish_expand_test/.foo", 0))
    {
        err(L"Expansion not correctly handling dotfiles");
    }
    if (! expand_test(L"/tmp/fish_expand_test/./.*", 0, L"/tmp/fish_expand_test/./.foo", 0))
    {
        err(L"Expansion not correctly handling literal path components in dotfiles");
    }

    char saved_wd[PATH_MAX] = {};
    if (NULL == getcwd(saved_wd, sizeof saved_wd))
    {
        err(L"getcwd failed");
        return;
    }

    if (chdir_set_pwd("/tmp/fish_expand_test"))
    {
        err(L"chdir failed");
        return;
    }

    expand_test(L"b/xx", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                L"bax/xxx", L"baz/xxx", wnull,
                L"Wrong fuzzy matching 5");

    if (chdir_set_pwd(saved_wd))
    {
        err(L"chdir failed");
    }


    if (system("rm -Rf /tmp/fish_expand_test")) err(L"rm failed");
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
        L"foo=bar" ARRAY_SEP_STR
        L"gx git checkout";

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

    /* space separation */
    expanded = reader_expand_abbreviation_in_command(L"gx somebranch", wcslen(L"gc"), &result);
    if (! expanded) err(L"gx not expanded");
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

    /* others should not be */
    expanded = reader_expand_abbreviation_in_command(L"command gc", wcslen(L"command gc"), &result);
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

static void test_pager_navigation()
{
    say(L"Testing pager navigation");

    /* Generate 19 strings of width 10. There's 2 spaces between completions, and our term size is 80; these can therefore fit into 6 columns (6 * 12 - 2 = 70) or 5 columns (58) but not 7 columns (7 * 12 - 2 = 82).

       You can simulate this test by creating 19 files named "file00.txt" through "file_18.txt".
    */
    completion_list_t completions;
    for (size_t i=0; i < 19; i++)
    {
        append_completion(&completions, L"abcdefghij");
    }

    pager_t pager;
    pager.set_completions(completions);
    pager.set_term_size(80, 24);
    page_rendering_t render = pager.render();

    if (render.term_width != 80)
        err(L"Wrong term width");
    if (render.term_height != 24)
        err(L"Wrong term height");

    size_t rows = 4, cols = 5;

    /* We have 19 completions. We can fit into 6 columns with 4 rows or 5 columns with 4 rows; the second one is better and so is what we ought to have picked. */
    if (render.rows != rows)
        err(L"Wrong row count");
    if (render.cols != cols)
        err(L"Wrong column count");

    /* Initially expect to have no completion index */
    if (render.selected_completion_idx != (size_t)(-1))
    {
        err(L"Wrong initial selection");
    }

    /* Here are navigation directions and where we expect the selection to be */
    const struct
    {
        selection_direction_t dir;
        size_t sel;
    }
    cmds[] =
    {
        /* Tab completion to get into the list */
        {direction_next, 0},

        /* Westward motion in upper left wraps along the top row */
        {direction_west, 16},
        {direction_east, 1},

        /* "Next" motion goes down the column */
        {direction_next, 2},
        {direction_next, 3},

        {direction_west, 18},
        {direction_east, 3},
        {direction_east, 7},
        {direction_east, 11},
        {direction_east, 15},
        {direction_east, 3},

        {direction_west, 18},
        {direction_east, 3},

        /* Eastward motion wraps along the bottom, westward goes to the prior column */
        {direction_east, 7},
        {direction_east, 11},
        {direction_east, 15},
        {direction_east, 3},

        /* Column memory */
        {direction_west, 18},
        {direction_south, 15},
        {direction_north, 18},
        {direction_west, 14},
        {direction_south, 15},
        {direction_north, 14},

        /* pages */
        {direction_page_north, 12},
        {direction_page_south, 15},
        {direction_page_north, 12},
        {direction_east, 16},
        {direction_page_south, 18},
        {direction_east, 3},
        {direction_north, 2},
        {direction_page_north, 0},
        {direction_page_south, 3},

    };
    for (size_t i=0; i < sizeof cmds / sizeof *cmds; i++)
    {
        pager.select_next_completion_in_direction(cmds[i].dir, render);
        pager.update_rendering(&render);
        if (cmds[i].sel != render.selected_completion_idx)
        {
            err(L"For command %lu, expected selection %lu, but found instead %lu\n", i, cmds[i].sel, render.selected_completion_idx);
        }
    }

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

    do_test(is_potential_path(L"al", wds, PATH_REQUIRE_DIR));
    do_test(is_potential_path(L"alpha/", wds, PATH_REQUIRE_DIR));
    do_test(is_potential_path(L"aard", wds, 0));

    do_test(! is_potential_path(L"balpha/", wds, PATH_REQUIRE_DIR));
    do_test(! is_potential_path(L"aard", wds, PATH_REQUIRE_DIR));
    do_test(! is_potential_path(L"aarde", wds, PATH_REQUIRE_DIR));
    do_test(! is_potential_path(L"aarde", wds, 0));

    do_test(is_potential_path(L"/tmp/is_potential_path_test/aardvark", wds, 0));
    do_test(is_potential_path(L"/tmp/is_potential_path_test/al", wds, PATH_REQUIRE_DIR));
    do_test(is_potential_path(L"/tmp/is_potential_path_test/aardv", wds, 0));

    do_test(! is_potential_path(L"/tmp/is_potential_path_test/aardvark", wds, PATH_REQUIRE_DIR));
    do_test(! is_potential_path(L"/tmp/is_potential_path_test/al/", wds, 0));
    do_test(! is_potential_path(L"/tmp/is_potential_path_test/ar", wds, 0));

    do_test(is_potential_path(L"/usr", wds, PATH_REQUIRE_DIR));

}

/** Test the 'test' builtin */
int builtin_test(parser_t &parser, io_streams_t &streams, wchar_t **argv);
static bool run_one_test_test(int expected, wcstring_list_t &lst, bool bracket)
{
    parser_t parser;
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
    io_streams_t streams;
    int result = builtin_test(parser, streams, argv);
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
    do_test(bracket == nonbracket);
    return nonbracket;
}

static void test_test_brackets()
{
    // Ensure [ knows it needs a ]
    parser_t parser;
    io_streams_t streams;

    const wchar_t *argv1[] = {L"[", L"foo", NULL};
    do_test(builtin_test(parser, streams, (wchar_t **)argv1) != 0);

    const wchar_t *argv2[] = {L"[", L"foo", L"]", NULL};
    do_test(builtin_test(parser, streams, (wchar_t **)argv2) == 0);

    const wchar_t *argv3[] = {L"[", L"foo", L"]", L"bar", NULL};
    do_test(builtin_test(parser, streams, (wchar_t **)argv3) != 0);

}

static void test_test()
{
    say(L"Testing test builtin");
    test_test_brackets();

    do_test(run_test_test(0, L"5 -ne 6"));
    do_test(run_test_test(0, L"5 -eq 5"));
    do_test(run_test_test(0, L"0 -eq 0"));
    do_test(run_test_test(0, L"-1 -eq -1"));
    do_test(run_test_test(0, L"1 -ne -1"));
    do_test(run_test_test(1, L"-1 -ne -1"));
    do_test(run_test_test(0, L"abc != def"));
    do_test(run_test_test(1, L"abc = def"));
    do_test(run_test_test(0, L"5 -le 10"));
    do_test(run_test_test(0, L"10 -le 10"));
    do_test(run_test_test(1, L"20 -le 10"));
    do_test(run_test_test(0, L"-1 -le 0"));
    do_test(run_test_test(1, L"0 -le -1"));
    do_test(run_test_test(0, L"15 -ge 10"));
    do_test(run_test_test(0, L"15 -ge 10"));
    do_test(run_test_test(1, L"! 15 -ge 10"));
    do_test(run_test_test(0, L"! ! 15 -ge 10"));

    do_test(run_test_test(0, L"0 -ne 1 -a 0 -eq 0"));
    do_test(run_test_test(0, L"0 -ne 1 -a -n 5"));
    do_test(run_test_test(0, L"-n 5 -a 10 -gt 5"));
    do_test(run_test_test(0, L"-n 3 -a -n 5"));

    /* test precedence:
            '0 == 0 || 0 == 1 && 0 == 2'
        should be evaluated as:
            '0 == 0 || (0 == 1 && 0 == 2)'
        and therefore true. If it were
            '(0 == 0 || 0 == 1) && 0 == 2'
        it would be false. */
    do_test(run_test_test(0, L"0 = 0 -o 0 = 1 -a 0 = 2"));
    do_test(run_test_test(0, L"-n 5 -o 0 = 1 -a 0 = 2"));
    do_test(run_test_test(1, L"( 0 = 0 -o  0 = 1 ) -a 0 = 2"));
    do_test(run_test_test(0, L"0 = 0 -o ( 0 = 1 -a 0 = 2 )"));

    /* A few lame tests for permissions; these need to be a lot more complete. */
    do_test(run_test_test(0, L"-e /bin/ls"));
    do_test(run_test_test(1, L"-e /bin/ls_not_a_path"));
    do_test(run_test_test(0, L"-x /bin/ls"));
    do_test(run_test_test(1, L"-x /bin/ls_not_a_path"));
    do_test(run_test_test(0, L"-d /bin/"));
    do_test(run_test_test(1, L"-d /bin/ls"));

    /* This failed at one point */
    do_test(run_test_test(1, L"-d /bin -a 5 -eq 3"));
    do_test(run_test_test(0, L"-d /bin -o 5 -eq 3"));
    do_test(run_test_test(0, L"-d /bin -a ! 5 -eq 3"));

    /* We didn't properly handle multiple "just strings" either */
    do_test(run_test_test(0, L"foo"));
    do_test(run_test_test(0, L"foo -a bar"));

    /* These should be errors */
    do_test(run_test_test(1, L"foo bar"));
    do_test(run_test_test(1, L"foo bar baz"));

    /* This crashed */
    do_test(run_test_test(1, L"1 = 1 -a = 1"));

    /* Make sure we can treat -S as a parameter instead of an operator. https://github.com/fish-shell/fish-shell/issues/601 */
    do_test(run_test_test(0, L"-S = -S"));
    do_test(run_test_test(1, L"! ! ! A"));
}

/** Testing colors */
static void test_colors()
{
    say(L"Testing colors");
    do_test(rgb_color_t(L"#FF00A0").is_rgb());
    do_test(rgb_color_t(L"FF00A0").is_rgb());
    do_test(rgb_color_t(L"#F30").is_rgb());
    do_test(rgb_color_t(L"F30").is_rgb());
    do_test(rgb_color_t(L"f30").is_rgb());
    do_test(rgb_color_t(L"#FF30a5").is_rgb());
    do_test(rgb_color_t(L"3f30").is_none());
    do_test(rgb_color_t(L"##f30").is_none());
    do_test(rgb_color_t(L"magenta").is_named());
    do_test(rgb_color_t(L"MaGeNTa").is_named());
    do_test(rgb_color_t(L"mooganta").is_none());
}

static void test_complete(void)
{
    say(L"Testing complete");

    const wchar_t *name_strs[] = {L"Foo1", L"Foo2", L"Foo3", L"Bar1", L"Bar2", L"Bar3"};
    size_t count = sizeof name_strs / sizeof *name_strs;
    const wcstring_list_t names(name_strs, name_strs + count);

    complete_set_variable_names(&names);
    
    const env_vars_snapshot_t &vars = env_vars_snapshot_t::current();

    std::vector<completion_t> completions;
    complete(L"$", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    completions_sort_and_prioritize(&completions);
    do_test(completions.size() == 6);
    do_test(completions.at(0).completion == L"Bar1");
    do_test(completions.at(1).completion == L"Bar2");
    do_test(completions.at(2).completion == L"Bar3");
    do_test(completions.at(3).completion == L"Foo1");
    do_test(completions.at(4).completion == L"Foo2");
    do_test(completions.at(5).completion == L"Foo3");

    completions.clear();
    complete(L"$F", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    completions_sort_and_prioritize(&completions);
    do_test(completions.size() == 3);
    do_test(completions.at(0).completion == L"oo1");
    do_test(completions.at(1).completion == L"oo2");
    do_test(completions.at(2).completion == L"oo3");

    completions.clear();
    complete(L"$1", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    completions_sort_and_prioritize(&completions);
    do_test(completions.empty());

    completions.clear();
    complete(L"$1", &completions, COMPLETION_REQUEST_DEFAULT | COMPLETION_REQUEST_FUZZY_MATCH, vars);
    completions_sort_and_prioritize(&completions);
    do_test(completions.size() == 2);
    do_test(completions.at(0).completion == L"$Bar1");
    do_test(completions.at(1).completion == L"$Foo1");

    if (system("mkdir -p '/tmp/complete_test/'")) err(L"mkdir failed");
    if (system("touch '/tmp/complete_test/testfile'")) err(L"touch failed");
    if (system("chmod 700 '/tmp/complete_test/testfile'")) err(L"chmod failed");

    completions.clear();
    complete(L"echo (/tmp/complete_test/testfil", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"e");

    completions.clear();
    complete(L"echo (ls /tmp/complete_test/testfil", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"e");

    completions.clear();
    complete(L"echo (command ls /tmp/complete_test/testfil", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"e");

    /* Add a function and test completing it in various ways */
    struct function_data_t func_data = {};
    func_data.name = L"scuttlebutt";
    func_data.definition = L"echo gongoozle";
    function_add(func_data, parser_t::principal_parser());

    /* Complete a function name */
    completions.clear();
    complete(L"echo (scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"t");

    /* But not with the command prefix */
    completions.clear();
    complete(L"echo (command scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 0);

    /* Not with the builtin prefix */
    completions.clear();
    complete(L"echo (builtin scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 0);

    /* Not after a redirection */
    completions.clear();
    complete(L"echo hi > scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 0);

    /* Trailing spaces (#1261) */
    complete_add(L"foobarbaz", false, wcstring(), option_type_args_only, NO_FILES, NULL, L"qux", NULL, COMPLETE_AUTO_SPACE);
    completions.clear();
    complete(L"foobarbaz ", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"qux");

    /* Don't complete variable names in single quotes (#1023) */
    completions.clear();
    complete(L"echo '$Foo", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.empty());
    completions.clear();
    complete(L"echo \\$Foo", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.empty());

    /* File completions */
    char saved_wd[PATH_MAX + 1] = {};
    if (!getcwd(saved_wd, sizeof saved_wd))
    {
        perror("getcwd");
        exit(-1);
    }
    if (chdir_set_pwd("/tmp/complete_test/")) err(L"chdir failed");
    
    complete(L"cat te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something --abc=te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something -abc=te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something abc=te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something abc=stfile", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 0);
    completions.clear();
    complete(L"something abc=stfile", &completions, COMPLETION_REQUEST_FUZZY_MATCH, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"abc=testfile");
    completions.clear();

    complete(L"cat /tmp/complete_test/te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"echo sup > /tmp/complete_test/te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"echo sup > /tmp/complete_test/te", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();

    // Zero escapes can cause problems. See #1631
    complete(L"cat foo\\0", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.empty());
    completions.clear();
    complete(L"cat foo\\0bar", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.empty());
    completions.clear();
    complete(L"cat \\0", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.empty());
    completions.clear();
    complete(L"cat te\\0", &completions, COMPLETION_REQUEST_DEFAULT, vars);
    do_test(completions.empty());
    completions.clear();

    if (chdir_set_pwd(saved_wd)) err(L"chdir failed");
    if (system("rm -Rf '/tmp/complete_test/'")) err(L"rm failed");

    complete_set_variable_names(NULL);

    /* Test wraps */
    do_test(comma_join(complete_get_wrap_chain(L"wrapper1")) == L"wrapper1");
    complete_add_wrapper(L"wrapper1", L"wrapper2");
    do_test(comma_join(complete_get_wrap_chain(L"wrapper1")) == L"wrapper1,wrapper2");
    complete_add_wrapper(L"wrapper2", L"wrapper3");
    do_test(comma_join(complete_get_wrap_chain(L"wrapper1")) == L"wrapper1,wrapper2,wrapper3");
    complete_add_wrapper(L"wrapper3", L"wrapper1"); //loop!
    do_test(comma_join(complete_get_wrap_chain(L"wrapper1")) == L"wrapper1,wrapper2,wrapper3");
    complete_remove_wrapper(L"wrapper1", L"wrapper2");
    do_test(comma_join(complete_get_wrap_chain(L"wrapper1")) == L"wrapper1");
    do_test(comma_join(complete_get_wrap_chain(L"wrapper2")) == L"wrapper2,wrapper3,wrapper1");
}

static void test_1_completion(wcstring line, const wcstring &completion, complete_flags_t flags, bool append_only, wcstring expected, long source_line)
{
    // str is given with a caret, which we use to represent the cursor position
    // find it
    const size_t in_cursor_pos = line.find(L'^');
    do_test(in_cursor_pos != wcstring::npos);
    line.erase(in_cursor_pos, 1);

    const size_t out_cursor_pos = expected.find(L'^');
    do_test(out_cursor_pos != wcstring::npos);
    expected.erase(out_cursor_pos, 1);

    size_t cursor_pos = in_cursor_pos;
    wcstring result = completion_apply_to_command_line(completion, flags, line, &cursor_pos, append_only);
    if (result != expected)
    {
        fprintf(stderr, "line %ld: %ls + %ls -> [%ls], expected [%ls]\n", source_line, line.c_str(), completion.c_str(), result.c_str(), expected.c_str());
    }
    do_test(result == expected);
    do_test(cursor_pos == out_cursor_pos);
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

static void perform_one_autosuggestion_cd_test(const wcstring &command, const env_vars_snapshot_t &vars, const wcstring &expected, long line)
{
    std::vector<completion_t> comps;
    complete(command, &comps,COMPLETION_REQUEST_AUTOSUGGESTION, vars);
    
    bool expects_error = (expected == L"<error>");
    
    if (comps.empty() && ! expects_error)
    {
        printf("line %ld: autosuggest_suggest_special() failed for command %ls\n", line, command.c_str());
        do_test(! comps.empty());
        return;
    }
    else if (! comps.empty() && expects_error)
    {
        printf("line %ld: autosuggest_suggest_special() was expected to fail but did not, for command %ls\n", line, command.c_str());
        do_test(comps.empty());
    }
    
    if (! comps.empty())
    {
        completions_sort_and_prioritize(&comps);
        const completion_t &suggestion = comps.at(0);
        
        if (suggestion.completion != expected)
        {
            printf("line %ld: complete() for cd returned the wrong expected string for command %ls\n", line, command.c_str());
            printf("  actual: %ls\n", suggestion.completion.c_str());
            printf("expected: %ls\n", expected.c_str());
            do_test(suggestion.completion == expected);
        }
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
    if (system("mkdir -p /tmp/autosuggest_test/start/unique2/unique3/multi4")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/autosuggest_test/start/unique2/unique3/multi42")) err(L"mkdir failed");
    if (system("mkdir -p /tmp/autosuggest_test/start/unique2/.hiddenDir/moreStuff")) err(L"mkdir failed");
    
    char saved_wd[PATH_MAX] = {};
    if (NULL == getcwd(saved_wd, sizeof saved_wd)) err(L"getcwd failed");
    
    const wcstring wd = L"/tmp/autosuggest_test/";
    if (chdir_set_pwd(wcs2string(wd).c_str())) err(L"chdir failed");
    
    env_set(L"AUTOSUGGEST_TEST_LOC", wd.c_str(), ENV_LOCAL);
    
    const env_vars_snapshot_t &vars = env_vars_snapshot_t::current();

    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/0", vars, L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"/tmp/autosuggest_test/0", vars, L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '/tmp/autosuggest_test/0", vars, L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 0", vars, L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"0", vars, L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '0", vars, L"foobar/", __LINE__);

    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/1", vars, L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"/tmp/autosuggest_test/1", vars, L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '/tmp/autosuggest_test/1", vars, L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 1", vars, L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"1", vars, L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '1", vars, L"foo bar/", __LINE__);

    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/2", vars, L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"/tmp/autosuggest_test/2", vars, L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '/tmp/autosuggest_test/2", vars, L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 2", vars, L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"2", vars, L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '2", vars, L"foo  bar/", __LINE__);

    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/3", vars, L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"/tmp/autosuggest_test/3", vars, L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '/tmp/autosuggest_test/3", vars, L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 3", vars, L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"3", vars, L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '3", vars, L"foo\\bar/", __LINE__);

    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/4", vars, L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"/tmp/autosuggest_test/4", vars, L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '/tmp/autosuggest_test/4", vars, L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 4", vars, L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"4", vars, L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '4", vars, L"foo'bar/", __LINE__);

    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/5", vars, L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"/tmp/autosuggest_test/5", vars, L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '/tmp/autosuggest_test/5", vars, L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 5", vars, L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"5", vars, L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '5", vars, L"foo\"bar/", __LINE__);
    
    perform_one_autosuggestion_cd_test(L"cd $AUTOSUGGEST_TEST_LOC/0", vars, L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd ~/test_autosuggest_suggest_specia", vars, L"l/", __LINE__);
    
    perform_one_autosuggestion_cd_test(L"cd /tmp/autosuggest_test/start/", vars, L"unique2/unique3/", __LINE__);

    // A single quote should defeat tilde expansion
    perform_one_autosuggestion_cd_test(L"cd '~/test_autosuggest_suggest_specia'", vars, L"<error>", __LINE__);
    
    // Don't crash on ~ (2696)
    // note this was wd dependent, hence why we set it
    if (chdir_set_pwd("/tmp/autosuggest_test/")) err(L"chdir failed");
    
    if (system("mkdir -p '/tmp/autosuggest_test/~hahaha/path1/path2/'")) err(L"mkdir failed");
    
    perform_one_autosuggestion_cd_test(L"cd ~haha", vars, L"ha/path1/path2/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd ~hahaha/", vars, L"path1/path2/", __LINE__);
    if (chdir_set_pwd(saved_wd)) err(L"chdir failed");

    if (system("rm -Rf '/tmp/autosuggest_test/'")) err(L"rm failed");
    if (system("rm -Rf ~/test_autosuggest_suggest_special/")) err(L"rm failed");
}

static void perform_one_autosuggestion_should_ignore_test(const wcstring &command, const wcstring &wd, long line)
{
    completion_list_t comps;
    complete(command, &comps, COMPLETION_REQUEST_AUTOSUGGESTION, env_vars_snapshot_t::current());
    do_test(comps.empty());
    if (! comps.empty())
    {
        const wcstring &suggestion = comps.front().completion;
        printf("line %ld: complete() expected to return nothing for %ls\n", line, command.c_str());
        printf("  instead got: %ls\n", suggestion.c_str());
    }
}

static void test_autosuggestion_ignores()
{
    say(L"Testing scenarios that should produce no autosuggestions");
    const wcstring wd = L"/tmp/autosuggest_test/";
    // Do not do file autosuggestions immediately after certain statement terminators - see #1631
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST|", wd, __LINE__);
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST&", wd, __LINE__);
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST#comment", wd, __LINE__);
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST;", wd, __LINE__);
}

static void test_autosuggestion_combining()
{
    say(L"Testing autosuggestion combining");
    do_test(combine_command_and_autosuggestion(L"alpha", L"alphabeta") == L"alphabeta");

    // when the last token contains no capital letters, we use the case of the autosuggestion
    do_test(combine_command_and_autosuggestion(L"alpha", L"ALPHABETA") == L"ALPHABETA");

    // when the last token contains capital letters, we use its case
    do_test(combine_command_and_autosuggestion(L"alPha", L"alphabeTa") == L"alPhabeTa");

    // if autosuggestion is not longer than input, use the input's case
    do_test(combine_command_and_autosuggestion(L"alpha", L"ALPHAA") == L"ALPHAA");
    do_test(combine_command_and_autosuggestion(L"alpha", L"ALPHA") == L"alpha");
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

        complete(str, &out, COMPLETION_REQUEST_DEFAULT, env_vars_snapshot_t::current());

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

        complete(str, &out, COMPLETION_REQUEST_DEFAULT, env_vars_snapshot_t::current());

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
        do_test(search.go_backwards());
        wcstring item = search.current_string();
    }
    do_test(! search.go_backwards());

    for (i=1; i < matches; i++)
    {
        do_test(search.go_forwards());
    }
    do_test(! search.go_forwards());
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

static void test_input()
{
    say(L"Testing input");
    /* Ensure sequences are order independent. Here we add two bindings where the first is a prefix of the second, and then emit the second key list. The second binding should be invoked, not the first! */
    wcstring prefix_binding = L"qqqqqqqa";
    wcstring desired_binding = prefix_binding + L'a';
    input_mapping_add(prefix_binding.c_str(), L"up-line");
    input_mapping_add(desired_binding.c_str(), L"down-line");

    /* Push the desired binding to the queue */
    for (size_t idx = 0; idx < desired_binding.size(); idx++) {
        input_queue_ch(desired_binding.at(idx));
    }

    /* Now test */
    wint_t c = input_readch();
    if (c != R_DOWN_LINE)
    {
        err(L"Expected to read char R_DOWN_LINE, but instead got %ls\n", describe_char(c).c_str());
    }
}

#define UVARS_PER_THREAD 8
#define UVARS_TEST_PATH L"/tmp/fish_uvars_test/varsfile.txt"

static int test_universal_helper(int *x)
{
    env_universal_t uvars(UVARS_TEST_PATH);
    for (int j=0; j < UVARS_PER_THREAD; j++)
    {
        const wcstring key = format_string(L"key_%d_%d", *x, j);
        const wcstring val = format_string(L"val_%d_%d", *x, j);
        uvars.set(key, val, false);
        bool synced = uvars.sync(NULL);
        if (! synced)
        {
            err(L"Failed to sync universal variables after modification");
        }
        fputc('.', stderr);
    }

    /* Last step is to delete the first key */
    uvars.remove(format_string(L"key_%d_%d", *x, 0));
    bool synced = uvars.sync(NULL);
    if (! synced)
    {
        err(L"Failed to sync universal variables after deletion");
    }
    fputc('.', stderr);

    return 0;
}

static void test_universal()
{
    say(L"Testing universal variables");
    if (system("mkdir -p /tmp/fish_uvars_test/")) err(L"mkdir failed");

    const int threads = 16;
    static int ctx[threads];
    for (int i=0; i < threads; i++)
    {
        ctx[i] = i;
        iothread_perform(test_universal_helper, &ctx[i]);
    }
    iothread_drain_all();

    env_universal_t uvars(UVARS_TEST_PATH);
    bool loaded = uvars.load();
    if (! loaded)
    {
        err(L"Failed to load universal variables");
    }
    for (int i=0; i < threads; i++)
    {
        for (int j=0; j < UVARS_PER_THREAD; j++)
        {
            const wcstring key = format_string(L"key_%d_%d", i, j);
            env_var_t expected_val;
            if (j == 0)
            {
                expected_val = env_var_t::missing_var();
            }
            else
            {
                expected_val = format_string(L"val_%d_%d", i, j);
            }
            const env_var_t var = uvars.get(key);
            if (j == 0)
            {
                assert(expected_val.missing());
            }
            if (var != expected_val)
            {
                const wchar_t *missing_desc = L"<missing>";
                err(L"Wrong value for key %ls: expected %ls, got %ls\n", key.c_str(), (expected_val.missing() ? missing_desc : expected_val.c_str()), (var.missing() ? missing_desc : var.c_str()));
            }
        }
    }

    if (system("rm -Rf /tmp/fish_uvars_test")) err(L"rm failed");
    putc('\n', stderr);
}

static bool callback_data_less_than(const callback_data_t &a, const callback_data_t &b) {
    return a.key < b.key;
}

static void test_universal_callbacks()
{
    say(L"Testing universal callbacks");
    if (system("mkdir -p /tmp/fish_uvars_test/")) err(L"mkdir failed");
    env_universal_t uvars1(UVARS_TEST_PATH);
    env_universal_t uvars2(UVARS_TEST_PATH);

    /* Put some variables into both */
    uvars1.set(L"alpha", L"1", false);
    uvars1.set(L"beta", L"1", false);
    uvars1.set(L"delta", L"1", false);
    uvars1.set(L"epsilon", L"1", false);
    uvars1.set(L"lambda", L"1", false);
    uvars1.set(L"kappa", L"1", false);
    uvars1.set(L"omicron", L"1", false);

    uvars1.sync(NULL);
    uvars2.sync(NULL);

    /* Change uvars1 */
    uvars1.set(L"alpha", L"2", false); //changes value
    uvars1.set(L"beta", L"1", true); //changes export
    uvars1.remove(L"delta"); //erases value
    uvars1.set(L"epsilon", L"1", false); //changes nothing
    uvars1.sync(NULL);

    /* Change uvars2. It should treat its value as correct and ignore changes from uvars1. */
    uvars2.set(L"lambda", L"1", false); //same value
    uvars2.set(L"kappa", L"2", false); //different value

    /* Now see what uvars2 sees */
    callback_data_list_t callbacks;
    uvars2.sync(&callbacks);

    /* Sort them to get them in a predictable order */
    std::sort(callbacks.begin(), callbacks.end(), callback_data_less_than);

    /* Should see exactly two changes */
    do_test(callbacks.size() == 3);
    do_test(callbacks.at(0).type == SET);
    do_test(callbacks.at(0).key == L"alpha");
    do_test(callbacks.at(0).val == L"2");
    do_test(callbacks.at(1).type == SET_EXPORT);
    do_test(callbacks.at(1).key == L"beta");
    do_test(callbacks.at(1).val == L"1");
    do_test(callbacks.at(2).type == ERASE);
    do_test(callbacks.at(2).key == L"delta");
    do_test(callbacks.at(2).val == L"");


    if (system("rm -Rf /tmp/fish_uvars_test")) err(L"rm failed");
}

bool poll_notifier(universal_notifier_t *note)
{
    bool result = false;
    if (note->usec_delay_between_polls() > 0)
    {
        result = note->poll();
    }

    int fd = note->notification_fd();
    if (! result && fd >= 0)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0, 0};
        if (select(fd + 1, &fds, NULL, NULL, &tv) > 0 && FD_ISSET(fd, &fds))
        {
            result = note->notification_fd_became_readable(fd);
        }
    }
    return result;
}

static void trigger_or_wait_for_notification(universal_notifier_t *notifier, universal_notifier_t::notifier_strategy_t strategy)
{
    switch (strategy)
    {
        case universal_notifier_t::strategy_default:
            assert(0 && "strategy_default should be passed");
            break;

        case universal_notifier_t::strategy_shmem_polling:
            // nothing required
            break;

        case universal_notifier_t::strategy_notifyd:
            // notifyd requires a round trip to the notifyd server, which means we have to wait a little bit to receive it
            // In practice, this seems to be enough
            usleep(1000000 / 25);
            break;

        case universal_notifier_t::strategy_named_pipe:
        case universal_notifier_t::strategy_null:
            break;
    }
}

static void test_notifiers_with_strategy(universal_notifier_t::notifier_strategy_t strategy)
{
    assert(strategy != universal_notifier_t::strategy_default);
    say(L"Testing universal notifiers with strategy %d", (int)strategy);
    universal_notifier_t *notifiers[16];
    size_t notifier_count = sizeof notifiers / sizeof *notifiers;

    // Populate array of notifiers
    for (size_t i=0; i < notifier_count; i++)
    {
        notifiers[i] = universal_notifier_t::new_notifier_for_strategy(strategy, UVARS_TEST_PATH);
    }

    // Nobody should poll yet
    for (size_t i=0; i < notifier_count; i++)
    {
        if (poll_notifier(notifiers[i]))
        {
            err(L"Universal variable notifier polled true before any changes, with strategy %d", (int)strategy);
        }
    }

    // Tweak each notifier. Verify that others see it.
    for (size_t post_idx=0; post_idx < notifier_count; post_idx++)
    {
        notifiers[post_idx]->post_notification();

        // Do special stuff to "trigger" a notification for testing
        trigger_or_wait_for_notification(notifiers[post_idx], strategy);

        for (size_t i=0; i < notifier_count; i++)
        {
            // We aren't concerned with the one who posted
            // Poll from it (to drain it), and then skip it
            if (i == post_idx)
            {
                poll_notifier(notifiers[i]);
                continue;
            }

            if (! poll_notifier(notifiers[i]))
            {
                err(L"Universal variable notifier (%lu) %p polled failed to notice changes, with strategy %d", i, notifiers[i], (int)strategy);
            }
        }

        // Named pipes have special cleanup requirements
        if (strategy == universal_notifier_t::strategy_named_pipe)
        {
            usleep(1000000 / 10); //corresponds to NAMED_PIPE_FLASH_DURATION_USEC
            // Have to clean up the posted one first, so that the others see the pipe become no longer readable
            poll_notifier(notifiers[post_idx]);
            for (size_t i=0; i < notifier_count; i++)
            {
                poll_notifier(notifiers[i]);
            }
        }
    }

    // Nobody should poll now
    for (size_t i=0; i < notifier_count; i++)
    {
        if (poll_notifier(notifiers[i]))
        {
            err(L"Universal variable notifier polled true after all changes, with strategy %d", (int)strategy);
        }
    }

    // Clean up
    for (size_t i=0; i < notifier_count; i++)
    {
        delete notifiers[i];
    }
}

static void test_universal_notifiers()
{
    if (system("mkdir -p /tmp/fish_uvars_test/ && touch /tmp/fish_uvars_test/varsfile.txt")) err(L"mkdir failed");
    test_notifiers_with_strategy(universal_notifier_t::strategy_shmem_polling);
    test_notifiers_with_strategy(universal_notifier_t::strategy_named_pipe);
#if __APPLE__
    test_notifiers_with_strategy(universal_notifier_t::strategy_notifyd);
#endif

    if (system("rm -Rf /tmp/fish_uvars_test/")) err(L"rm failed");
}

class history_tests_t
{
public:
    static void test_history(void);
    static void test_history_merge(void);
    static void test_history_formats(void);
    // static void test_history_speed(void);
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
    do_test(search1.current_string() == L"Alpha");

    /* One item matches "et" */
    history_search_t search2(history, L"et");
    test_history_matches(search2, 1);
    do_test(search2.current_string() == L"Beta");

    /* Test item removal */
    history.remove(L"Alpha");
    history_search_t search3(history, L"Alpha");
    test_history_matches(search3, 0);

    /* Test history escaping and unescaping, yaml, etc. */
    history_item_list_t before, after;
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
        history_item_t item(value, time(NULL));
        item.required_paths = paths;
        before.push_back(item);
        history.add(item);
    }
    history.save();

    /* Read items back in reverse order and ensure they're the same */
    for (i=100; i >= 1; i--)
    {
        history_item_t item = history.item_at_index(i);
        do_test(! item.empty());
        after.push_back(item);
    }
    do_test(before.size() == after.size());
    for (size_t i=0; i < before.size(); i++)
    {
        const history_item_t &bef = before.at(i), &aft = after.at(i);
        do_test(bef.contents == aft.contents);
        do_test(bef.creation_timestamp == aft.creation_timestamp);
        do_test(bef.required_paths == aft.required_paths);
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
    do_test(hist_idx >= RACE_COUNT);

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
    const wcstring texts[count] = {L"History 1", L"History 2", L"History 3"};
    const wcstring alt_texts[count] = {L"History Alt 1", L"History Alt 2", L"History Alt 3"};

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
            do_test(should_contain == does_contain);
        }
    }

    /* Make a new history. It should contain everything. The time_barrier() is so that the timestamp is newer, since we only pick up items whose timestamp is before the birth stamp. */
    time_barrier();
    history_t *everything = new history_t(name);
    for (size_t i=0; i < count; i++)
    {
        do_test(history_contains(everything, texts[i]));
    }

    /* Tell all histories to merge. Now everybody should have everything. */
    for (size_t i=0; i < count; i++)
    {
        hists[i]->incorporate_external_changes();
    }
    /* Add some more per-history items */
    for (size_t i=0; i < count; i++)
    {
        hists[i]->add(alt_texts[i]);
    }
    /* Everybody should have old items, but only one history should have each new item */
    for (size_t i = 0; i < count; i++)
    {
        for (size_t j=0; j < count; j++)
        {
            /* Old item */
            do_test(history_contains(hists[i], texts[j]));

            /* New item */
            bool does_contain = history_contains(hists[i], alt_texts[j]);
            bool should_contain = (i == j);
            do_test(should_contain == does_contain);
        }
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
    wcstring path;
    if (! path_get_data(path)) {
        err(L"Failed to get data directory");
        return false;
    }
    char command[512];
    snprintf(command, sizeof command, "cp tests/%ls %ls/%ls_history", name, path.c_str(), name);
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

    name = L"history_sample_corrupt1";
    say(L"Testing %ls", name);
    if (! install_sample_history(name))
    {
        err(L"Couldn't open file tests/%ls", name);
    }
    else
    {
        /* We simply invoke get_string_representation. If we don't die, the test is a success. */
        history_t &test_history = history_t::history_with_name(name);
        const wchar_t *expected[] =
        {
            L"no_newline_at_end_of_file",

            L"corrupt_prefix",

            L"this_command_is_ok",

            NULL
        };
        if (! history_equals(test_history, expected))
        {
            err(L"test_history_formats failed for %ls\n", name);
        }
        test_history.clear();
    }
}

#if 0
// This test isn't run at this time. It was added by commit b9283d48 but not actually enabled.
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
#endif

static void test_new_parser_correctness(void)
{
    say(L"Testing new parser!");
    const struct parser_test_t
    {
        const wchar_t *src;
        bool ok;
    }
    parser_tests[] =
    {
        {L"; ; ; ", true},
        {L"if ; end", false},
        {L"if true ; end", true},
        {L"if true; end ; end", false},
        {L"if end; end ; end", false},
        {L"if end", false},
        {L"end", false},
        {L"for i i", false},
        {L"for i in a b c ; end", true},
        {L"begin end", true},
        {L"begin; end", true},
        {L"begin if true; end; end;", true},
        {L"begin if true ; echo hi ; end; end", true},
    };

    for (size_t i=0; i < sizeof parser_tests / sizeof *parser_tests; i++)
    {
        const parser_test_t *test = &parser_tests[i];

        parse_node_tree_t parse_tree;
        bool success = parse_tree_from_string(test->src, parse_flag_none, &parse_tree, NULL);
        say(L"%lu / %lu: Parse \"%ls\": %s", i+1, sizeof parser_tests / sizeof *parser_tests, test->src, success ? "yes" : "no");
        if (success && ! test->ok)
        {
            err(L"\"%ls\" should NOT have parsed, but did", test->src);
        }
        else if (! success && test->ok)
        {
            err(L"\"%ls\" should have parsed, but failed", test->src);
        }
    }
    say(L"Parse tests complete");
}

/* Given that we have an array of 'fuzz_count' strings, we wish to enumerate all permutations of 'len' values. We do this by incrementing an integer, interpreting it as "base fuzz_count". */
static inline bool string_for_permutation(const wcstring *fuzzes, size_t fuzz_count, size_t len, size_t permutation, wcstring *out_str)
{
    out_str->clear();

    size_t remaining_permutation = permutation;
    for (size_t i=0; i < len; i++)
    {
        size_t idx = remaining_permutation % fuzz_count;
        remaining_permutation /= fuzz_count;

        out_str->append(fuzzes[idx]);
        out_str->push_back(L' ');
    }
    // Return false if we wrapped
    return remaining_permutation == 0;
}

static void test_new_parser_fuzzing(void)
{
    say(L"Fuzzing parser (node size: %lu)", sizeof(parse_node_t));
    const wcstring fuzzes[] =
    {
        L"if",
        L"else",
        L"for",
        L"in",
        L"while",
        L"begin",
        L"function",
        L"switch",
        L"case",
        L"end",
        L"and",
        L"or",
        L"not",
        L"command",
        L"builtin",
        L"foo",
        L"|",
        L"^",
        L"&",
        L";",
    };

    /* Generate a list of strings of all keyword / token combinations. */
    wcstring src;
    src.reserve(128);

    parse_node_tree_t node_tree;
    parse_error_list_t errors;

    double start = timef();
    bool log_it = true;
    unsigned long max_len = 5;
    for (unsigned long len = 0; len < max_len; len++)
    {
        if (log_it)
            fprintf(stderr, "%lu / %lu...", len, max_len);

        /* We wish to look at all permutations of 4 elements of 'fuzzes' (with replacement). Construct an int and keep incrementing it. */
        unsigned long permutation = 0;
        while (string_for_permutation(fuzzes, sizeof fuzzes / sizeof *fuzzes, len, permutation++, &src))
        {
            parse_tree_from_string(src, parse_flag_continue_after_error, &node_tree, &errors);
        }
        if (log_it)
            fprintf(stderr, "done (%lu)\n", permutation);

    }
    double end = timef();
    if (log_it)
        say(L"All fuzzed in %f seconds!", end - start);
}

// Parse a statement, returning the command, args (joined by spaces), and the decoration. Returns true if successful.
static bool test_1_parse_ll2(const wcstring &src, wcstring *out_cmd, wcstring *out_joined_args, enum parse_statement_decoration_t *out_deco)
{
    out_cmd->clear();
    out_joined_args->clear();
    *out_deco = parse_statement_decoration_none;

    bool result = false;
    parse_node_tree_t tree;
    if (parse_tree_from_string(src, parse_flag_none, &tree, NULL))
    {
        /* Get the statement. Should only have one */
        const parse_node_tree_t::parse_node_list_t stmt_nodes = tree.find_nodes(tree.at(0), symbol_plain_statement);
        if (stmt_nodes.size() != 1)
        {
            say(L"Unexpected number of statements (%lu) found in '%ls'", stmt_nodes.size(), src.c_str());
            return false;
        }
        const parse_node_t &stmt = *stmt_nodes.at(0);

        /* Return its decoration */
        *out_deco = tree.decoration_for_plain_statement(stmt);

        /* Return its command */
        tree.command_for_plain_statement(stmt, src, out_cmd);

        /* Return arguments separated by spaces */
        const parse_node_tree_t::parse_node_list_t arg_nodes = tree.find_nodes(stmt, symbol_argument);
        for (size_t i=0; i < arg_nodes.size(); i++)
        {
            if (i > 0) out_joined_args->push_back(L' ');
            out_joined_args->append(arg_nodes.at(i)->get_source(src));
        }
        result = true;
    }
    return result;
}

/* Test the LL2 (two token lookahead) nature of the parser by exercising the special builtin and command handling. In particular, 'command foo' should be a decorated statement 'foo' but 'command --help' should be an undecorated statement 'command' with argument '--help', and NOT attempt to run a command called '--help' */
static void test_new_parser_ll2(void)
{
    say(L"Testing parser two-token lookahead");

    const struct
    {
        wcstring src;
        wcstring cmd;
        wcstring args;
        enum parse_statement_decoration_t deco;
    } tests[] =
    {
        {L"echo hello", L"echo", L"hello", parse_statement_decoration_none},
        {L"command echo hello", L"echo", L"hello", parse_statement_decoration_command},
        {L"exec echo hello", L"echo", L"hello", parse_statement_decoration_exec},
        {L"command command hello", L"command", L"hello", parse_statement_decoration_command},
        {L"builtin command hello", L"command", L"hello", parse_statement_decoration_builtin},
        {L"command --help", L"command", L"--help", parse_statement_decoration_none},
        {L"command -h", L"command", L"-h", parse_statement_decoration_none},
        {L"command", L"command", L"", parse_statement_decoration_none},
        {L"command -", L"command", L"-", parse_statement_decoration_none},
        {L"command --", L"command", L"--", parse_statement_decoration_none},
        {L"builtin --names", L"builtin", L"--names", parse_statement_decoration_none},
        {L"function", L"function", L"", parse_statement_decoration_none},
        {L"function --help", L"function", L"--help", parse_statement_decoration_none}
    };

    for (size_t i=0; i < sizeof tests / sizeof *tests; i++)
    {
        wcstring cmd, args;
        enum parse_statement_decoration_t deco = parse_statement_decoration_none;
        bool success = test_1_parse_ll2(tests[i].src, &cmd, &args, &deco);
        if (! success)
            err(L"Parse of '%ls' failed on line %ld", tests[i].cmd.c_str(), (long)__LINE__);
        if (cmd != tests[i].cmd)
            err(L"When parsing '%ls', expected command '%ls' but got '%ls' on line %ld", tests[i].src.c_str(), tests[i].cmd.c_str(), cmd.c_str(), (long)__LINE__);
        if (args != tests[i].args)
            err(L"When parsing '%ls', expected args '%ls' but got '%ls' on line %ld", tests[i].src.c_str(), tests[i].args.c_str(), args.c_str(), (long)__LINE__);
        if (deco != tests[i].deco)
            err(L"When parsing '%ls', expected decoration %d but got %d on line %ld", tests[i].src.c_str(), (int)tests[i].deco, (int)deco, (long)__LINE__);
    }

    /* Verify that 'function -h' and 'function --help' are plain statements but 'function --foo' is not (#1240) */
    const struct
    {
        wcstring src;
        parse_token_type_t type;
    }
    tests2[] =
    {
        {L"function -h", symbol_plain_statement},
        {L"function --help", symbol_plain_statement},
        {L"function --foo ; end", symbol_function_header},
        {L"function foo ; end", symbol_function_header},
    };
    for (size_t i=0; i < sizeof tests2 / sizeof *tests2; i++)
    {
        parse_node_tree_t tree;
        if (! parse_tree_from_string(tests2[i].src, parse_flag_none, &tree, NULL))
        {
            err(L"Failed to parse '%ls'", tests2[i].src.c_str());
        }

        const parse_node_tree_t::parse_node_list_t node_list = tree.find_nodes(tree.at(0), tests2[i].type);
        if (node_list.size() == 0)
        {
            err(L"Failed to find node of type '%ls'", token_type_description(tests2[i].type));
        }
        else if (node_list.size() > 1)
        {
            err(L"Found too many nodes of type '%ls'", token_type_description(tests2[i].type));
        }
    }
}

static void test_new_parser_ad_hoc()
{
    /* Very ad-hoc tests for issues encountered */
    say(L"Testing new parser ad hoc tests");

    /* Ensure that 'case' terminates a job list */
    const wcstring src = L"switch foo ; case bar; case baz; end";
    parse_node_tree_t parse_tree;
    bool success = parse_tree_from_string(src, parse_flag_none, &parse_tree, NULL);
    if (! success)
    {
        err(L"Parsing failed");
    }

    /* Expect three case_item_lists: one for each case, and a terminal one. The bug was that we'd try to run a command 'case' */
    const parse_node_t &root = parse_tree.at(0);
    const parse_node_tree_t::parse_node_list_t node_list = parse_tree.find_nodes(root, symbol_case_item_list);
    if (node_list.size() != 3)
    {
        err(L"Expected 3 case item nodes, found %lu", node_list.size());
    }
}

static void test_new_parser_errors(void)
{
    say(L"Testing new parser error reporting");
    const struct
    {
        const wchar_t *src;
        parse_error_code_t code;
    }
    tests[] =
    {
        {L"echo 'abc", parse_error_tokenizer_unterminated_quote},
        {L"'", parse_error_tokenizer_unterminated_quote},
        {L"echo (abc", parse_error_tokenizer_unterminated_subshell},

        {L"end", parse_error_unbalancing_end},
        {L"echo hi ; end", parse_error_unbalancing_end},

        {L"else", parse_error_unbalancing_else},
        {L"if true ; end ; else", parse_error_unbalancing_else},

        {L"case", parse_error_unbalancing_case},
        {L"if true ; case ; end", parse_error_unbalancing_case},

        {L"foo || bar", parse_error_double_pipe},
        {L"foo && bar", parse_error_double_background},
    };

    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++)
    {
        const wcstring src = tests[i].src;
        parse_error_code_t expected_code = tests[i].code;

        parse_error_list_t errors;
        parse_node_tree_t parse_tree;
        bool success = parse_tree_from_string(src, parse_flag_none, &parse_tree, &errors);
        if (success)
        {
            err(L"Source '%ls' was expected to fail to parse, but succeeded", src.c_str());
        }

        if (errors.size() != 1)
        {
            err(L"Source '%ls' was expected to produce 1 error, but instead produced %lu errors", src.c_str(), errors.size());
        }
        else if (errors.at(0).code != expected_code)
        {
            err(L"Source '%ls' was expected to produce error code %lu, but instead produced error code %lu", src.c_str(), expected_code, (unsigned long)errors.at(0).code);
            for (size_t i=0; i < errors.size(); i++)
            {
                err(L"\t\t%ls", errors.at(i).describe(src).c_str());
            }
        }

    }
}

/* Given a format string, returns a list of non-empty strings separated by format specifiers. The format specifiers themselves are omitted. */
static wcstring_list_t separate_by_format_specifiers(const wchar_t *format)
{
    wcstring_list_t result;
    const wchar_t *cursor = format;
    const wchar_t *end = format + wcslen(format);
    while (cursor < end)
    {
        const wchar_t *next_specifier = wcschr(cursor, '%');
        if (next_specifier == NULL)
        {
            next_specifier = end;
        }
        assert(next_specifier != NULL);

        /* Don't return empty strings */
        if (next_specifier > cursor)
        {
            result.push_back(wcstring(cursor, next_specifier - cursor));
        }

        /* Walk over the format specifier (if any) */
        cursor = next_specifier;
        if (*cursor == '%')
        {
            cursor++;
            // Flag
            if (wcschr(L"#0- +'", *cursor))
                cursor++;
            // Minimum field width
            while (iswdigit(*cursor))
                cursor++;
            // Precision
            if (*cursor == L'.')
            {
                cursor++;
                while (iswdigit(*cursor))
                    cursor++;
            }
            // Length modifier
            if (! wcsncmp(cursor, L"ll", 2) || ! wcsncmp(cursor, L"hh", 2))
            {
                cursor += 2;
            }
            else if (wcschr(L"hljtzqL", *cursor))
            {
                cursor++;
            }
            // The format specifier itself. We allow any character except NUL
            if (*cursor != L'\0')
            {
                cursor += 1;
            }
            assert(cursor <= end);
        }
    }
    return result;
}

/* Given a format string 'format', return true if the string may have been produced by that format string. We do this by splitting the format string around the format specifiers, and then ensuring that each of the remaining chunks is found (in order) in the string. */
static bool string_matches_format(const wcstring &string, const wchar_t *format)
{
    bool result = true;
    wcstring_list_t components = separate_by_format_specifiers(format);
    size_t idx = 0;
    for (size_t i=0; i < components.size(); i++)
    {
        const wcstring &component = components.at(i);
        size_t where = string.find(component, idx);
        if (where == wcstring::npos)
        {
            result = false;
            break;
        }
        idx = where + component.size();
        assert(idx <= string.size());
    }
    return result;
}

static void test_error_messages()
{
    say(L"Testing error messages");
    const struct error_test_t
    {
        const wchar_t *src;
        const wchar_t *error_text_format;
    }
    error_tests[] =
    {
        {L"echo $^", ERROR_BAD_VAR_CHAR1},
        {L"echo foo${a}bar", ERROR_BRACKETED_VARIABLE1},
        {L"echo foo\"${a}\"bar", ERROR_BRACKETED_VARIABLE_QUOTED1},
        {L"echo foo\"${\"bar", ERROR_BAD_VAR_CHAR1},
        {L"echo $?", ERROR_NOT_STATUS},
        {L"echo $$", ERROR_NOT_PID},
        {L"echo $#", ERROR_NOT_ARGV_COUNT},
        {L"echo $@", ERROR_NOT_ARGV_AT},
        {L"echo $*", ERROR_NOT_ARGV_STAR},
        {L"echo $", ERROR_NO_VAR_NAME},
        {L"echo foo\"$\"bar", ERROR_NO_VAR_NAME},
        {L"echo \"foo\"$\"bar\"", ERROR_NO_VAR_NAME},
        {L"echo foo $ bar", ERROR_NO_VAR_NAME},
        {L"echo foo$(foo)bar", ERROR_BAD_VAR_SUBCOMMAND1},
        {L"echo \"foo$(foo)bar\"", ERROR_BAD_VAR_SUBCOMMAND1},
        {L"echo foo || echo bar", ERROR_BAD_OR},
        {L"echo foo && echo bar", ERROR_BAD_AND}
    };

    parse_error_list_t errors;
    for (size_t i=0; i < sizeof error_tests / sizeof *error_tests; i++)
    {
        const struct error_test_t *test = &error_tests[i];
        errors.clear();
        parse_util_detect_errors(test->src, &errors, false /* allow_incomplete */);
        do_test(! errors.empty());
        if (! errors.empty())
        {
            do_test1(string_matches_format(errors.at(0).text, test->error_text_format), test->src);
        }
    }
}

static void test_highlighting(void)
{
    say(L"Testing syntax highlighting");
    if (system("mkdir -p /tmp/fish_highlight_test/")) err(L"mkdir failed");
    if (system("touch /tmp/fish_highlight_test/foo")) err(L"touch failed");
    if (system("touch /tmp/fish_highlight_test/bar")) err(L"touch failed");

    // Here are the components of our source and the colors we expect those to be
    struct highlight_component_t
    {
        const wchar_t *txt;
        int color;
    };

    const highlight_component_t components1[] =
    {
        {L"echo", highlight_spec_command},
        {L"/tmp/fish_highlight_test/foo", highlight_spec_param | highlight_modifier_valid_path},
        {L"&", highlight_spec_statement_terminator},
        {NULL, -1}
    };

    const highlight_component_t components2[] =
    {
        {L"command", highlight_spec_command},
        {L"echo", highlight_spec_command},
        {L"abc", highlight_spec_param},
        {L"/tmp/fish_highlight_test/foo", highlight_spec_param | highlight_modifier_valid_path},
        {L"&", highlight_spec_statement_terminator},
        {NULL, -1}
    };

    const highlight_component_t components3[] =
    {
        {L"if command ls", highlight_spec_command},
        {L"; ", highlight_spec_statement_terminator},
        {L"echo", highlight_spec_command},
        {L"abc", highlight_spec_param},
        {L"; ", highlight_spec_statement_terminator},
        {L"/bin/definitely_not_a_command", highlight_spec_error},
        {L"; ", highlight_spec_statement_terminator},
        {L"end", highlight_spec_command},
        {NULL, -1}
    };

    /* Verify that cd shows errors for non-directories */
    const highlight_component_t components4[] =
    {
        {L"cd", highlight_spec_command},
        {L"/tmp/fish_highlight_test", highlight_spec_param | highlight_modifier_valid_path},
        {NULL, -1}
    };

    const highlight_component_t components5[] =
    {
        {L"cd", highlight_spec_command},
        {L"/tmp/fish_highlight_test/foo", highlight_spec_error},
        {NULL, -1}
    };

    const highlight_component_t components6[] =
    {
        {L"cd", highlight_spec_command},
        {L"--help", highlight_spec_param},
        {L"-h", highlight_spec_param},
        {L"definitely_not_a_directory", highlight_spec_error},
        {NULL, -1}
    };

    // Command substitutions
    const highlight_component_t components7[] =
    {
        {L"echo", highlight_spec_command},
        {L"param1", highlight_spec_param},
        {L"(", highlight_spec_operator},
        {L"ls", highlight_spec_command},
        {L"param2", highlight_spec_param},
        {L")", highlight_spec_operator},
        {L"|", highlight_spec_statement_terminator},
        {L"cat", highlight_spec_command},
        {NULL, -1}
    };

    // Redirections substitutions
    const highlight_component_t components8[] =
    {
        {L"echo", highlight_spec_command},
        {L"param1", highlight_spec_param},

        /* Input redirection */
        {L"<", highlight_spec_redirection},
        {L"/bin/echo", highlight_spec_redirection},

        /* Output redirection to a valid fd */
        {L"1>&2", highlight_spec_redirection},

        /* Output redirection to an invalid fd */
        {L"2>&", highlight_spec_redirection},
        {L"LOL", highlight_spec_error},

        /* Just a param, not a redirection */
        {L"/tmp/blah", highlight_spec_param},

        /* Input redirection from directory */
        {L"<", highlight_spec_redirection},
        {L"/tmp/", highlight_spec_error},

        /* Output redirection to an invalid path */
        {L"3>", highlight_spec_redirection},
        {L"/not/a/valid/path/nope", highlight_spec_error},

        /* Output redirection to directory */
        {L"3>", highlight_spec_redirection},
        {L"/tmp/nope/", highlight_spec_error},


        /* Redirections to overflow fd */
        {L"99999999999999999999>&2", highlight_spec_error},
        {L"2>&", highlight_spec_redirection},
        {L"99999999999999999999", highlight_spec_error},

        /* Output redirection containing a command substitution */
        {L"4>", highlight_spec_redirection},
        {L"(", highlight_spec_operator},
        {L"echo", highlight_spec_command},
        {L"/tmp/somewhere", highlight_spec_param},
        {L")", highlight_spec_operator},

        /* Just another param */
        {L"param2", highlight_spec_param},
        {NULL, -1}
    };

    const highlight_component_t components9[] =
    {
        {L"end", highlight_spec_error},
        {L";", highlight_spec_statement_terminator},
        {L"if", highlight_spec_command},
        {L"end", highlight_spec_error},
        {NULL, -1}
    };

    const highlight_component_t components10[] =
    {
        {L"echo", highlight_spec_command},
        {L"'single_quote", highlight_spec_error},
        {NULL, -1}
    };

    const highlight_component_t components11[] =
    {
        {L"echo", highlight_spec_command},
        {L"$foo", highlight_spec_operator},
        {L"\"", highlight_spec_quote},
        {L"$bar", highlight_spec_operator},
        {L"\"", highlight_spec_quote},
        {L"$baz[", highlight_spec_operator},
        {L"1 2..3", highlight_spec_param},
        {L"]", highlight_spec_operator},
        {NULL, -1}
    };

    const highlight_component_t components12[] =
    {
        {L"for", highlight_spec_command},
        {L"i", highlight_spec_param},
        {L"in", highlight_spec_command},
        {L"1 2 3", highlight_spec_param},
        {L";", highlight_spec_statement_terminator},
        {L"end", highlight_spec_command},
        {NULL, -1}
    };

    const highlight_component_t components13[] =
    {
        {L"echo", highlight_spec_command},
        {L"$$foo[", highlight_spec_operator},
        {L"1", highlight_spec_param},
        {L"][", highlight_spec_operator},
        {L"2", highlight_spec_param},
        {L"]", highlight_spec_operator},
        {L"[3]", highlight_spec_param}, // two dollar signs, so last one is not an expansion
        {NULL, -1}
    };

    const highlight_component_t *tests[] = {components1, components2, components3, components4, components5, components6, components7, components8, components9, components10, components11, components12, components13};
    for (size_t which = 0; which < sizeof tests / sizeof *tests; which++)
    {
        const highlight_component_t *components = tests[which];
        // Count how many we have
        size_t component_count = 0;
        while (components[component_count].txt != NULL)
        {
            component_count++;
        }

        // Generate the text
        wcstring text;
        std::vector<highlight_spec_t> expected_colors;
        for (size_t i=0; i < component_count; i++)
        {
            if (i > 0)
            {
                text.push_back(L' ');
                expected_colors.push_back(0);
            }
            text.append(components[i].txt);
            expected_colors.resize(text.size(), components[i].color);
        }
        do_test(expected_colors.size() == text.size());

        std::vector<highlight_spec_t> colors(text.size());
        highlight_shell(text, colors, 20, NULL, env_vars_snapshot_t());

        if (expected_colors.size() != colors.size())
        {
            err(L"Color vector has wrong size! Expected %lu, actual %lu", expected_colors.size(), colors.size());
        }
        do_test(expected_colors.size() == colors.size());
        for (size_t i=0; i < text.size(); i++)
        {
            // Hackish space handling. We don't care about the colors in spaces.
            if (text.at(i) == L' ')
                continue;

            if (expected_colors.at(i) != colors.at(i))
            {
                const wcstring spaces(i, L' ');
                err(L"Wrong color at index %lu in text (expected %#x, actual %#x):\n%ls\n%ls^", i, expected_colors.at(i), colors.at(i), text.c_str(), spaces.c_str());
            }
        }
    }

    if (system("rm -Rf /tmp/fish_highlight_test"))
    {
        err(L"rm failed");
    }
}

static void test_wcstring_tok(void)
{
    say(L"Testing wcstring_tok");
    wcstring buff = L"hello world";
    wcstring needle = L" \t\n";
    wcstring_range loc = wcstring_tok(buff, needle);
    if (loc.first == wcstring::npos || buff.substr(loc.first, loc.second) != L"hello")
    {
        err(L"Wrong results from first wcstring_tok(): {%zu, %zu}", loc.first, loc.second);
    }
    loc = wcstring_tok(buff, needle, loc);
    if (loc.first == wcstring::npos || buff.substr(loc.first, loc.second) != L"world")
    {
        err(L"Wrong results from second wcstring_tok(): {%zu, %zu}", loc.first, loc.second);
    }
    loc = wcstring_tok(buff, needle, loc);
    if (loc.first != wcstring::npos)
    {
        err(L"Wrong results from third wcstring_tok(): {%zu, %zu}", loc.first, loc.second);
    }

    buff = L"hello world";
    loc = wcstring_tok(buff, needle);
    // loc is "hello" again
    loc = wcstring_tok(buff, L"", loc);
    if (loc.first == wcstring::npos || buff.substr(loc.first, loc.second) != L"world")
    {
        err(L"Wrong results from wcstring_tok with empty needle: {%zu, %zu}", loc.first, loc.second);
    }
}

int builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv);
static void run_one_string_test(const wchar_t **argv, int expected_rc, const wchar_t *expected_out)
{
    parser_t parser;
    io_streams_t streams;
    streams.stdin_is_directly_redirected = false; // read from argv instead of stdin
    int rc = builtin_string(parser, streams, const_cast<wchar_t**>(argv));
    wcstring args;
    for (int i = 0; argv[i] != 0; i++)
    {
        args += escape_string(argv[i], ESCAPE_ALL) + L' ';
    }
    args.resize(args.size() - 1);
    if (rc != expected_rc)
    {
        err(L"Test failed on line %lu: [%ls]: expected return code %d but got %d",
                __LINE__, args.c_str(), expected_rc, rc);
    }
    else if (streams.out.buffer() != expected_out)
    {
        err(L"Test failed on line %lu: [%ls]: expected [%ls] but got [%ls]",
                __LINE__, args.c_str(),
                escape_string(expected_out, ESCAPE_ALL).c_str(),
                escape_string(streams.out.buffer(), ESCAPE_ALL).c_str());
    }
}

static void test_string(void)
{
    static struct string_test
    {
        const wchar_t *argv[15];
        int expected_rc;
        const wchar_t *expected_out;
    }
    string_tests[] =
    {
        { {L"string", L"escape", 0},                                1, L"" },
        { {L"string", L"escape", L"", 0},                           0, L"''\n" },
        { {L"string", L"escape", L"-n", L"", 0},                    0, L"\n" },
        { {L"string", L"escape", L"a", 0},                          0, L"a\n" },
        { {L"string", L"escape", L"\x07", 0},                       0, L"\\cg\n" },
        { {L"string", L"escape", L"\"x\"", 0},                      0, L"'\"x\"'\n" },
        { {L"string", L"escape", L"hello world", 0},                0, L"'hello world'\n" },
        { {L"string", L"escape", L"-n", L"hello world", 0},         0, L"hello\\ world\n" },
        { {L"string", L"escape", L"hello", L"world", 0},            0, L"hello\nworld\n" },
        { {L"string", L"escape", L"-n", L"~", 0},                   0, L"\\~\n" },

        { {L"string", L"join", 0},                                  2, L"" },
        { {L"string", L"join", L"", 0},                             1, L"" },
        { {L"string", L"join", L"", L"", L"", L"", 0},              0, L"\n" },
        { {L"string", L"join", L"", L"a", L"b", L"c", 0},           0, L"abc\n" },
        { {L"string", L"join", L".", L"fishshell", L"com", 0},      0, L"fishshell.com\n" },
        { {L"string", L"join", L"/", L"usr", 0},                    1, L"usr\n" },
        { {L"string", L"join", L"/", L"usr", L"local", L"bin", 0},  0, L"usr/local/bin\n" },
        { {L"string", L"join", L"...", L"3", L"2", L"1", 0},        0, L"3...2...1\n" },
        { {L"string", L"join", L"-q", 0},                           2, L"" },
        { {L"string", L"join", L"-q", L".", 0},                     1, L"" },
        { {L"string", L"join", L"-q", L".", L".", 0},               1, L"" },

        { {L"string", L"length", 0},                                1, L"" },
        { {L"string", L"length", L"", 0},                           1, L"0\n" },
        { {L"string", L"length", L"", L"", L"", 0},                 1, L"0\n0\n0\n" },
        { {L"string", L"length", L"a", 0},                          0, L"1\n" },
        { {L"string", L"length", L"\U0002008A", 0},                 0, L"1\n" },
        { {L"string", L"length", L"um", L"dois", L"três", 0},       0, L"2\n4\n4\n" },
        { {L"string", L"length", L"um", L"dois", L"três", 0},       0, L"2\n4\n4\n" },
        { {L"string", L"length", L"-q", 0},                         1, L"" },
        { {L"string", L"length", L"-q", L"", 0},                    1, L"" },
        { {L"string", L"length", L"-q", L"a", 0},                   0, L"" },

        { {L"string", L"match", 0},                                         2, L"" },
        { {L"string", L"match", L"", 0},                                    1, L"" },
        { {L"string", L"match", L"", L"", 0},                               0, L"\n" },
        { {L"string", L"match", L"?", L"a", 0},                             0, L"a\n" },
        { {L"string", L"match", L"*", L"", 0},                              0, L"\n" },
        { {L"string", L"match", L"**", L"", 0},                             0, L"\n" },
        { {L"string", L"match", L"*", L"xyzzy", 0},                         0, L"xyzzy\n" },
        { {L"string", L"match", L"**", L"plugh", 0},                        0, L"plugh\n" },
        { {L"string", L"match", L"a*b", L"axxb", 0},                        0, L"axxb\n" },
        { {L"string", L"match", L"a??b", L"axxb", 0},                       0, L"axxb\n" },
        { {L"string", L"match", L"-i", L"a??B", L"axxb", 0},                0, L"axxb\n" },
        { {L"string", L"match", L"-i", L"a??b", L"Axxb", 0},                0, L"Axxb\n" },
        { {L"string", L"match", L"a*", L"axxb", 0},                         0, L"axxb\n" },
        { {L"string", L"match", L"*a", L"xxa", 0},                          0, L"xxa\n" },
        { {L"string", L"match", L"*a*", L"axa", 0},                         0, L"axa\n" },
        { {L"string", L"match", L"*a*", L"xax", 0},                         0, L"xax\n" },
        { {L"string", L"match", L"*a*", L"bxa", 0},                         0, L"bxa\n" },
        { {L"string", L"match", L"*a", L"a", 0},                            0, L"a\n" },
        { {L"string", L"match", L"a*", L"a", 0},                            0, L"a\n" },
        { {L"string", L"match", L"a*b*c", L"axxbyyc", 0},                   0, L"axxbyyc\n" },
        { {L"string", L"match", L"a*b?c", L"axxbyc", 0},                    0, L"axxbyc\n" },
        { {L"string", L"match", L"*?", L"a", 0},                            0, L"a\n" },
        { {L"string", L"match", L"*?", L"ab", 0},                           0, L"ab\n" },
        { {L"string", L"match", L"?*", L"a", 0},                            0, L"a\n" },
        { {L"string", L"match", L"?*", L"ab", 0},                           0, L"ab\n" },
        { {L"string", L"match", L"\\*", L"*", 0},                           0, L"*\n" },
        { {L"string", L"match", L"a*\\", L"abc\\", 0},                      0, L"abc\\\n" },
        { {L"string", L"match", L"a*\\?", L"abc?", 0},                      0, L"abc?\n" },

        { {L"string", L"match", L"?", L"", 0},                              1, L"" },
        { {L"string", L"match", L"?", L"ab", 0},                            1, L"" },
        { {L"string", L"match", L"??", L"a", 0},                            1, L"" },
        { {L"string", L"match", L"?a", L"a", 0},                            1, L"" },
        { {L"string", L"match", L"a?", L"a", 0},                            1, L"" },
        { {L"string", L"match", L"a??B", L"axxb", 0},                       1, L"" },
        { {L"string", L"match", L"a*b", L"axxbc", 0},                       1, L"" },
        { {L"string", L"match", L"*b", L"bbba", 0},                         1, L"" },
        { {L"string", L"match", L"0x[0-9a-fA-F][0-9a-fA-F]", L"0xbad", 0},  1, L"" },

        { {L"string", L"match", L"-a", L"*", L"ab", L"cde", 0},             0, L"ab\ncde\n" },
        { {L"string", L"match", L"*", L"ab", L"cde", 0},                    0, L"ab\ncde\n" },
        { {L"string", L"match", L"-n", L"*d*", L"cde", 0},                  0, L"1 3\n" },
        { {L"string", L"match", L"-n", L"*x*", L"cde", 0},                  1, L"" },
        { {L"string", L"match", L"-q", L"a*", L"b", L"c", 0},               1, L"" },
        { {L"string", L"match", L"-q", L"a*", L"b", L"a", 0},               0, L"" },

        { {L"string", L"match", L"-r", 0},                                  2, L"" },
        { {L"string", L"match", L"-r", L"", 0},                             1, L"" },
        { {L"string", L"match", L"-r", L"", L"", 0},                        0, L"\n" },
        { {L"string", L"match", L"-r", L".", L"a", 0},                      0, L"a\n" },
        { {L"string", L"match", L"-r", L".*", L"", 0},                      0, L"\n" },
        { {L"string", L"match", L"-r", L"a*b", L"b", 0},                    0, L"b\n" },
        { {L"string", L"match", L"-r", L"a*b", L"aab", 0},                  0, L"aab\n" },
        { {L"string", L"match", L"-r", L"-i", L"a*b", L"Aab", 0},           0, L"Aab\n" },
        { {L"string", L"match", L"-r", L"-a", L"a[bc]", L"abadac", 0},      0, L"ab\nac\n" },
        { {L"string", L"match", L"-r", L"a", L"xaxa", L"axax", 0},          0, L"a\na\n" },
        { {L"string", L"match", L"-r", L"-a", L"a", L"xaxa", L"axax", 0},   0, L"a\na\na\na\n" },
        { {L"string", L"match", L"-r", L"a[bc]", L"abadac", 0},             0, L"ab\n" },
        { {L"string", L"match", L"-r", L"-q", L"a[bc]", L"abadac", 0},      0, L"" },
        { {L"string", L"match", L"-r", L"-q", L"a[bc]", L"ad", 0},          1, L"" },
        { {L"string", L"match", L"-r", L"(a+)b(c)", L"aabc", 0},            0, L"aabc\naa\nc\n" },
        { {L"string", L"match", L"-r", L"-a", L"(a)b(c)", L"abcabc", 0},    0, L"abc\na\nc\nabc\na\nc\n" },
        { {L"string", L"match", L"-r", L"(a)b(c)", L"abcabc", 0},           0, L"abc\na\nc\n" },
        { {L"string", L"match", L"-r", L"(a|(z))(bc)", L"abc", 0},          0, L"abc\na\nbc\n" },
        { {L"string", L"match", L"-r", L"-n", L"a", L"ada", L"dad", 0},     0, L"1 1\n2 1\n" },
        { {L"string", L"match", L"-r", L"-n", L"-a", L"a", L"bacadae", 0},  0, L"2 1\n4 1\n6 1\n" },
        { {L"string", L"match", L"-r", L"-n", L"(a).*(b)", L"a---b", 0},    0, L"1 5\n1 1\n5 1\n" },
        { {L"string", L"match", L"-r", L"-n", L"(a)(b)", L"ab", 0}, 0, L"1 2\n1 1\n2 1\n" },
        { {L"string", L"match", L"-r", L"-n", L"(a)(b)", L"abab", 0}, 0, L"1 2\n1 1\n2 1\n" },
        { {L"string", L"match", L"-r", L"-n", L"-a", L"(a)(b)", L"abab", 0}, 0, L"1 2\n1 1\n2 1\n3 2\n3 1\n4 1\n" },
        { {L"string", L"match", L"-r", L"*", L"", 0},                       2, L"" },
        { {L"string", L"match", L"-r", L"-a", L"a*", L"b", 0},              0, L"\n\n" },
        { {L"string", L"match", L"-r", L"foo\\Kbar", L"foobar", 0},         0, L"bar\n" },
        { {L"string", L"match", L"-r", L"(foo)\\Kbar", L"foobar", 0},       0, L"bar\nfoo\n" },
        { {L"string", L"match", L"-r", L"(?=ab\\K)", L"ab", 0},             0, L"\n" },
        { {L"string", L"match", L"-r", L"(?=ab\\K)..(?=cd\\K)", L"abcd", 0}, 0, L"\n" },

        { {L"string", L"replace", 0},                                       2, L"" },
        { {L"string", L"replace", L"", 0},                                  2, L"" },
        { {L"string", L"replace", L"", L"", 0},                             1, L"" },
        { {L"string", L"replace", L"", L"", L"", 0},                        1, L"\n" },
        { {L"string", L"replace", L"", L"", L" ", 0},                       1, L" \n" },
        { {L"string", L"replace", L"a", L"b", L"", 0},                      1, L"\n" },
        { {L"string", L"replace", L"a", L"b", L"a", 0},                     0, L"b\n" },
        { {L"string", L"replace", L"a", L"b", L"xax", 0},                   0, L"xbx\n" },
        { {L"string", L"replace", L"a", L"b", L"xax", L"axa", 0},           0, L"xbx\nbxa\n" },
        { {L"string", L"replace", L"bar", L"x", L"red barn", 0},            0, L"red xn\n" },
        { {L"string", L"replace", L"x", L"bar", L"red xn", 0},              0, L"red barn\n" },
        { {L"string", L"replace", L"--", L"x", L"-", L"xyz", 0},            0, L"-yz\n" },
        { {L"string", L"replace", L"--", L"y", L"-", L"xyz", 0},            0, L"x-z\n" },
        { {L"string", L"replace", L"--", L"z", L"-", L"xyz", 0},            0, L"xy-\n" },
        { {L"string", L"replace", L"-i", L"z", L"X", L"_Z_", 0},            0, L"_X_\n" },
        { {L"string", L"replace", L"-a", L"a", L"A", L"aaa", 0},            0, L"AAA\n" },
        { {L"string", L"replace", L"-i", L"a", L"z", L"AAA", 0},            0, L"zAA\n" },
        { {L"string", L"replace", L"-q", L"x", L">x<", L"x", 0},            0, L"" },
        { {L"string", L"replace", L"-a", L"x", L"", L"xxx", 0},             0, L"\n" },
        { {L"string", L"replace", L"-a", L"***", L"_", L"*****", 0},        0, L"_**\n" },
        { {L"string", L"replace", L"-a", L"***", L"***", L"******", 0},     0, L"******\n" },
        { {L"string", L"replace", L"-a", L"a", L"b", L"xax", L"axa", 0},    0, L"xbx\nbxb\n" },

        { {L"string", L"replace", L"-r", 0},                                2, L"" },
        { {L"string", L"replace", L"-r", L"", 0},                           2, L"" },
        { {L"string", L"replace", L"-r", L"", L"", 0},                      1, L"" },
        { {L"string", L"replace", L"-r", L"", L"", L"", 0},                 0, L"\n" },  // pcre2 behavior
        { {L"string", L"replace", L"-r", L"", L"", L" ", 0},                0, L" \n" }, // pcre2 behavior
        { {L"string", L"replace", L"-r", L"a", L"b", L"", 0},               1, L"\n" },
        { {L"string", L"replace", L"-r", L"a", L"b", L"a", 0},              0, L"b\n" },
        { {L"string", L"replace", L"-r", L".", L"x", L"abc", 0},            0, L"xbc\n" },
        { {L"string", L"replace", L"-r", L".", L"", L"abc", 0},             0, L"bc\n" },
        { {L"string", L"replace", L"-r", L"(\\w)(\\w)", L"$2$1", L"ab", 0}, 0, L"ba\n" },
        { {L"string", L"replace", L"-r", L"(\\w)", L"$1$1", L"ab", 0},      0, L"aab\n" },
        { {L"string", L"replace", L"-r", L"-a", L".", L"x", L"abc", 0},     0, L"xxx\n" },
        { {L"string", L"replace", L"-r", L"-a", L"(\\w)", L"$1$1", L"ab", 0}, 0, L"aabb\n" },
        { {L"string", L"replace", L"-r", L"-a", L".", L"", L"abc", 0},      0, L"\n" },
        { {L"string", L"replace", L"-r", L"a", L"x", L"bc", L"cd", L"de", 0}, 1, L"bc\ncd\nde\n" },
        { {L"string", L"replace", L"-r", L"a", L"x", L"aba", L"caa", 0},    0, L"xba\ncxa\n" },
        { {L"string", L"replace", L"-r", L"-a", L"a", L"x", L"aba", L"caa", 0}, 0, L"xbx\ncxx\n" },
        { {L"string", L"replace", L"-r", L"-i", L"A", L"b", L"xax", 0},     0, L"xbx\n" },
        { {L"string", L"replace", L"-r", L"-i", L"[a-z]", L".", L"1A2B", 0}, 0, L"1.2B\n" },
        { {L"string", L"replace", L"-r", L"A", L"b", L"xax", 0},            1, L"xax\n" },
        { {L"string", L"replace", L"-r", L"a", L"$1", L"a", 0},             2, L"" },
        { {L"string", L"replace", L"-r", L"(a)", L"$2", L"a", 0},           2, L"" },
        { {L"string", L"replace", L"-r", L"*", L".", L"a", 0},              2, L"" },
        { {L"string", L"replace", L"-r", L"^(.)", L"\t$1", L"abc", L"x", 0}, 0, L"\tabc\n\tx\n" },

        { {L"string", L"split", 0},                                         2, L"" },
        { {L"string", L"split", L":", 0},                                   1, L"" },
        { {L"string", L"split", L".", L"www.ch.ic.ac.uk", 0},               0, L"www\nch\nic\nac\nuk\n" },
        { {L"string", L"split", L"..", L"....", 0},                         0, L"\n\n\n" },
        { {L"string", L"split", L"-m", L"x", L"..", L"....", 0},            2, L"" },
        { {L"string", L"split", L"-m1", L"..", L"....", 0},                 0, L"\n..\n" },
        { {L"string", L"split", L"-m0", L"/", L"/usr/local/bin/fish", 0},   1, L"/usr/local/bin/fish\n" },
        { {L"string", L"split", L"-m2", L":", L"a:b:c:d", L"e:f:g:h", 0},   0, L"a\nb\nc:d\ne\nf\ng:h\n" },
        { {L"string", L"split", L"-m1", L"-r", L"/", L"/usr/local/bin/fish", 0}, 0, L"/usr/local/bin\nfish\n" },
        { {L"string", L"split", L"-r", L".", L"www.ch.ic.ac.uk", 0},        0, L"www\nch\nic\nac\nuk\n" },
        { {L"string", L"split", L"--", L"--", L"a--b---c----d", 0},         0, L"a\nb\n-c\n\nd\n" },
        { {L"string", L"split", L"-r", L"..", L"....", 0},                  0, L"\n\n\n" },
        { {L"string", L"split", L"-r", L"--", L"--", L"a--b---c----d", 0},  0, L"a\nb-\nc\n\nd\n" },
        { {L"string", L"split", L"", L"", 0},                               1, L"\n" },
        { {L"string", L"split", L"", L"a", 0},                              1, L"a\n" },
        { {L"string", L"split", L"", L"ab", 0},                             0, L"a\nb\n" },
        { {L"string", L"split", L"", L"abc", 0},                            0, L"a\nb\nc\n" },
        { {L"string", L"split", L"-m1", L"", L"abc", 0},                    0, L"a\nbc\n" },
        { {L"string", L"split", L"-r", L"", L"", 0},                        1, L"\n" },
        { {L"string", L"split", L"-r", L"", L"a", 0},                       1, L"a\n" },
        { {L"string", L"split", L"-r", L"", L"ab", 0},                      0, L"a\nb\n" },
        { {L"string", L"split", L"-r", L"", L"abc", 0},                     0, L"a\nb\nc\n" },
        { {L"string", L"split", L"-r", L"-m1", L"", L"abc", 0},             0, L"ab\nc\n" },
        { {L"string", L"split", L"-q", 0},                                  2, L"" },
        { {L"string", L"split", L"-q", L":", 0},                            1, L"" },
        { {L"string", L"split", L"-q", L"x", L"axbxc", 0},                  0, L"" },

        { {L"string", L"sub", 0},                                   1, L"" },
        { {L"string", L"sub", L"abcde", 0},                         0, L"abcde\n"},
        { {L"string", L"sub", L"-l", L"x", L"abcde", 0},            2, L""},
        { {L"string", L"sub", L"-s", L"x", L"abcde", 0},            2, L""},
        { {L"string", L"sub", L"-l0", L"abcde", 0},                 0, L"\n"},
        { {L"string", L"sub", L"-l2", L"abcde", 0},                 0, L"ab\n"},
        { {L"string", L"sub", L"-l5", L"abcde", 0},                 0, L"abcde\n"},
        { {L"string", L"sub", L"-l6", L"abcde", 0},                 0, L"abcde\n"},
        { {L"string", L"sub", L"-l-1", L"abcde", 0},                2, L""},
        { {L"string", L"sub", L"-s0", L"abcde", 0},                 2, L""},
        { {L"string", L"sub", L"-s1", L"abcde", 0},                 0, L"abcde\n"},
        { {L"string", L"sub", L"-s5", L"abcde", 0},                 0, L"e\n"},
        { {L"string", L"sub", L"-s6", L"abcde", 0},                 0, L"\n"},
        { {L"string", L"sub", L"-s-1", L"abcde", 0},                0, L"e\n"},
        { {L"string", L"sub", L"-s-5", L"abcde", 0},                0, L"abcde\n"},
        { {L"string", L"sub", L"-s-6", L"abcde", 0},                0, L"abcde\n"},
        { {L"string", L"sub", L"-s1", L"-l0", L"abcde", 0},         0, L"\n"},
        { {L"string", L"sub", L"-s1", L"-l1", L"abcde", 0},         0, L"a\n"},
        { {L"string", L"sub", L"-s2", L"-l2", L"abcde", 0},         0, L"bc\n"},
        { {L"string", L"sub", L"-s-1", L"-l1", L"abcde", 0},        0, L"e\n"},
        { {L"string", L"sub", L"-s-1", L"-l2", L"abcde", 0},        0, L"e\n"},
        { {L"string", L"sub", L"-s-3", L"-l2", L"abcde", 0},        0, L"cd\n"},
        { {L"string", L"sub", L"-s-3", L"-l4", L"abcde", 0},        0, L"cde\n"},
        { {L"string", L"sub", L"-q", 0},                            1, L"" },
        { {L"string", L"sub", L"-q", L"abcde", 0},                  0, L""},

        { {L"string", L"trim", 0},                                  1, L""},
        { {L"string", L"trim", L""},                                1, L"\n"},
        { {L"string", L"trim", L" "},                               0, L"\n"},
        { {L"string", L"trim", L"  \f\n\r\t"},                      0, L"\n"},
        { {L"string", L"trim", L" a"},                              0, L"a\n"},
        { {L"string", L"trim", L"a "},                              0, L"a\n"},
        { {L"string", L"trim", L" a "},                             0, L"a\n"},
        { {L"string", L"trim", L"-l", L" a"},                       0, L"a\n"},
        { {L"string", L"trim", L"-l", L"a "},                       1, L"a \n"},
        { {L"string", L"trim", L"-l", L" a "},                      0, L"a \n"},
        { {L"string", L"trim", L"-r", L" a"},                       1, L" a\n"},
        { {L"string", L"trim", L"-r", L"a "},                       0, L"a\n"},
        { {L"string", L"trim", L"-r", L" a "},                      0, L" a\n"},
        { {L"string", L"trim", L"-c", L".", L" a"},                 1, L" a\n"},
        { {L"string", L"trim", L"-c", L".", L"a "},                 1, L"a \n"},
        { {L"string", L"trim", L"-c", L".", L" a "},                1, L" a \n"},
        { {L"string", L"trim", L"-c", L".", L".a"},                 0, L"a\n"},
        { {L"string", L"trim", L"-c", L".", L"a."},                 0, L"a\n"},
        { {L"string", L"trim", L"-c", L".", L".a."},                0, L"a\n"},
        { {L"string", L"trim", L"-c", L"\\/", L"/a\\"},             0, L"a\n"},
        { {L"string", L"trim", L"-c", L"\\/", L"a/"},               0, L"a\n"},
        { {L"string", L"trim", L"-c", L"\\/", L"\\a/"},             0, L"a\n"},
        { {L"string", L"trim", L"-c", L"", L".a."},                 1, L".a.\n"},

        { {0}, 0, 0 }
    };

    struct string_test *t = string_tests;
    while (t->argv[0] != 0)
    {
        run_one_string_test(t->argv, t->expected_rc, t->expected_out);
        t++;
    }
}

/**
   Main test
*/
int main(int argc, char **argv)
{
    // Look for the file tests/test.fish. We expect to run in a directory containing that file.
    // If we don't find it, walk up the directory hierarchy until we do, or error
    while (access("./tests/test.fish", F_OK) != 0)
    {
        char wd[PATH_MAX + 1] = {};
        if (!getcwd(wd, sizeof wd))
        {
            perror("getcwd");
            exit(-1);
        }
        if (! strcmp(wd, "/"))
        {
            fprintf(stderr, "Unable to find 'tests' directory, which should contain file test.fish\n");
            exit(EXIT_FAILURE);
        }
        if (chdir(dirname(wd)) < 0)
        {
            perror("chdir");
        }
    }

    setlocale(LC_ALL, "");
    //srand(time(0));
    configure_thread_assertions_for_testing();

    program_name=L"(ignore)";
    s_arguments = argv + 1;

    say(L"Testing low-level functionality");
    set_main_thread();
    setup_fork_guards();
    proc_init();
    event_init();
    function_init();
    builtin_init();
    reader_init();
    env_init();

    /* Set default signal handlers, so we can ctrl-C out of this */
    signal_reset_handlers();

    if (should_test_function("highlighting")) test_highlighting();
    if (should_test_function("new_parser_ll2")) test_new_parser_ll2();
    if (should_test_function("new_parser_fuzzing")) test_new_parser_fuzzing(); //fuzzing is expensive
    if (should_test_function("new_parser_correctness")) test_new_parser_correctness();
    if (should_test_function("new_parser_ad_hoc")) test_new_parser_ad_hoc();
    if (should_test_function("new_parser_errors")) test_new_parser_errors();
    if (should_test_function("error_messages")) test_error_messages();
    if (should_test_function("escape")) test_unescape_sane();
    if (should_test_function("escape")) test_escape_crazy();
    if (should_test_function("format")) test_format();
    if (should_test_function("convert")) test_convert();
    if (should_test_function("convert_nulls")) test_convert_nulls();
    if (should_test_function("tok")) test_tok();
    if (should_test_function("iothread")) test_iothread();
    if (should_test_function("parser")) test_parser();
    if (should_test_function("cancellation")) test_cancellation();
    if (should_test_function("indents")) test_indents();
    if (should_test_function("utils")) test_utils();
    if (should_test_function("utf8")) test_utf8();
    if (should_test_function("escape_sequences")) test_escape_sequences();
    if (should_test_function("lru")) test_lru();
    if (should_test_function("expand")) test_expand();
    if (should_test_function("fuzzy_match")) test_fuzzy_match();
    if (should_test_function("abbreviations")) test_abbreviations();
    if (should_test_function("test")) test_test();
    if (should_test_function("path")) test_path();
    if (should_test_function("pager_navigation")) test_pager_navigation();
    if (should_test_function("word_motion")) test_word_motion();
    if (should_test_function("is_potential_path")) test_is_potential_path();
    if (should_test_function("colors")) test_colors();
    if (should_test_function("complete")) test_complete();
    if (should_test_function("input")) test_input();
    if (should_test_function("universal")) test_universal();
    if (should_test_function("universal")) test_universal_callbacks();
    if (should_test_function("notifiers")) test_universal_notifiers();
    if (should_test_function("completion_insertions")) test_completion_insertions();
    if (should_test_function("autosuggestion_ignores")) test_autosuggestion_ignores();
    if (should_test_function("autosuggestion_combining")) test_autosuggestion_combining();
    if (should_test_function("autosuggest_suggest_special")) test_autosuggest_suggest_special();
    if (should_test_function("wcstring_tok")) test_wcstring_tok();
    if (should_test_function("history")) history_tests_t::test_history();
    if (should_test_function("history_merge")) history_tests_t::test_history_merge();
    if (should_test_function("history_races")) history_tests_t::test_history_races();
    if (should_test_function("history_formats")) history_tests_t::test_history_formats();
    if (should_test_function("string")) test_string();
    // history_tests_t::test_history_speed();

    say(L"Encountered %d errors in low-level tests", err_count);
    if (s_test_run_count == 0)
        say(L"*** No Tests Were Actually Run! ***");

    /*
      Skip performance tests for now, since they seem to hang when running from inside make (?)
    */
//  say( L"Testing performance" );
//  perf_complete();

    reader_destroy();
    builtin_destroy();
    event_destroy();
    proc_destroy();

    if (err_count != 0)
    {
        return(1);
    }
}
