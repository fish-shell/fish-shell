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
#include "parse_tree.h"
#include "parse_util.h"

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
        printf("%lu / %lu\n", (unsigned long)(i+1), (unsigned long) max);
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
    int iterations = 1000;
    for (int i=0; i < iterations; i++)
    {
        iothread_perform(test_iothread_thread_call, (void (*)(int *, int))NULL, int_ptr);
    }

    // Now wait until we're done
    iothread_drain_all();

    // Should have incremented it once per thread
    if (*int_ptr != iterations)
    {
        say(L"Expected int to be %d, but instead it was %d", iterations, *int_ptr);
    }

    delete int_ptr;
}

/**
   Test the parser
*/
static void test_parser()
{
    say(L"Testing parser");

    parser_t parser(PARSER_TYPE_GENERAL, true);

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

    if (! parse_util_detect_errors(L"cat | exec") || ! parse_util_detect_errors(L"exec | cat"))
    {
        err(L"'exec' command in pipeline not reported as error");
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

    /* Ensure that we don't crash on infinite self recursion and mutual recursion. These must use the principal parser because we cannot yet execute jobs on other parsers (!) */
    say(L"Testing recursion detection");
    parser_t::principal_parser().eval(L"function recursive ; recursive ; end ; recursive; ", io_chain_t(), TOP);
#if 0
    /* This is disabled since it produces a long backtrace. We should find a way to either visually compress the backtrace, or disable error spewing */
    parser_t::principal_parser().eval(L"function recursive1 ; recursive2 ; end ; function recursive2 ; recursive1 ; end ; recursive1; ", io_chain_t(), TOP);
#endif
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
    shared_ptr<io_buffer_t> out_buff(io_buffer_t::create(false, STDOUT_FILENO));
    const io_chain_t io_chain(out_buff);
    test_cancellation_info_t ctx = {pthread_self(), 0.25 /* seconds */ };
    iothread_perform(signal_main, (void (*)(test_cancellation_info_t *, int))NULL, &ctx);
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
        {L"begin; end", 0},
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



    const indent_component_t *tests[] = {components1, components2, components3, components4, components5, components6, components7, components8, components9, components10, components11};
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
        assert(expected_indents.size() == text.size());

        // Compute the indents
        std::vector<int> indents = parse_util_compute_indents(text);

        if (expected_indents.size() != indents.size())
        {
            err(L"Indent vector has wrong size! Expected %lu, actual %lu", expected_indents.size(), indents.size());
        }
        assert(expected_indents.size() == indents.size());
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

static void test_escape_sequences(void)
{
    say(L"Testing escape codes");
    if (escape_code_length(L"") != 0) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"abcd") != 0) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b[2J") != 4) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b[38;5;123mABC") != strlen("\x1b[38;5;123m")) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1b@") != 2) err(L"test_escape_sequences failed on line %d\n", __LINE__);
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
    printf("input: %ls\n", in);
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

    completions.clear();
    complete(L"echo (/bin/mkdi", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 1);
    assert(completions.at(0).completion == L"r");

    completions.clear();
    complete(L"echo (ls /bin/mkdi", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 1);
    assert(completions.at(0).completion == L"r");

    completions.clear();
    complete(L"echo (command ls /bin/mkdi", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 1);
    assert(completions.at(0).completion == L"r");

    /* Add a function and test completing it in various ways */
    struct function_data_t func_data;
    func_data.name = L"scuttlebutt";
    func_data.definition = L"echo gongoozle";
    function_add(func_data, parser_t::principal_parser());

    /* Complete a function name */
    completions.clear();
    complete(L"echo (scuttlebut", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 1);
    assert(completions.at(0).completion == L"t");

    /* But not with the command prefix */
    completions.clear();
    complete(L"echo (command scuttlebut", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 0);

    /* Not with the builtin prefix */
    completions.clear();
    complete(L"echo (builtin scuttlebut", completions, COMPLETION_REQUEST_DEFAULT);
    assert(completions.size() == 0);

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

        complete(str, out, COMPLETION_REQUEST_DEFAULT);

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

        complete(str, out, COMPLETION_REQUEST_DEFAULT);

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
        {L"for i in a b c ; end", true}
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
            err(L"Failed to find node of type '%ls'", token_type_description(tests2[i].type).c_str());
        }
        else if (node_list.size() > 1)
        {
            err(L"Found too many nodes of type '%ls'", token_type_description(tests2[i].type).c_str());
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


    const highlight_component_t *tests[] = {components1, components2, components3, components4, components5, components6, components7, components8, components9, components10};
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
        std::vector<int> expected_colors;
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
        assert(expected_colors.size() == text.size());

        std::vector<highlight_spec_t> colors(text.size());
        highlight_shell(text, colors, 20, NULL, env_vars_snapshot_t());

        if (expected_colors.size() != colors.size())
        {
            err(L"Color vector has wrong size! Expected %lu, actual %lu", expected_colors.size(), colors.size());
        }
        assert(expected_colors.size() == colors.size());
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

    system("rm -Rf /tmp/fish_highlight_test");
}

/**
   Main test
*/
int main(int argc, char **argv)
{
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
    if (should_test_function("escape")) test_unescape_sane();
    if (should_test_function("escape")) test_escape_crazy();
    if (should_test_function("format")) test_format();
    if (should_test_function("convert")) test_convert();
    if (should_test_function("convert_nulls")) test_convert_nulls();
    if (should_test_function("tok")) test_tok();
    if (should_test_function("fork")) test_fork();
    if (should_test_function("iothread")) test_iothread();
    if (should_test_function("parser")) test_parser();
    if (should_test_function("cancellation")) test_cancellation();
    if (should_test_function("indents")) test_indents();
    if (should_test_function("utils")) test_utils();
    if (should_test_function("escape_sequences")) test_escape_sequences();
    if (should_test_function("lru")) test_lru();
    if (should_test_function("expand")) test_expand();
    if (should_test_function("fuzzy_match")) test_fuzzy_match();
    if (should_test_function("abbreviations")) test_abbreviations();
    if (should_test_function("test")) test_test();
    if (should_test_function("path")) test_path();
    if (should_test_function("word_motion")) test_word_motion();
    if (should_test_function("is_potential_path")) test_is_potential_path();
    if (should_test_function("colors")) test_colors();
    if (should_test_function("complete")) test_complete();
    if (should_test_function("completion_insertions")) test_completion_insertions();
    if (should_test_function("autosuggestion_combining")) test_autosuggestion_combining();
    if (should_test_function("autosuggest_suggest_special")) test_autosuggest_suggest_special();
    if (should_test_function("history")) history_tests_t::test_history();
    if (should_test_function("history_merge")) history_tests_t::test_history_merge();
    if (should_test_function("history_races")) history_tests_t::test_history_races();
    if (should_test_function("history_formats")) history_tests_t::test_history_formats();
    //history_tests_t::test_history_speed();

    say(L"Encountered %d errors in low-level tests", err_count);
    if (s_test_run_count == 0)
        say(L"*** No Tests Were Actually Run! ***");

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

    if(err_count != 0) {
        return(1);
    }
}
