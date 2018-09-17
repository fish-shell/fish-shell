// Various bug and feature tests. Compiled and run by make test.
#include "config.h"  // IWYU pragma: keep

// IWYU pragma: no_include <cstring>
// IWYU pragma: no_include <cstddef>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "builtin.h"
#include "color.h"
#include "common.h"
#include "complete.h"
#include "env.h"
#include "env_universal_common.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "function.h"
#include "future_feature_flags.h"
#include "highlight.h"
#include "history.h"
#include "input.h"
#include "input_common.h"
#include "io.h"
#include "iothread.h"
#include "lru.h"
#include "maybe.h"
#include "pager.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "screen.h"
#include "signal.h"
#include "tnode.h"
#include "tokenizer.h"
#include "utf8.h"
#include "util.h"
#include "wcstringutil.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

static const char *const *s_arguments;
static int s_test_run_count = 0;

// Indicate if we should test the given function. Either we test everything (all arguments) or we
// run only tests that have a prefix in s_arguments.
static bool should_test_function(const char *func_name) {
    // No args, test everything.
    bool result = false;
    if (!s_arguments || !s_arguments[0]) {
        result = true;
    } else {
        for (size_t i = 0; s_arguments[i] != NULL; i++) {
            if (!strncmp(func_name, s_arguments[i], strlen(s_arguments[i]))) {
                // Prefix match.
                result = true;
                break;
            }
        }
    }
    if (result) s_test_run_count++;
    return result;
}

/// The number of tests to run.
#define ESCAPE_TEST_COUNT 100000
/// The average length of strings to unescape.
#define ESCAPE_TEST_LENGTH 100
/// The higest character number of character to try and escape.
#define ESCAPE_TEST_CHAR 4000

/// Number of encountered errors.
static int err_count = 0;

/// Print formatted output.
static void say(const wchar_t *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vfwprintf(stdout, fmt, va);
    va_end(va);
    fwprintf(stdout, L"\n");
}

/// Print formatted error string.
static void err(const wchar_t *blah, ...) {
    va_list va;
    va_start(va, blah);
    err_count++;

    // Xcode's term doesn't support color (even though TERM claims it does).
    bool colorize = !getenv("RUNNING_IN_XCODE");

    // Show errors in red.
    if (colorize) {
        fputws(L"\x1B[31m", stdout);
    }
    fwprintf(stdout, L"Error: ");
    vfwprintf(stdout, blah, va);
    va_end(va);

    // Return to normal color.
    if (colorize) {
        fputws(L"\x1B[0m", stdout);
    }

    fwprintf(stdout, L"\n");
}

/// Joins a wcstring_list_t via commas.
static wcstring comma_join(const wcstring_list_t &lst) {
    wcstring result;
    for (size_t i = 0; i < lst.size(); i++) {
        if (i > 0) {
            result.push_back(L',');
        }
        result.append(lst.at(i));
    }
    return result;
}

static std::vector<const char *> pushed_dirs;

/// Helper to chdir and then update $PWD.
static bool pushd(const char *path) {
    char cwd[PATH_MAX] = {};
    if (getcwd(cwd, sizeof cwd) == NULL) {
        err(L"getcwd() from pushd() failed: errno = %d", errno);
        return false;
    }
    pushed_dirs.push_back(strdup(cwd));

    // We might need to create the directory. We don't care if this fails due to the directory
    // already being present.
    mkdir(path, 0770);

    int ret = chdir(path);
    if (ret != 0) {
        err(L"chdir(\"%s\") from pushd() failed: errno = %d", path, errno);
        return false;
    }

    env_set_pwd();
    return true;
}

static void popd() {
    const char *old_cwd = pushed_dirs.back();
    if (chdir(old_cwd) == -1) {
        err(L"chdir(\"%s\") from popd() failed: errno = %d", old_cwd, errno);
    }
    free((void *)old_cwd);
    pushed_dirs.pop_back();
    env_set_pwd();
}

// The odd formulation of these macros is to avoid "multiple unary operator" warnings from oclint
// were we to use the more natural "if (!(e)) err(..." form. We have to do this because the rules
// for the C preprocessor make it practically impossible to embed a comment in the body of a macro.
#define do_test(e)                                             \
    do {                                                       \
        if (e) {                                               \
            ;                                                  \
        } else {                                               \
            err(L"Test failed on line %lu: %s", __LINE__, #e); \
        }                                                      \
    } while (0)

#define do_test_from(e, from)                                                   \
    do {                                                                        \
        if (e) {                                                                \
            ;                                                                   \
        } else {                                                                \
            err(L"Test failed on line %lu (from %lu): %s", __LINE__, from, #e); \
        }                                                                       \
    } while (0)

#define do_test1(e, msg)                                           \
    do {                                                           \
        if (e) {                                                   \
            ;                                                      \
        } else {                                                   \
            err(L"Test failed on line %lu: %ls", __LINE__, (msg)); \
        }                                                          \
    } while (0)

/// Test that the fish functions for converting strings to numbers work.
static void test_str_to_num() {
    say(L"Testing str_to_num");
    const wchar_t *end;
    int i;
    long l;

    i = fish_wcstoi(L"");
    do_test1(errno == EINVAL && i == 0, L"converting empty string to int did not fail");
    i = fish_wcstoi(L" \n ");
    do_test1(errno == EINVAL && i == 0, L"converting whitespace string to int did not fail");
    i = fish_wcstoi(L"123");
    do_test1(errno == 0 && i == 123, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L"-123");
    do_test1(errno == 0 && i == -123, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L" 345  ");
    do_test1(errno == 0 && i == 345, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L" -345  ");
    do_test1(errno == 0 && i == -345, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L"x345");
    do_test1(errno == EINVAL && i == 0, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L" x345");
    do_test1(errno == EINVAL && i == 0, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"456 x");
    do_test1(errno == -1 && i == 456, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"99999999999999999999999");
    do_test1(errno == ERANGE && i == INT_MAX, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"-99999999999999999999999");
    do_test1(errno == ERANGE && i == INT_MIN, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"567]", &end);
    do_test1(errno == -1 && i == 567 && *end == L']',
             L"converting valid num to int did not succeed");
    // This is subtle. "567" in base 8 is "375" in base 10. The final "8" is not converted.
    i = fish_wcstoi(L"5678", &end, 8);
    do_test1(errno == -1 && i == 375 && *end == L'8',
             L"converting invalid num to int did not fail");

    l = fish_wcstol(L"");
    do_test1(errno == EINVAL && l == 0, L"converting empty string to long did not fail");
    l = fish_wcstol(L" \t ");
    do_test1(errno == EINVAL && l == 0, L"converting whitespace string to long did not fail");
    l = fish_wcstol(L"123");
    do_test1(errno == 0 && l == 123, L"converting valid num to long did not succeed");
    l = fish_wcstol(L"-123");
    do_test1(errno == 0 && l == -123, L"converting valid num to long did not succeed");
    l = fish_wcstol(L" 345  ");
    do_test1(errno == 0 && l == 345, L"converting valid num to long did not succeed");
    l = fish_wcstol(L" -345  ");
    do_test1(errno == 0 && l == -345, L"converting valid num to long did not succeed");
    l = fish_wcstol(L"x345");
    do_test1(errno == EINVAL && l == 0, L"converting invalid num to long did not fail");
    l = fish_wcstol(L" x345");
    do_test1(errno == EINVAL && l == 0, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"456 x");
    do_test1(errno == -1 && l == 456, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"99999999999999999999999");
    do_test1(errno == ERANGE && l == LONG_MAX, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"-99999999999999999999999");
    do_test1(errno == ERANGE && l == LONG_MIN, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"567]", &end);
    do_test1(errno == -1 && l == 567 && *end == L']',
             L"converting valid num to long did not succeed");
    // This is subtle. "567" in base 8 is "375" in base 10. The final "8" is not converted.
    l = fish_wcstol(L"5678", &end, 8);
    do_test1(errno == -1 && l == 375 && *end == L'8',
             L"converting invalid num to long did not fail");
}

/// Test sane escapes.
static void test_unescape_sane() {
    const struct test_t {
        const wchar_t *input;
        const wchar_t *expected;
    } tests[] = {
        {L"abcd", L"abcd"},           {L"'abcd'", L"abcd"},
        {L"'abcd\\n'", L"abcd\\n"},   {L"\"abcd\\n\"", L"abcd\\n"},
        {L"\"abcd\\n\"", L"abcd\\n"}, {L"\\143", L"c"},
        {L"'\\143'", L"\\143"},       {L"\\n", L"\n"}  // \n normally becomes newline
    };
    wcstring output;
    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        bool ret = unescape_string(tests[i].input, &output, UNESCAPE_DEFAULT);
        if (!ret) {
            err(L"Failed to unescape '%ls'\n", tests[i].input);
        } else if (output != tests[i].expected) {
            err(L"In unescaping '%ls', expected '%ls' but got '%ls'\n", tests[i].input,
                tests[i].expected, output.c_str());
        }
    }

    // Test for overflow.
    if (unescape_string(L"echo \\UFFFFFF", &output, UNESCAPE_DEFAULT)) {
        err(L"Should not have been able to unescape \\UFFFFFF\n");
    }
    if (unescape_string(L"echo \\U110000", &output, UNESCAPE_DEFAULT)) {
        err(L"Should not have been able to unescape \\U110000\n");
    }
#if WCHAR_MAX != 0xffff
    // TODO: Make this work on MS Windows.
    if (!unescape_string(L"echo \\U10FFFF", &output, UNESCAPE_DEFAULT)) {
        err(L"Should have been able to unescape \\U10FFFF\n");
    }
#endif
}

/// Test the escaping/unescaping code by escaping/unescaping random strings and verifying that the
/// original string comes back.
static void test_escape_crazy() {
    say(L"Testing escaping and unescaping");
    wcstring random_string;
    wcstring escaped_string;
    wcstring unescaped_string;
    bool unescaped_success;
    for (size_t i = 0; i < ESCAPE_TEST_COUNT; i++) {
        random_string.clear();
        while (rand() % ESCAPE_TEST_LENGTH) {
            random_string.push_back((rand() % ESCAPE_TEST_CHAR) + 1);
        }

        escaped_string = escape_string(random_string, ESCAPE_ALL);
        unescaped_success = unescape_string(escaped_string, &unescaped_string, UNESCAPE_DEFAULT);

        if (!unescaped_success) {
            err(L"Failed to unescape string <%ls>", escaped_string.c_str());
        } else if (unescaped_string != random_string) {
            err(L"Escaped and then unescaped string '%ls', but got back a different string '%ls'",
                random_string.c_str(), unescaped_string.c_str());
        }
    }

    // Verify that not using `ESCAPE_ALL` also escapes backslashes so we don't regress on issue
    // #3892.
    random_string = L"line 1\\n\nline 2";
    escaped_string = escape_string(random_string, ESCAPE_NO_QUOTED);
    unescaped_success = unescape_string(escaped_string, &unescaped_string, UNESCAPE_DEFAULT);
    if (!unescaped_success) {
        err(L"Failed to unescape string <%ls>", escaped_string.c_str());
    } else if (unescaped_string != random_string) {
        err(L"Escaped and then unescaped string '%ls', but got back a different string '%ls'",
            random_string.c_str(), unescaped_string.c_str());
    }
}

static void test_escape_quotes() {
    say(L"Testing escaping with quotes");
    // These are "raw string literals"
    do_test(parse_util_escape_string_with_quote(L"abc", L'\0') == L"abc");
    do_test(parse_util_escape_string_with_quote(L"abc~def", L'\0') == L"abc\\~def");
    do_test(parse_util_escape_string_with_quote(L"abc~def", L'\0', true) == L"abc~def");
    do_test(parse_util_escape_string_with_quote(L"abc\\~def", L'\0') == L"abc\\\\\\~def");
    do_test(parse_util_escape_string_with_quote(L"abc\\~def", L'\0', true) == L"abc\\\\~def");
    do_test(parse_util_escape_string_with_quote(L"~abc", L'\0') == L"\\~abc");
    do_test(parse_util_escape_string_with_quote(L"~abc", L'\0', true) == L"~abc");
    do_test(parse_util_escape_string_with_quote(L"~abc|def", L'\0') == L"\\~abc\\|def");
    do_test(parse_util_escape_string_with_quote(L"|abc~def", L'\0') == L"\\|abc\\~def");
    do_test(parse_util_escape_string_with_quote(L"|abc~def", L'\0', true) == L"\\|abc~def");
    do_test(parse_util_escape_string_with_quote(L"foo\nbar", L'\0') == L"foo\\nbar");

    // Note tildes are not expanded inside quotes, so no_tilde is ignored with a quote.
    do_test(parse_util_escape_string_with_quote(L"abc", L'\'') == L"abc");
    do_test(parse_util_escape_string_with_quote(L"abc\\def", L'\'') == L"abc\\\\def");
    do_test(parse_util_escape_string_with_quote(L"abc'def", L'\'') == L"abc\\'def");
    do_test(parse_util_escape_string_with_quote(L"~abc'def", L'\'') == L"~abc\\'def");
    do_test(parse_util_escape_string_with_quote(L"~abc'def", L'\'', true) == L"~abc\\'def");
    do_test(parse_util_escape_string_with_quote(L"foo\nba'r", L'\'') == L"foo'\\n'ba\\'r");
    do_test(parse_util_escape_string_with_quote(L"foo\\\\bar", L'\'') == L"foo\\\\\\\\bar");

    do_test(parse_util_escape_string_with_quote(L"abc", L'"') == L"abc");
    do_test(parse_util_escape_string_with_quote(L"abc\\def", L'"') == L"abc\\\\def");
    do_test(parse_util_escape_string_with_quote(L"~abc'def", L'"') == L"~abc'def");
    do_test(parse_util_escape_string_with_quote(L"~abc'def", L'"', true) == L"~abc'def");
    do_test(parse_util_escape_string_with_quote(L"foo\nba'r", L'"') == L"foo\"\\n\"ba'r");
    do_test(parse_util_escape_string_with_quote(L"foo\\\\bar", L'"') == L"foo\\\\\\\\bar");
}

static void test_format() {
    say(L"Testing formatting functions");
    struct {
        unsigned long long val;
        const char *expected;
    } tests[] = {{0, "empty"},  {1, "1B"},       {2, "2B"},
                 {1024, "1kB"}, {1870, "1.8kB"}, {4322911, "4.1MB"}};
    size_t i;
    for (i = 0; i < sizeof tests / sizeof *tests; i++) {
        char buff[128];
        format_size_safe(buff, tests[i].val);
        do_test(!strcmp(buff, tests[i].expected));
    }

    for (int j = -129; j <= 129; j++) {
        char buff1[128], buff2[128];
        format_long_safe(buff1, j);
        sprintf(buff2, "%d", j);
        do_test(!strcmp(buff1, buff2));

        wchar_t wbuf1[128], wbuf2[128];
        format_long_safe(wbuf1, j);
        swprintf(wbuf2, 128, L"%d", j);
        do_test(!wcscmp(wbuf1, wbuf2));
    }

    long q = LONG_MIN;
    char buff1[128], buff2[128];
    format_long_safe(buff1, q);
    sprintf(buff2, "%ld", q);
    do_test(!strcmp(buff1, buff2));
}

/// Helper to convert a narrow string to a sequence of hex digits.
static char *str2hex(const char *input) {
    char *output = (char *)malloc(5 * strlen(input) + 1);
    char *p = output;
    for (; *input; input++) {
        sprintf(p, "0x%02X ", (int)*input & 0xFF);
        p += 5;
    }
    *p = '\0';
    return output;
}

/// Test wide/narrow conversion by creating random strings and verifying that the original string
/// comes back through double conversion.
static void test_convert() {
    int i;
    std::vector<char> sb;

    say(L"Testing wide/narrow string conversion");

    for (i = 0; i < ESCAPE_TEST_COUNT; i++) {
        const char *o, *n;
        char c;

        sb.clear();
        while (rand() % ESCAPE_TEST_LENGTH) {
            c = rand();
            sb.push_back(c);
        }
        c = 0;
        sb.push_back(c);

        o = &sb.at(0);
        const wcstring w = str2wcstring(o);
        n = wcs2str(w);

        if (!o || !n) {
            err(L"Line %d - Conversion cycle of string %s produced null pointer on %s", __LINE__, o,
                L"wcs2str");
        }

        if (strcmp(o, n)) {
            char *o2 = str2hex(o);
            char *n2 = str2hex(n);
            err(L"Line %d - %d: Conversion cycle of string:\n%4d chars: %s\n"
                L"produced different string:\n%4d chars: %s",
                __LINE__, i, strlen(o), o2, strlen(n), n2);
            free(o2);
            free(n2);
        }
        free((void *)n);
    }
}

/// Verify correct behavior with embedded nulls.
static void test_convert_nulls() {
    say(L"Testing convert_nulls");
    const wchar_t in[] = L"AAA\0BBB";
    const size_t in_len = (sizeof in / sizeof *in) - 1;
    const wcstring in_str = wcstring(in, in_len);
    std::string out_str = wcs2string(in_str);
    if (out_str.size() != in_len) {
        err(L"Embedded nulls mishandled in wcs2string");
    }
    for (size_t i = 0; i < in_len; i++) {
        if (in[i] != out_str.at(i)) {
            err(L"Embedded nulls mishandled in wcs2string at index %lu", (unsigned long)i);
        }
    }

    wcstring out_wstr = str2wcstring(out_str);
    if (out_wstr.size() != in_len) {
        err(L"Embedded nulls mishandled in str2wcstring");
    }
    for (size_t i = 0; i < in_len; i++) {
        if (in[i] != out_wstr.at(i)) {
            err(L"Embedded nulls mishandled in str2wcstring at index %lu", (unsigned long)i);
        }
    }
}

/// Test the tokenizer.
static void test_tokenizer() {
    say(L"Testing tokenizer");
    tok_t token;

    {
        bool got = false;
        const wchar_t *str = L"alpha beta";
        tokenizer_t t(str, 0);

        got = t.next(&token);  // alpha
        do_test(got);
        do_test(token.type == TOK_STRING);
        do_test(token.offset == 0);
        do_test(token.length == 5);
        do_test(t.text_of(token) == L"alpha");

        got = t.next(&token);  // beta
        do_test(got);
        do_test(token.type == TOK_STRING);
        do_test(token.offset == 6);
        do_test(token.length == 4);
        do_test(t.text_of(token) == L"beta");

        got = t.next(&token);
        do_test(!got);
    }

    const wchar_t *str =
        L"string <redirection  2>&1 'nested \"quoted\" '(string containing subshells "
        L"){and,brackets}$as[$well (as variable arrays)] not_a_redirect^ ^ ^^is_a_redirect "
        L"&&& ||| "
        L"&& || & |"
        L"Compress_Newlines\n  \n\t\n   \nInto_Just_One";
    const int types[] = {TOK_STRING, TOK_REDIRECT,   TOK_STRING,   TOK_REDIRECT, TOK_STRING,
                         TOK_STRING, TOK_STRING,     TOK_REDIRECT, TOK_REDIRECT, TOK_STRING,
                         TOK_ANDAND, TOK_BACKGROUND, TOK_OROR,     TOK_PIPE,     TOK_ANDAND,
                         TOK_OROR,   TOK_BACKGROUND, TOK_PIPE,     TOK_STRING,   TOK_END,
                         TOK_STRING};

    say(L"Test correct tokenization");

    {
        tokenizer_t t(str, 0);
        size_t i = 0;
        while (t.next(&token)) {
            if (i >= sizeof types / sizeof *types) {
                err(L"Too many tokens returned from tokenizer");
                fwprintf(stdout, L"Got excess token type %ld\n", (long)token.type);
                break;
            }
            if (types[i] != token.type) {
                err(L"Tokenization error:");
                fwprintf(stdout,
                         L"Token number %zu of string \n'%ls'\n, expected type %ld, got token type "
                         L"%ld\n",
                         i + 1, str, (long)types[i], (long)token.type);
            }
            i++;
        }
        if (i < sizeof types / sizeof *types) {
            err(L"Too few tokens returned from tokenizer");
        }
    }

    // Test some errors.
    {
        tokenizer_t t(L"abc\\", 0);
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == tokenizer_error_t::unterminated_escape);
        do_test(token.error_offset == 3);
    }

    {
        tokenizer_t t(L"abc )defg(hij", 0);
        do_test(t.next(&token));
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == tokenizer_error_t::closing_unopened_subshell);
        do_test(token.error_offset == 4);
    }

    {
        tokenizer_t t(L"abc defg(hij (klm)", 0);
        do_test(t.next(&token));
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == tokenizer_error_t::unterminated_subshell);
        do_test(token.error_offset == 4);
    }

    {
        tokenizer_t t(L"abc defg[hij (klm)", 0);
        do_test(t.next(&token));
        do_test(t.next(&token));
        do_test(token.type == TOK_ERROR);
        do_test(token.error == tokenizer_error_t::unterminated_slice);
        do_test(token.error_offset == 4);
    }

    // Test redirection_type_for_string.
    if (redirection_type_for_string(L"<") != redirection_type_t::input)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"^") != redirection_type_t::overwrite)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L">") != redirection_type_t::overwrite)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>") != redirection_type_t::overwrite)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L">>") != redirection_type_t::append)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>>") != redirection_type_t::append)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>?") != redirection_type_t::noclob)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"9999999999999999>?"))
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>&3") != redirection_type_t::fd)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    if (redirection_type_for_string(L"2>|"))
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);

    // Test ^ with our feature flag on and off.
    auto saved_flags = fish_features();
    mutable_fish_features().set(features_t::stderr_nocaret, false);
    if (redirection_type_for_string(L"^") != redirection_type_t::overwrite)
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    mutable_fish_features().set(features_t::stderr_nocaret, true);
    if (redirection_type_for_string(L"^") != none())
        err(L"redirection_type_for_string failed on line %ld", (long)__LINE__);
    mutable_fish_features() = saved_flags;
}

// Little function that runs in a background thread, bouncing to the main.
static int test_iothread_thread_call(std::atomic<int> *addr) {
    int before = *addr;
    iothread_perform_on_main([=]() { *addr += 1; });
    int after = *addr;

    // Must have incremented it at least once.
    if (before >= after) {
        err(L"Failed to increment from background thread");
    }
    return after;
}

static void test_iothread() {
    say(L"Testing iothreads");
    std::unique_ptr<std::atomic<int>> int_ptr = make_unique<std::atomic<int>>(0);
    int iterations = 50000;
    int max_achieved_thread_count = 0;
    double start = timef();
    for (int i = 0; i < iterations; i++) {
        int thread_count = iothread_perform([&]() { test_iothread_thread_call(int_ptr.get()); });
        max_achieved_thread_count = std::max(max_achieved_thread_count, thread_count);
    }

    // Now wait until we're done.
    iothread_drain_all();
    double end = timef();

    // Should have incremented it once per thread.
    if (*int_ptr != iterations) {
        say(L"Expected int to be %d, but instead it was %d", iterations, int_ptr->load());
    }

    say(L"    (%.02f msec, with max of %d threads)", (end - start) * 1000.0,
        max_achieved_thread_count);
}

static parser_test_error_bits_t detect_argument_errors(const wcstring &src) {
    parse_node_tree_t tree;
    if (!parse_tree_from_string(src, parse_flag_none, &tree, NULL, symbol_argument_list)) {
        return PARSER_TEST_ERROR;
    }

    assert(!tree.empty());  //!OCLINT(multiple unary operator)
    tnode_t<grammar::argument_list> arg_list{&tree, &tree.at(0)};
    auto first_arg = arg_list.next_in_list<grammar::argument>();
    return parse_util_detect_errors_in_argument(first_arg, first_arg.get_source(src));
}

/// Test the parser.
static void test_parser() {
    say(L"Testing parser");

    say(L"Testing block nesting");
    if (!parse_util_detect_errors(L"if; end")) {
        err(L"Incomplete if statement undetected");
    }
    if (!parse_util_detect_errors(L"if test; echo")) {
        err(L"Missing end undetected");
    }
    if (!parse_util_detect_errors(L"if test; end; end")) {
        err(L"Unbalanced end undetected");
    }

    say(L"Testing detection of invalid use of builtin commands");
    if (!parse_util_detect_errors(L"case foo")) {
        err(L"'case' command outside of block context undetected");
    }
    if (!parse_util_detect_errors(L"switch ggg; if true; case foo;end;end")) {
        err(L"'case' command outside of switch block context undetected");
    }
    if (!parse_util_detect_errors(L"else")) {
        err(L"'else' command outside of conditional block context undetected");
    }
    if (!parse_util_detect_errors(L"else if")) {
        err(L"'else if' command outside of conditional block context undetected");
    }
    if (!parse_util_detect_errors(L"if false; else if; end")) {
        err(L"'else if' missing command undetected");
    }

    if (!parse_util_detect_errors(L"break")) {
        err(L"'break' command outside of loop block context undetected");
    }

    if (parse_util_detect_errors(L"break --help")) {
        err(L"'break --help' incorrectly marked as error");
    }

    if (!parse_util_detect_errors(L"while false ; function foo ; break ; end ; end ")) {
        err(L"'break' command inside function allowed to break from loop outside it");
    }

    if (!parse_util_detect_errors(L"exec ls|less") || !parse_util_detect_errors(L"echo|return")) {
        err(L"Invalid pipe command undetected");
    }

    if (parse_util_detect_errors(L"for i in foo ; switch $i ; case blah ; break; end; end ")) {
        err(L"'break' command inside switch falsely reported as error");
    }

    if (parse_util_detect_errors(L"or cat | cat") || parse_util_detect_errors(L"and cat | cat")) {
        err(L"boolean command at beginning of pipeline falsely reported as error");
    }

    if (!parse_util_detect_errors(L"cat | and cat")) {
        err(L"'and' command in pipeline not reported as error");
    }

    if (!parse_util_detect_errors(L"cat | or cat")) {
        err(L"'or' command in pipeline not reported as error");
    }

    if (!parse_util_detect_errors(L"cat | exec") || !parse_util_detect_errors(L"exec | cat")) {
        err(L"'exec' command in pipeline not reported as error");
    }

    if (!parse_util_detect_errors(L"begin ; end arg")) {
        err(L"argument to 'end' not reported as error");
    }

    if (!parse_util_detect_errors(L"switch foo ; end arg")) {
        err(L"argument to 'end' not reported as error");
    }

    if (!parse_util_detect_errors(L"if true; else if false ; end arg")) {
        err(L"argument to 'end' not reported as error");
    }

    if (!parse_util_detect_errors(L"if true; else ; end arg")) {
        err(L"argument to 'end' not reported as error");
    }

    if (parse_util_detect_errors(L"begin ; end 2> /dev/null")) {
        err(L"redirection after 'end' wrongly reported as error");
    }

    if (parse_util_detect_errors(L"true | ") != PARSER_TEST_INCOMPLETE) {
        err(L"unterminated pipe not reported properly");
    }

    if (parse_util_detect_errors(L"begin ; true ; end | ") != PARSER_TEST_INCOMPLETE) {
        err(L"unterminated pipe not reported properly");
    }

    if (parse_util_detect_errors(L" | true ") != PARSER_TEST_ERROR) {
        err(L"leading pipe not reported properly");
    }

    if (parse_util_detect_errors(L"true | # comment") != PARSER_TEST_INCOMPLETE) {
        err(L"comment after pipe not reported as incomplete");
    }

    if (parse_util_detect_errors(L"true | # comment \n false ")) {
        err(L"comment and newline after pipe wrongly reported as error");
    }

    if (parse_util_detect_errors(L"true | ; false ") != PARSER_TEST_ERROR) {
        err(L"semicolon after pipe not detected as error");
    }

    if (detect_argument_errors(L"foo")) {
        err(L"simple argument reported as error");
    }

    if (detect_argument_errors(L"''")) {
        err(L"Empty string reported as error");
    }

    if (!(detect_argument_errors(L"foo$$") & PARSER_TEST_ERROR)) {
        err(L"Bad variable expansion not reported as error");
    }

    if (!(detect_argument_errors(L"foo$@") & PARSER_TEST_ERROR)) {
        err(L"Bad variable expansion not reported as error");
    }

    // Within command substitutions, we should be able to detect everything that
    // parse_util_detect_errors can detect.
    if (!(detect_argument_errors(L"foo(cat | or cat)") & PARSER_TEST_ERROR)) {
        err(L"Bad command substitution not reported as error");
    }

    if (!(detect_argument_errors(L"foo\\xFF9") & PARSER_TEST_ERROR)) {
        err(L"Bad escape not reported as error");
    }

    if (!(detect_argument_errors(L"foo(echo \\xFF9)") & PARSER_TEST_ERROR)) {
        err(L"Bad escape in command substitution not reported as error");
    }

    if (!(detect_argument_errors(L"foo(echo (echo (echo \\xFF9)))") & PARSER_TEST_ERROR)) {
        err(L"Bad escape in nested command substitution not reported as error");
    }

    if (!parse_util_detect_errors(L"false & ; and cat")) {
        err(L"'and' command after background not reported as error");
    }

    if (!parse_util_detect_errors(L"true & ; or cat")) {
        err(L"'or' command after background not reported as error");
    }

    if (parse_util_detect_errors(L"true & ; not cat")) {
        err(L"'not' command after background falsely reported as error");
    }

    if (!parse_util_detect_errors(L"if true & ; end")) {
        err(L"backgrounded 'if' conditional not reported as error");
    }

    if (!parse_util_detect_errors(L"if false; else if true & ; end")) {
        err(L"backgrounded 'else if' conditional not reported as error");
    }

    if (!parse_util_detect_errors(L"while true & ; end")) {
        err(L"backgrounded 'while' conditional not reported as error");
    }

    if (!parse_util_detect_errors(L"true | || false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"|| false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"&& false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"true ; && false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"true ; || false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"true || && false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"true && || false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    if (!parse_util_detect_errors(L"true && && false")) {
        err(L"bogus boolean statement error not detected on line %d", __LINE__);
    }

    say(L"Testing basic evaluation");

    // Ensure that we don't crash on infinite self recursion and mutual recursion. These must use
    // the principal parser because we cannot yet execute jobs on other parsers.
    say(L"Testing recursion detection");
    parser_t::principal_parser().eval(L"function recursive ; recursive ; end ; recursive; ",
                                      io_chain_t(), TOP);
#if 0
    // This is disabled since it produces a long backtrace. We should find a way to either visually
    // compress the backtrace, or disable error spewing.
    parser_t::principal_parser().eval(L"function recursive1 ; recursive2 ; end ; "
            L"function recursive2 ; recursive1 ; end ; recursive1; ", io_chain_t(), TOP);
#endif

    say(L"Testing empty function name");
    parser_t::principal_parser().eval(L"function '' ; echo fail; exit 42 ; end ; ''", io_chain_t(),
                                      TOP);

    say(L"Testing eval_args");
    completion_list_t comps;
    parser_t::expand_argument_list(L"alpha 'beta gamma' delta", 0, &comps);
    do_test(comps.size() == 3);
    do_test(comps.at(0).completion == L"alpha");
    do_test(comps.at(1).completion == L"beta gamma");
    do_test(comps.at(2).completion == L"delta");
}

static void test_1_cancellation(const wchar_t *src) {
    shared_ptr<io_buffer_t> out_buff(io_buffer_t::create(STDOUT_FILENO, io_chain_t()));
    const io_chain_t io_chain(out_buff);
    pthread_t thread = pthread_self();
    double delay = 0.25 /* seconds */;
    iothread_perform([=]() {
        /// Wait a while and then SIGINT the main thread.
        usleep(delay * 1E6);
        pthread_kill(thread, SIGINT);
    });
    parser_t::principal_parser().eval(src, io_chain, TOP);
    out_buff->read();
    if (out_buff->buffer().size() != 0) {
        err(L"Expected 0 bytes in out_buff, but instead found %lu bytes\n",
            out_buff->buffer().size());
    }
    iothread_drain_all();
}

static void test_cancellation() {
    if (getenv("RUNNING_IN_XCODE")) {
        say(L"Skipping Ctrl-C cancellation test because we are running in Xcode debugger");
        return;
    }
    say(L"Testing Ctrl-C cancellation. If this hangs, that's a bug!");

    // Enable fish's signal handling here. We need to make this interactive for fish to install its
    // signal handlers.
    proc_push_interactive(1);
    signal_set_handlers();

    // This tests that we can correctly ctrl-C out of certain loop constructs, and that nothing gets
    // printed if we do.

    // Here the command substitution is an infinite loop. echo never even gets its argument, so when
    // we cancel we expect no output.
    test_1_cancellation(L"echo (while true ; echo blah ; end)");

    // Nasty infinite loop that doesn't actually execute anything.
    test_1_cancellation(L"echo (while true ; end) (while true ; end) (while true ; end)");
    test_1_cancellation(L"while true ; end");
    test_1_cancellation(L"while true ; echo nothing > /dev/null; end");
    test_1_cancellation(L"for i in (while true ; end) ; end");

    // Ensure that if child processes SIGINT, we exit our loops
    // Test for #3780
    // Ugly hack - temporarily set is_interactive_session
    // else we will SIGINT ourselves in response to our child death
    scoped_push<bool> iis(&is_interactive_session, true);
    const wchar_t *child_self_destructor = L"while true ; sh -c 'sleep .25; kill -s INT $$' ; end";
    parser_t::principal_parser().eval(child_self_destructor, io_chain_t(), TOP);
    iis.restore();

    // Restore signal handling.
    proc_pop_interactive();
    signal_reset_handlers();

    // Ensure that we don't think we should cancel.
    reader_reset_interrupted();
}

static void test_indents() {
    say(L"Testing indents");

    // Here are the components of our source and the indents we expect those to be.
    struct indent_component_t {
        const wchar_t *txt;
        int indent;
    };

    const indent_component_t components1[] = {{L"if foo", 0}, {L"end", 0}, {NULL, -1}};

    const indent_component_t components2[] = {{L"if foo", 0},
                                              {L"", 1},  // trailing newline!
                                              {NULL, -1}};

    const indent_component_t components3[] = {{L"if foo", 0},
                                              {L"foo", 1},
                                              {L"end", 0},  // trailing newline!
                                              {NULL, -1}};

    const indent_component_t components4[] = {{L"if foo", 0}, {L"if bar", 1}, {L"end", 1},
                                              {L"end", 0},    {L"", 0},       {NULL, -1}};

    const indent_component_t components5[] = {{L"if foo", 0}, {L"if bar", 1}, {L"", 2}, {NULL, -1}};

    const indent_component_t components6[] = {{L"begin", 0}, {L"foo", 1}, {L"", 1}, {NULL, -1}};

    const indent_component_t components7[] = {{L"begin", 0}, {L";", 1}, {L"end", 0},
                                              {L"foo", 0},   {L"", 0},  {NULL, -1}};

    const indent_component_t components8[] = {{L"if foo", 0}, {L"if bar", 1}, {L"baz", 2},
                                              {L"end", 1},    {L"", 1},       {NULL, -1}};

    const indent_component_t components9[] = {{L"switch foo", 0}, {L"", 1}, {NULL, -1}};

    const indent_component_t components10[] = {
        {L"switch foo", 0}, {L"case bar", 1}, {L"case baz", 1}, {L"quux", 2}, {L"", 2}, {NULL, -1}};

    const indent_component_t components11[] = {{L"switch foo", 0},
                                               {L"cas", 1},  // parse error indentation handling
                                               {NULL, -1}};

    const indent_component_t components12[] = {{L"while false", 0},
                                               {L"# comment", 1},   // comment indentation handling
                                               {L"command", 1},     // comment indentation handling
                                               {L"# comment2", 1},  // comment indentation handling
                                               {NULL, -1}};

    const indent_component_t *tests[] = {components1, components2,  components3,  components4,
                                         components5, components6,  components7,  components8,
                                         components9, components10, components11, components12};
    for (size_t which = 0; which < sizeof tests / sizeof *tests; which++) {
        const indent_component_t *components = tests[which];
        // Count how many we have.
        size_t component_count = 0;
        while (components[component_count].txt != NULL) {
            component_count++;
        }

        // Generate the expected indents.
        wcstring text;
        std::vector<int> expected_indents;
        for (size_t i = 0; i < component_count; i++) {
            if (i > 0) {
                text.push_back(L'\n');
                expected_indents.push_back(components[i].indent);
            }
            text.append(components[i].txt);
            expected_indents.resize(text.size(), components[i].indent);
        }
        do_test(expected_indents.size() == text.size());

        // Compute the indents.
        std::vector<int> indents = parse_util_compute_indents(text);

        if (expected_indents.size() != indents.size()) {
            err(L"Indent vector has wrong size! Expected %lu, actual %lu", expected_indents.size(),
                indents.size());
        }
        do_test(expected_indents.size() == indents.size());
        for (size_t i = 0; i < text.size(); i++) {
            if (expected_indents.at(i) != indents.at(i)) {
                err(L"Wrong indent at index %lu in test #%lu (expected %d, actual %d):\n%ls\n", i,
                    which + 1, expected_indents.at(i), indents.at(i), text.c_str());
                break;  // don't keep showing errors for the rest of the line
            }
        }
    }
}

static void test_parse_util_cmdsubst_extent() {
    const wchar_t *a = L"echo (echo (echo hi";
    const wchar_t *begin = NULL, *end = NULL;

    parse_util_cmdsubst_extent(a, 0, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) {
        err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    }
    parse_util_cmdsubst_extent(a, 1, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) {
        err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    }
    parse_util_cmdsubst_extent(a, 2, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) {
        err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    }
    parse_util_cmdsubst_extent(a, 3, &begin, &end);
    if (begin != a || end != begin + wcslen(begin)) {
        err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    }

    parse_util_cmdsubst_extent(a, 8, &begin, &end);
    if (begin != a + wcslen(L"echo (")) {
        err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    }

    parse_util_cmdsubst_extent(a, 17, &begin, &end);
    if (begin != a + wcslen(L"echo (echo (")) {
        err(L"parse_util_cmdsubst_extent failed on line %ld", (long)__LINE__);
    }
}

static struct wcsfilecmp_test {
    const wchar_t *str1;
    const wchar_t *str2;
    int expected_rc;
} wcsfilecmp_tests[] = {{L"", L"", 0},
                        {L"", L"def", -1},
                        {L"abc", L"", 1},
                        {L"abc", L"def", -1},
                        {L"abc", L"DEF", -1},
                        {L"DEF", L"abc", 1},
                        {L"abc", L"abc", 0},
                        {L"ABC", L"ABC", 0},
                        {L"AbC", L"abc", -1},
                        {L"AbC", L"ABC", 1},
                        {L"def", L"abc", 1},
                        {L"1ghi", L"1gHi", 1},
                        {L"1ghi", L"2ghi", -1},
                        {L"1ghi", L"01ghi", 1},
                        {L"1ghi", L"02ghi", -1},
                        {L"01ghi", L"1ghi", -1},
                        {L"1ghi", L"002ghi", -1},
                        {L"002ghi", L"1ghi", 1},
                        {L"abc01def", L"abc1def", -1},
                        {L"abc1def", L"abc01def", 1},
                        {L"abc12", L"abc5", 1},
                        {L"51abc", L"050abc", 1},
                        {L"abc5", L"abc12", -1},
                        {L"5abc", L"12ABC", -1},
                        {L"abc0789", L"abc789", -1},
                        {L"abc0xA789", L"abc0xA0789", 1},
                        {L"abc002", L"abc2", -1},
                        {L"abc002g", L"abc002", 1},
                        {L"abc002g", L"abc02g", -1},
                        {L"abc002.txt", L"abc02.txt", -1},
                        {L"abc005", L"abc012", -1},
                        {L"abc02", L"abc002", 1},
                        {L"abc002.txt", L"abc02.txt", -1},
                        {L"GHI1abc2.txt", L"ghi1abc2.txt", -1},
                        {L"a0", L"a00", -1},
                        {L"a00b", L"a0b", -1},
                        {L"a0b", L"a00b", 1},
                        {NULL, NULL, 0}};

/// Verify the behavior of the `wcsfilecmp()` function.
static void test_wcsfilecmp() {
    for (auto test = wcsfilecmp_tests; test->str1; test++) {
        int rc = wcsfilecmp(test->str1, test->str2);
        if (rc != test->expected_rc) {
            err(L"New failed on line %lu: [\"%ls\" <=> \"%ls\"]: "
                L"expected return code %d but got %d",
                __LINE__, test->str1, test->str2, test->expected_rc, rc);
        }
    }
}

static void test_utility_functions() {
    say(L"Testing utility functions");
    test_wcsfilecmp();
    test_parse_util_cmdsubst_extent();
}

// UTF8 tests taken from Alexey Vatchenko's utf8 library. See http://www.bsdua.org/libbsdua.html.
static void test_utf82wchar(const char *src, size_t slen, const wchar_t *dst, size_t dlen,
                            int flags, size_t res, const char *descr) {
    size_t size;
    wchar_t *mem = NULL;

#if WCHAR_MAX == 0xffff
    // Hack: if wchar is only UCS-2, and the UTF-8 input string contains astral characters, then
    // tweak the expected size to 0.
    if (src) {
        // A UTF-8 code unit may represent an astral code point if it has 4 or more leading 1s.
        const unsigned char astral_mask = 0xF0;
        for (size_t i = 0; i < slen; i++) {
            if ((src[i] & astral_mask) == astral_mask) {
                // Astral char. We want this conversion to fail.
                res = 0;  //!OCLINT(parameter reassignment)
                break;
            }
        }
    }
#endif

    if (!dst) {
        size = utf8_to_wchar(src, slen, NULL, flags);
    } else {
        mem = (wchar_t *)malloc(dlen * sizeof(*mem));
        if (!mem) {
            err(L"u2w: %s: MALLOC FAILED\n", descr);
            return;
        }

        std::wstring buff;
        size = utf8_to_wchar(src, slen, &buff, flags);
        std::copy(buff.begin(), buff.begin() + std::min(dlen, buff.size()), mem);
    }

    if (res != size) {
        err(L"u2w: %s: FAILED (rv: %lu, must be %lu)", descr, size, res);
    } else if (mem && memcmp(mem, dst, size * sizeof(*mem)) != 0) {
        err(L"u2w: %s: BROKEN", descr);
    }

    free(mem);
}

// Annoying variant to handle uchar to avoid narrowing conversion warnings.
static void test_utf82wchar(const unsigned char *usrc, size_t slen, const wchar_t *dst, size_t dlen,
                            int flags, size_t res, const char *descr) {
    const char *src = reinterpret_cast<const char *>(usrc);
    return test_utf82wchar(src, slen, dst, dlen, flags, res, descr);
}

static void test_wchar2utf8(const wchar_t *src, size_t slen, const char *dst, size_t dlen,
                            int flags, size_t res, const char *descr) {
    size_t size;
    char *mem = NULL;

#if WCHAR_MAX == 0xffff
    // Hack: if wchar is simulating UCS-2, and the wchar_t input string contains astral characters,
    // then tweak the expected size to 0.
    if (src) {
        const uint32_t astral_mask = 0xFFFF0000U;
        for (size_t i = 0; i < slen; i++) {
            if ((src[i] & astral_mask) != 0) {
                // Astral char. We want this conversion to fail.
                res = 0;  //!OCLINT(parameter reassignment)
                break;
            }
        }
    }
#endif

    if (dst) {
        mem = (char *)malloc(dlen);
        if (!mem) {
            err(L"w2u: %s: MALLOC FAILED", descr);
            return;
        }
    }

    size = wchar_to_utf8(src, slen, mem, dlen, flags);
    if (res != size) {
        err(L"w2u: %s: FAILED (rv: %lu, must be %lu)", descr, size, res);
    } else if (dst && memcmp(mem, dst, size) != 0) {
        err(L"w2u: %s: BROKEN", descr);
    }

    free(mem);
}

// Annoying variant to handle uchar to avoid narrowing conversion warnings.
static void test_wchar2utf8(const wchar_t *src, size_t slen, const unsigned char *udst, size_t dlen,
                            int flags, size_t res, const char *descr) {
    const char *dst = reinterpret_cast<const char *>(udst);
    return test_wchar2utf8(src, slen, dst, dlen, flags, res, descr);
}

static void test_utf8() {
    say(L"Testing utf8");
    wchar_t w1[] = {0x54, 0x65, 0x73, 0x74};
    wchar_t w2[] = {0x0422, 0x0435, 0x0441, 0x0442};
    wchar_t w3[] = {0x800, 0x1e80, 0x98c4, 0x9910, 0xff00};
    wchar_t wm[] = {0x41, 0x0441, 0x3042, 0xff67, 0x9b0d};
    wchar_t wb2[] = {0xd800, 0xda00, 0x41, 0xdfff, 0x0a};
    wchar_t wbom[] = {0xfeff, 0x41, 0x0a};
    wchar_t wbom2[] = {0x41, 0xa};
    wchar_t wbom22[] = {0xfeff, 0x41, 0x0a};
    unsigned char u1[] = {0x54, 0x65, 0x73, 0x74};
    unsigned char u2[] = {0xd0, 0xa2, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82};
    unsigned char u3[] = {0xe0, 0xa0, 0x80, 0xe1, 0xba, 0x80, 0xe9, 0xa3,
                          0x84, 0xe9, 0xa4, 0x90, 0xef, 0xbc, 0x80};
    unsigned char um[] = {0x41, 0xd1, 0x81, 0xe3, 0x81, 0x82, 0xef, 0xbd, 0xa7, 0xe9, 0xac, 0x8d};
    unsigned char uc080[] = {0xc0, 0x80};
    unsigned char ub2[] = {0xed, 0xa1, 0x8c, 0xed, 0xbe, 0xb4, 0x0a};
    unsigned char ubom[] = {0x41, 0xa};
    unsigned char ubom2[] = {0xef, 0xbb, 0xbf, 0x41, 0x0a};
#if WCHAR_MAX != 0xffff
    wchar_t w4[] = {0x15555, 0xf7777, 0x0a};
    wchar_t wb[] = {(wchar_t)-2, 0xa, (wchar_t)0xffffffff, 0x0441};
    wchar_t wb1[] = {0x0a, 0x0422};
    unsigned char u4[] = {0xf0, 0x95, 0x95, 0x95, 0xf3, 0xb7, 0x9d, 0xb7, 0x0a};
    unsigned char ub[] = {0xa, 0xd1, 0x81};
    unsigned char ub1[] = {0xa, 0xff, 0xd0, 0xa2, 0xfe, 0x8f, 0xe0, 0x80};
#endif

    // UTF-8 -> UCS-4 string.
    test_utf82wchar(ubom2, sizeof(ubom2), wbom2, sizeof(wbom2) / sizeof(*wbom2), UTF8_SKIP_BOM,
                    sizeof(wbom2) / sizeof(*wbom2), "ubom2 skip BOM");
    test_utf82wchar(ubom2, sizeof(ubom2), wbom22, sizeof(wbom22) / sizeof(*wbom22), 0,
                    sizeof(wbom22) / sizeof(*wbom22), "ubom2 BOM");
    test_utf82wchar(uc080, sizeof(uc080), NULL, 0, 0, 0, "uc080 c0 80 - forbitten by rfc3629");
    test_utf82wchar(ub2, sizeof(ub2), NULL, 0, 0, 3, "ub2 resulted in forbitten wchars (len)");
    test_utf82wchar(ub2, sizeof(ub2), wb2, sizeof(wb2) / sizeof(*wb2), 0, 0,
                    "ub2 resulted in forbitten wchars");
    test_utf82wchar(ub2, sizeof(ub2), L"\x0a", 1, UTF8_IGNORE_ERROR, 1,
                    "ub2 resulted in ignored forbitten wchars");
    test_utf82wchar(u1, sizeof(u1), w1, sizeof(w1) / sizeof(*w1), 0, sizeof(w1) / sizeof(*w1),
                    "u1/w1 1 octet chars");
    test_utf82wchar(u2, sizeof(u2), w2, sizeof(w2) / sizeof(*w2), 0, sizeof(w2) / sizeof(*w2),
                    "u2/w2 2 octets chars");
    test_utf82wchar(u3, sizeof(u3), w3, sizeof(w3) / sizeof(*w3), 0, sizeof(w3) / sizeof(*w3),
                    "u3/w3 3 octets chars");
    test_utf82wchar("\xff", 1, NULL, 0, 0, 0, "broken utf-8 0xff symbol");
    test_utf82wchar("\xfe", 1, NULL, 0, 0, 0, "broken utf-8 0xfe symbol");
    test_utf82wchar("\x8f", 1, NULL, 0, 0, 0, "broken utf-8, start from 10 higher bits");
    test_utf82wchar((const char *)NULL, 0, NULL, 0, 0, 0, "invalid params, all 0");
    test_utf82wchar(u1, 0, NULL, 0, 0, 0, "invalid params, src buf not NULL");
    test_utf82wchar((const char *)NULL, 10, NULL, 0, 0, 0, "invalid params, src length is not 0");

    // UCS-4 -> UTF-8 string.
    const char *const nullc = NULL;
    test_wchar2utf8(wbom, sizeof(wbom) / sizeof(*wbom), ubom, sizeof(ubom), UTF8_SKIP_BOM,
                    sizeof(ubom), "BOM");
    test_wchar2utf8(wb2, sizeof(wb2) / sizeof(*wb2), nullc, 0, 0, 0, "prohibited wchars");
    test_wchar2utf8(wb2, sizeof(wb2) / sizeof(*wb2), nullc, 0, UTF8_IGNORE_ERROR, 2,
                    "ignore prohibited wchars");
    test_wchar2utf8(w1, sizeof(w1) / sizeof(*w1), u1, sizeof(u1), 0, sizeof(u1),
                    "w1/u1 1 octet chars");
    test_wchar2utf8(w2, sizeof(w2) / sizeof(*w2), u2, sizeof(u2), 0, sizeof(u2),
                    "w2/u2 2 octets chars");
    test_wchar2utf8(w3, sizeof(w3) / sizeof(*w3), u3, sizeof(u3), 0, sizeof(u3),
                    "w3/u3 3 octets chars");
    test_wchar2utf8(NULL, 0, nullc, 0, 0, 0, "invalid params, all 0");
    test_wchar2utf8(w1, 0, nullc, 0, 0, 0, "invalid params, src buf not NULL");
    test_wchar2utf8(w1, sizeof(w1) / sizeof(*w1), u1, 0, 0, 0, "invalid params, dst is not NULL");
    test_wchar2utf8(NULL, 10, nullc, 0, 0, 0, "invalid params, src length is not 0");

    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), um, sizeof(um), 0, sizeof(um),
                    "wm/um mixed languages");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), um, sizeof(um) - 1, 0, 0, "wm/um boundaries -1");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), um, sizeof(um) + 1, 0, sizeof(um),
                    "wm/um boundaries +1");
    test_wchar2utf8(wm, sizeof(wm) / sizeof(*wm), nullc, 0, 0, sizeof(um),
                    "wm/um calculate length");
    test_utf82wchar(um, sizeof(um), wm, sizeof(wm) / sizeof(*wm), 0, sizeof(wm) / sizeof(*wm),
                    "um/wm mixed languages");
    test_utf82wchar(um, sizeof(um), wm, sizeof(wm) / sizeof(*wm) + 1, 0, sizeof(wm) / sizeof(*wm),
                    "um/wm boundaries +1");
    test_utf82wchar(um, sizeof(um), NULL, 0, 0, sizeof(wm) / sizeof(*wm), "um/wm calculate length");

// The following tests won't pass on systems (e.g., Cygwin) where sizeof wchar_t is 2. That's
// due to several reasons but the primary one is that narrowing conversions of literals assigned
// to the wchar_t arrays above don't result in values that will be treated as errors by the
// conversion functions.
#if WCHAR_MAX != 0xffff
    test_utf82wchar(u4, sizeof(u4), w4, sizeof(w4) / sizeof(*w4), 0, sizeof(w4) / sizeof(*w4),
                    "u4/w4 4 octets chars");
    test_wchar2utf8(w4, sizeof(w4) / sizeof(*w4), u4, sizeof(u4), 0, sizeof(u4),
                    "w4/u4 4 octets chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), ub, sizeof(ub), 0, 0, "wb/ub bad chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), ub, sizeof(ub), UTF8_IGNORE_ERROR, sizeof(ub),
                    "wb/ub ignore bad chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), nullc, 0, 0, 0,
                    "wb calculate length of bad chars");
    test_wchar2utf8(wb, sizeof(wb) / sizeof(*wb), nullc, 0, UTF8_IGNORE_ERROR, sizeof(ub),
                    "calculate length, ignore bad chars");
    test_utf82wchar(ub1, sizeof(ub1), wb1, sizeof(wb1) / sizeof(*wb1), UTF8_IGNORE_ERROR,
                    sizeof(wb1) / sizeof(*wb1), "ub1/wb1 ignore bad chars");
    test_utf82wchar(ub1, sizeof(ub1), NULL, 0, 0, 0, "ub1 calculate length of bad chars");
    test_utf82wchar(ub1, sizeof(ub1), NULL, 0, UTF8_IGNORE_ERROR, sizeof(wb1) / sizeof(*wb1),
                    "ub1 calculate length, ignore bad chars");
#endif
}

static void test_feature_flags() {
    say(L"Testing future feature flags");
    using ft = features_t;
    ft f;
    do_test(!f.test(ft::stderr_nocaret));
    f.set(ft::stderr_nocaret, true);
    do_test(f.test(ft::stderr_nocaret));
    f.set(ft::stderr_nocaret, false);
    do_test(!f.test(ft::stderr_nocaret));

    f.set_from_string(L"stderr-nocaret,nonsense");
    do_test(f.test(ft::stderr_nocaret));
    f.set_from_string(L"stderr-nocaret,no-stderr-nocaret,nonsense");
    do_test(!f.test(ft::stderr_nocaret));

    // Ensure every metadata is represented once.
    size_t counts[ft::flag_count] = {};
    for (const auto &md : ft::metadata) {
        counts[md.flag]++;
    }
    for (size_t c : counts) {
        do_test(c == 1);
    }
    do_test(ft::metadata[ft::stderr_nocaret].name == wcstring(L"stderr-nocaret"));
    do_test(ft::metadata_for(L"stderr-nocaret") == &ft::metadata[ft::stderr_nocaret]);
    do_test(ft::metadata_for(L"not-a-flag") == nullptr);
}

static void test_escape_sequences() {
    say(L"Testing escape_sequences");
    if (escape_code_length(L"") != 0) err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"abcd") != 0)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B[2J") != 4)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B[38;5;123mABC") != strlen("\x1B[38;5;123m"))
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B@") != 2)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);

    // iTerm2 escape sequences.
    if (escape_code_length(L"\x1B]50;CurrentDir=test/foo\x07NOT_PART_OF_SEQUENCE") != 25)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B]50;SetMark\x07NOT_PART_OF_SEQUENCE") != 13)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B]6;1;bg;red;brightness;255\x07NOT_PART_OF_SEQUENCE") != 28)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B]Pg4040ff\x1B\\NOT_PART_OF_SEQUENCE") != 12)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B]blahblahblah\x1B\\") != 16)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
    if (escape_code_length(L"\x1B]blahblahblah\x07") != 15)
        err(L"test_escape_sequences failed on line %d\n", __LINE__);
}

class test_lru_t : public lru_cache_t<test_lru_t, int> {
   public:
    static constexpr size_t test_capacity = 16;
    typedef std::pair<wcstring, int> value_type;

    test_lru_t() : lru_cache_t<test_lru_t, int>(test_capacity) {}

    std::vector<value_type> evicted;

    void entry_was_evicted(const wcstring &key, int val) { evicted.push_back({key, val}); }

    std::vector<value_type> values() const {
        std::vector<value_type> result;
        for (const auto &p : *this) {
            result.push_back(p);
        }
        return result;
    }

    std::vector<int> ints() const {
        std::vector<int> result;
        for (const auto &p : *this) {
            result.push_back(p.second);
        }
        return result;
    }
};

static void test_lru() {
    say(L"Testing LRU cache");

    test_lru_t cache;
    std::vector<std::pair<wcstring, int>> expected_evicted;
    std::vector<std::pair<wcstring, int>> expected_values;
    int total_nodes = 20;
    for (int i = 0; i < total_nodes; i++) {
        do_test(cache.size() == size_t(std::min(i, 16)));
        do_test(cache.values() == expected_values);
        if (i < 4) expected_evicted.push_back({to_string(i), i});
        // Adding the node the first time should work, and subsequent times should fail.
        do_test(cache.insert(to_string(i), i));
        do_test(!cache.insert(to_string(i), i + 1));

        expected_values.push_back({to_string(i), i});
        while (expected_values.size() > test_lru_t::test_capacity) {
            expected_values.erase(expected_values.begin());
        }
        cache.check_sanity();
    }
    do_test(cache.evicted == expected_evicted);
    do_test(cache.values() == expected_values);
    cache.check_sanity();

    // Stable-sort ints in reverse order
    // This a/2 check ensures that some different ints compare the same
    // It also gives us a different order than we started with
    auto comparer = [](int a, int b) { return a / 2 > b / 2; };
    std::vector<int> ints = cache.ints();
    std::stable_sort(ints.begin(), ints.end(), comparer);

    cache.stable_sort(comparer);
    std::vector<int> new_ints = cache.ints();
    if (new_ints != ints) {
        auto commajoin = [](const std::vector<int> &vs) {
            wcstring ret;
            for (int v : vs) {
                append_format(ret, L"%d,", v);
            }
            if (!ret.empty()) ret.pop_back();
            return ret;
        };
        err(L"LRU stable sort failed. Expected %ls, got %ls\n", commajoin(new_ints).c_str(),
            commajoin(ints).c_str());
    }

    cache.evict_all_nodes();
    do_test(cache.evicted.size() == size_t(total_nodes));
}

/// Perform parameter expansion and test if the output equals the zero-terminated parameter list
/// supplied.
///
/// \param in the string to expand
/// \param flags the flags to send to expand_string
/// \param ... A zero-terminated parameter list of values to test.
/// After the zero terminator comes one more arg, a string, which is the error
/// message to print if the test fails.
static bool expand_test(const wchar_t *in, expand_flags_t flags, ...) {
    std::vector<completion_t> output;
    va_list va;
    bool res = true;
    wchar_t *arg;
    parse_error_list_t errors;

    if (expand_string(in, &output, flags, &errors) == EXPAND_ERROR) {
        if (errors.empty()) {
            err(L"Bug: Parse error reported but no error text found.");
        } else {
            err(L"%ls", errors.at(0).describe(wcstring(in)).c_str());
        }
        return false;
    }

    wcstring_list_t expected;

    va_start(va, flags);
    while ((arg = va_arg(va, wchar_t *)) != NULL) {
        expected.push_back(wcstring(arg));
    }
    va_end(va);

    std::set<wcstring> remaining(expected.begin(), expected.end());
    std::vector<completion_t>::const_iterator out_it = output.begin(), out_end = output.end();
    for (; out_it != out_end; ++out_it) {
        if (!remaining.erase(out_it->completion)) {
            res = false;
            break;
        }
    }
    if (!remaining.empty()) {
        res = false;
    }

    if (!res) {
        arg = va_arg(va, wchar_t *);
        if (arg) {
            wcstring msg = L"Expected [";
            bool first = true;
            for (const wcstring &exp : expected) {
                if (!first) msg += L", ";
                first = false;
                msg += '"';
                msg += exp;
                msg += '"';
            }
            msg += L"], found [";
            first = true;
            for (std::vector<completion_t>::const_iterator it = output.begin(), end = output.end();
                 it != end; ++it) {
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

/// Test globbing and other parameter expansion.
static void test_expand() {
    say(L"Testing parameter expansion");

    expand_test(L"foo", 0, L"foo", 0, L"Strings do not expand to themselves");
    expand_test(L"a{b,c,d}e", 0, L"abe", L"ace", L"ade", 0, L"Bracket expansion is broken");
    expand_test(L"a*", EXPAND_SKIP_WILDCARDS, L"a*", 0, L"Cannot skip wildcard expansion");
    expand_test(L"/bin/l\\0", EXPAND_FOR_COMPLETIONS, 0,
                L"Failed to handle null escape in expansion");
    expand_test(L"foo\\$bar", EXPAND_SKIP_VARIABLES, L"foo$bar", 0,
                L"Failed to handle dollar sign in variable-skipping expansion");

    // bb
    //    x
    // bar
    // baz
    //    xxx
    //    yyy
    // bax
    //    xxx
    // lol
    //    nub
    //       q
    // .foo
    // aaa
    // aaa2
    //    x
    if (system("mkdir -p test/fish_expand_test/")) err(L"mkdir failed");
    if (system("mkdir -p test/fish_expand_test/bb/")) err(L"mkdir failed");
    if (system("mkdir -p test/fish_expand_test/baz/")) err(L"mkdir failed");
    if (system("mkdir -p test/fish_expand_test/bax/")) err(L"mkdir failed");
    if (system("mkdir -p test/fish_expand_test/lol/nub/")) err(L"mkdir failed");
    if (system("mkdir -p test/fish_expand_test/aaa/")) err(L"mkdir failed");
    if (system("mkdir -p test/fish_expand_test/aaa2/")) err(L"mkdir failed");
    if (system("touch test/fish_expand_test/.foo")) err(L"touch failed");
    if (system("touch test/fish_expand_test/bb/x")) err(L"touch failed");
    if (system("touch test/fish_expand_test/bar")) err(L"touch failed");
    if (system("touch test/fish_expand_test/bax/xxx")) err(L"touch failed");
    if (system("touch test/fish_expand_test/baz/xxx")) err(L"touch failed");
    if (system("touch test/fish_expand_test/baz/yyy")) err(L"touch failed");
    if (system("touch test/fish_expand_test/lol/nub/q")) err(L"touch failed");
    if (system("touch test/fish_expand_test/aaa2/x")) err(L"touch failed");

    // This is checking that .* does NOT match . and ..
    // (https://github.com/fish-shell/fish-shell/issues/270). But it does have to match literal
    // components (e.g. "./*" has to match the same as "*".
    const wchar_t *const wnull = NULL;
    expand_test(L"test/fish_expand_test/.*", 0, L"test/fish_expand_test/.foo", wnull,
                L"Expansion not correctly handling dotfiles");

    expand_test(L"test/fish_expand_test/./.*", 0, L"test/fish_expand_test/./.foo", wnull,
                L"Expansion not correctly handling literal path components in dotfiles");

    expand_test(L"test/fish_expand_test/*/xxx", 0, L"test/fish_expand_test/bax/xxx",
                L"test/fish_expand_test/baz/xxx", wnull, L"Glob did the wrong thing 1");

    expand_test(L"test/fish_expand_test/*z/xxx", 0, L"test/fish_expand_test/baz/xxx", wnull,
                L"Glob did the wrong thing 2");

    expand_test(L"test/fish_expand_test/**z/xxx", 0, L"test/fish_expand_test/baz/xxx", wnull,
                L"Glob did the wrong thing 3");

    expand_test(L"test/fish_expand_test////baz/xxx", 0, L"test/fish_expand_test////baz/xxx", wnull,
                L"Glob did the wrong thing 3");

    expand_test(L"test/fish_expand_test/b**", 0, L"test/fish_expand_test/bb",
                L"test/fish_expand_test/bb/x", L"test/fish_expand_test/bar",
                L"test/fish_expand_test/bax", L"test/fish_expand_test/bax/xxx",
                L"test/fish_expand_test/baz", L"test/fish_expand_test/baz/xxx",
                L"test/fish_expand_test/baz/yyy", wnull, L"Glob did the wrong thing 4");

    // A trailing slash should only produce directories.
    expand_test(L"test/fish_expand_test/b*/", 0, L"test/fish_expand_test/bb/",
                L"test/fish_expand_test/baz/", L"test/fish_expand_test/bax/", wnull,
                L"Glob did the wrong thing 5");

    expand_test(L"test/fish_expand_test/b**/", 0, L"test/fish_expand_test/bb/",
                L"test/fish_expand_test/baz/", L"test/fish_expand_test/bax/", wnull,
                L"Glob did the wrong thing 6");

    expand_test(L"test/fish_expand_test/**/q", 0, L"test/fish_expand_test/lol/nub/q", wnull,
                L"Glob did the wrong thing 7");

    expand_test(L"test/fish_expand_test/BA", EXPAND_FOR_COMPLETIONS, L"test/fish_expand_test/bar",
                L"test/fish_expand_test/bax/", L"test/fish_expand_test/baz/", wnull,
                L"Case insensitive test did the wrong thing");

    expand_test(L"test/fish_expand_test/BA", EXPAND_FOR_COMPLETIONS, L"test/fish_expand_test/bar",
                L"test/fish_expand_test/bax/", L"test/fish_expand_test/baz/", wnull,
                L"Case insensitive test did the wrong thing");

    expand_test(L"test/fish_expand_test/bb/yyy", EXPAND_FOR_COMPLETIONS,
                /* nothing! */ wnull, L"Wrong fuzzy matching 1");

    expand_test(L"test/fish_expand_test/bb/x", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH, L"",
                wnull,  // we just expect the empty string since this is an exact match
                L"Wrong fuzzy matching 2");

    // Some vswprintfs refuse to append ANY_STRING in a format specifiers, so don't use
    // format_string here.
    const wcstring any_str_str(1, ANY_STRING);
    expand_test(L"test/fish_expand_test/b/xx*", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                (L"test/fish_expand_test/bax/xx" + any_str_str).c_str(),
                (L"test/fish_expand_test/baz/xx" + any_str_str).c_str(), wnull,
                L"Wrong fuzzy matching 3");

    expand_test(L"test/fish_expand_test/b/yyy", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                L"test/fish_expand_test/baz/yyy", wnull, L"Wrong fuzzy matching 4");

    expand_test(L"test/fish_expand_test/aa/x", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH,
                L"test/fish_expand_test/aaa2/x", wnull, L"Wrong fuzzy matching 5");

    expand_test(L"test/fish_expand_test/aaa/x", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH, wnull,
                L"Wrong fuzzy matching 6 - shouldn't remove valid directory names (#3211)");

    if (!expand_test(L"test/fish_expand_test/.*", 0, L"test/fish_expand_test/.foo", 0)) {
        err(L"Expansion not correctly handling dotfiles");
    }
    if (!expand_test(L"test/fish_expand_test/./.*", 0, L"test/fish_expand_test/./.foo", 0)) {
        err(L"Expansion not correctly handling literal path components in dotfiles");
    }

    if (!pushd("test/fish_expand_test")) return;

    expand_test(L"b/xx", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH, L"bax/xxx", L"baz/xxx", wnull,
                L"Wrong fuzzy matching 5");

    // multiple slashes with fuzzy matching - #3185
    expand_test(L"l///n", EXPAND_FOR_COMPLETIONS | EXPAND_FUZZY_MATCH, L"lol///nub/", wnull,
                L"Wrong fuzzy matching 6");

    popd();
}

static void test_fuzzy_match() {
    say(L"Testing fuzzy string matching");

    if (string_fuzzy_match_string(L"", L"").type != fuzzy_match_exact)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"alpha", L"alpha").type != fuzzy_match_exact)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"alp", L"alpha").type != fuzzy_match_prefix)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"ALPHA!", L"alPhA!").type != fuzzy_match_case_insensitive)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"alPh", L"ALPHA!").type != fuzzy_match_prefix_case_insensitive)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"LPH", L"ALPHA!").type != fuzzy_match_substring)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"AA", L"ALPHA!").type != fuzzy_match_subsequence_insertions_only)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
    if (string_fuzzy_match_string(L"BB", L"ALPHA!").type != fuzzy_match_none)
        err(L"test_fuzzy_match failed on line %ld", __LINE__);
}

static void test_abbreviations() {
    say(L"Testing abbreviations");
    env_push(true);

    const std::vector<std::pair<const wcstring, const wcstring>> abbreviations = {
        {L"gc", L"git checkout"},
        {L"foo", L"bar"},
        {L"gx", L"git checkout"},
    };
    for (auto it : abbreviations) {
        int ret = env_set_one(L"_fish_abbr_" + it.first, ENV_LOCAL, it.second);
        if (ret != 0) err(L"Unable to set abbreviation variable");
    }

    wcstring result;
    if (expand_abbreviation(L"", &result)) err(L"Unexpected success with empty abbreviation");
    if (expand_abbreviation(L"nothing", &result))
        err(L"Unexpected success with missing abbreviation");

    if (!expand_abbreviation(L"gc", &result)) err(L"Unexpected failure with gc abbreviation");
    if (result != L"git checkout") err(L"Wrong abbreviation result for gc");
    result.clear();

    if (!expand_abbreviation(L"foo", &result)) err(L"Unexpected failure with foo abbreviation");
    if (result != L"bar") err(L"Wrong abbreviation result for foo");

    bool expanded;
    expanded = reader_expand_abbreviation_in_command(L"just a command", 3, &result);
    if (expanded) err(L"Command wrongly expanded on line %ld", (long)__LINE__);
    expanded = reader_expand_abbreviation_in_command(L"gc somebranch", 0, &result);
    if (!expanded) err(L"Command not expanded on line %ld", (long)__LINE__);

    expanded = reader_expand_abbreviation_in_command(L"gc somebranch", wcslen(L"gc"), &result);
    if (!expanded) err(L"gc not expanded");
    if (result != L"git checkout somebranch")
        err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    // Space separation.
    expanded = reader_expand_abbreviation_in_command(L"gx somebranch", wcslen(L"gc"), &result);
    if (!expanded) err(L"gx not expanded");
    if (result != L"git checkout somebranch")
        err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    expanded = reader_expand_abbreviation_in_command(L"echo hi ; gc somebranch",
                                                     wcslen(L"echo hi ; g"), &result);
    if (!expanded) err(L"gc not expanded on line %ld", (long)__LINE__);
    if (result != L"echo hi ; git checkout somebranch")
        err(L"gc incorrectly expanded on line %ld", (long)__LINE__);

    expanded = reader_expand_abbreviation_in_command(
        L"echo (echo (echo (echo (gc ", wcslen(L"echo (echo (echo (echo (gc"), &result);
    if (!expanded) err(L"gc not expanded on line %ld", (long)__LINE__);
    if (result != L"echo (echo (echo (echo (git checkout ")
        err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    // If commands should be expanded.
    expanded = reader_expand_abbreviation_in_command(L"if gc", wcslen(L"if gc"), &result);
    if (!expanded) err(L"gc not expanded on line %ld", (long)__LINE__);
    if (result != L"if git checkout")
        err(L"gc incorrectly expanded on line %ld to '%ls'", (long)__LINE__, result.c_str());

    // Others should not be.
    expanded = reader_expand_abbreviation_in_command(L"of gc", wcslen(L"of gc"), &result);
    if (expanded) err(L"gc incorrectly expanded on line %ld", (long)__LINE__);

    // Others should not be.
    expanded = reader_expand_abbreviation_in_command(L"command gc", wcslen(L"command gc"), &result);
    if (expanded) err(L"gc incorrectly expanded on line %ld", (long)__LINE__);

    env_pop();
}

/// Test path functions.
static void test_path() {
    say(L"Testing path functions");

    wcstring path = L"//foo//////bar/";
    path_make_canonical(path);
    if (path != L"/foo/bar") {
        err(L"Bug in canonical PATH code");
    }

    path = L"/";
    path_make_canonical(path);
    if (path != L"/") {
        err(L"Bug in canonical PATH code");
    }

    if (paths_are_equivalent(L"/foo/bar/baz", L"foo/bar/baz"))
        err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
    if (!paths_are_equivalent(L"///foo///bar/baz", L"/foo/bar////baz//"))
        err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
    if (!paths_are_equivalent(L"/foo/bar/baz", L"/foo/bar/baz"))
        err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
    if (!paths_are_equivalent(L"/", L"/"))
        err(L"Bug in canonical PATH code on line %ld", (long)__LINE__);
}

static void test_pager_navigation() {
    say(L"Testing pager navigation");

    // Generate 19 strings of width 10. There's 2 spaces between completions, and our term size is
    // 80; these can therefore fit into 6 columns (6 * 12 - 2 = 70) or 5 columns (58) but not 7
    // columns (7 * 12 - 2 = 82).
    //
    // You can simulate this test by creating 19 files named "file00.txt" through "file_18.txt".
    completion_list_t completions;
    for (size_t i = 0; i < 19; i++) {
        append_completion(&completions, L"abcdefghij");
    }

    pager_t pager;
    pager.set_completions(completions);
    pager.set_term_size(80, 24);
    page_rendering_t render = pager.render();

    if (render.term_width != 80) err(L"Wrong term width");
    if (render.term_height != 24) err(L"Wrong term height");

    size_t rows = 4, cols = 5;

    // We have 19 completions. We can fit into 6 columns with 4 rows or 5 columns with 4 rows; the
    // second one is better and so is what we ought to have picked.
    if (render.rows != rows) err(L"Wrong row count");
    if (render.cols != cols) err(L"Wrong column count");

    // Initially expect to have no completion index.
    if (render.selected_completion_idx != (size_t)(-1)) {
        err(L"Wrong initial selection");
    }

    // Here are navigation directions and where we expect the selection to be.
    const struct {
        selection_direction_t dir;
        size_t sel;
    } cmds[] = {
        // Tab completion to get into the list.
        {direction_next, 0},

        // Westward motion in upper left goes to the last filled column in the last row.
        {direction_west, 15},
        // East goes back.
        {direction_east, 0},

        // "Next" motion goes down the column.
        {direction_next, 1},
        {direction_next, 2},

        {direction_west, 17},
        {direction_east, 2},
        {direction_east, 6},
        {direction_east, 10},
        {direction_east, 14},
        {direction_east, 18},

        {direction_west, 14},
        {direction_east, 18},

        // Eastward motion wraps back to the upper left, westward goes to the prior column.
        {direction_east, 3},
        {direction_east, 7},
        {direction_east, 11},
        {direction_east, 15},

        // Pages.
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
    for (size_t i = 0; i < sizeof cmds / sizeof *cmds; i++) {
        pager.select_next_completion_in_direction(cmds[i].dir, render);
        pager.update_rendering(&render);
        if (cmds[i].sel != render.selected_completion_idx) {
            err(L"For command %lu, expected selection %lu, but found instead %lu\n", i, cmds[i].sel,
                render.selected_completion_idx);
        }
    }
}

struct pager_layout_testcase_t {
    size_t width;
    const wchar_t *expected;

    // Run ourselves as a test case.
    // Set our data on the pager, and then check the rendering.
    // We should have one line, and it should have our expected text.
    void run(pager_t &pager) const {
        pager.set_term_size(this->width, 24);
        page_rendering_t rendering = pager.render();
        const screen_data_t &sd = rendering.screen_data;
        do_test(sd.line_count() == 1);
        if (sd.line_count() > 0) {
            wcstring expected = this->expected;

            // hack: handle the case where ellipsis is not L'\x2026'
            if (ellipsis_char != L'\x2026') {
                std::replace(expected.begin(), expected.end(), L'\x2026', ellipsis_char);
            }

            wcstring text = sd.line(0).to_string();
            if (text != expected) {
                fwprintf(stderr, L"width %zu got %zu<%ls>, expected %zu<%ls>\n", this->width,
                         text.length(), text.c_str(), expected.length(), expected.c_str());
                for (size_t i = 0; i < std::max(text.length(), expected.length()); i++) {
                    fwprintf(stderr, L"i %zu got <%lx> expected <%lx>\n", i,
                             i >= text.length() ? 0xffff : text[i],
                             i >= expected.length() ? 0xffff : expected[i]);
                }
            }
            do_test(text == expected);
        }
    }
};

static void test_pager_layout() {
    // These tests are woefully incomplete
    // They only test the truncation logic for a single completion
    say(L"Testing pager layout");
    pager_t pager;

    // These test cases have equal completions and descriptions
    const completion_t c1(L"abcdefghij", L"1234567890");
    pager.set_completions(completion_list_t(1, c1));
    const pager_layout_testcase_t testcases1[] = {
        {26, L"abcdefghij  (1234567890)"}, {25, L"abcdefghij  (1234567890)"},
        {24, L"abcdefghij  (1234567890)"}, {23, L"abcdefghij  (12345678…)"},
        {22, L"abcdefghij  (1234567…)"},   {21, L"abcdefghij  (123456…)"},
        {20, L"abcdefghij  (12345…)"},     {19, L"abcdefghij  (1234…)"},
        {18, L"abcdefgh…  (1234…)"},       {17, L"abcdefg…  (1234…)"},
        {16, L"abcdefg…  (123…)"},         {0, NULL}  // sentinel terminator
    };
    for (size_t i = 0; testcases1[i].expected != NULL; i++) {
        testcases1[i].run(pager);
    }

    // These test cases have heavyweight completions
    const completion_t c2(L"abcdefghijklmnopqrs", L"1");
    pager.set_completions(completion_list_t(1, c2));
    const pager_layout_testcase_t testcases2[] = {
        {26, L"abcdefghijklmnopqrs  (1)"}, {25, L"abcdefghijklmnopqrs  (1)"},
        {24, L"abcdefghijklmnopqrs  (1)"}, {23, L"abcdefghijklmnopq…  (1)"},
        {22, L"abcdefghijklmnop…  (1)"},   {21, L"abcdefghijklmno…  (1)"},
        {20, L"abcdefghijklmn…  (1)"},     {19, L"abcdefghijklm…  (1)"},
        {18, L"abcdefghijkl…  (1)"},       {17, L"abcdefghijk…  (1)"},
        {16, L"abcdefghij…  (1)"},         {0, NULL}  // sentinel terminator
    };
    for (size_t i = 0; testcases2[i].expected != NULL; i++) {
        testcases2[i].run(pager);
    }

    // These test cases have no descriptions
    const completion_t c3(L"abcdefghijklmnopqrst", L"");
    pager.set_completions(completion_list_t(1, c3));
    const pager_layout_testcase_t testcases3[] = {
        {26, L"abcdefghijklmnopqrst"}, {25, L"abcdefghijklmnopqrst"},
        {24, L"abcdefghijklmnopqrst"}, {23, L"abcdefghijklmnopqrst"},
        {22, L"abcdefghijklmnopqrst"}, {21, L"abcdefghijklmnopqrst"},
        {20, L"abcdefghijklmnopqrst"}, {19, L"abcdefghijklmnopqr…"},
        {18, L"abcdefghijklmnopq…"},   {17, L"abcdefghijklmnop…"},
        {16, L"abcdefghijklmno…"},     {0, NULL}  // sentinel terminator
    };
    for (size_t i = 0; testcases3[i].expected != NULL; i++) {
        testcases3[i].run(pager);
    }
}

enum word_motion_t { word_motion_left, word_motion_right };
static void test_1_word_motion(word_motion_t motion, move_word_style_t style,
                               const wcstring &test) {
    wcstring command;
    std::set<size_t> stops;

    // Carets represent stops and should be cut out of the command.
    for (size_t i = 0; i < test.size(); i++) {
        wchar_t wc = test.at(i);
        if (wc == L'^') {
            stops.insert(command.size());
        } else {
            command.push_back(wc);
        }
    }

    size_t idx, end;
    if (motion == word_motion_left) {
        idx = command.size();
        end = 0;
    } else {
        idx = 0;
        end = command.size();
    }

    move_word_state_machine_t sm(style);
    while (idx != end) {
        size_t char_idx = (motion == word_motion_left ? idx - 1 : idx);
        wchar_t wc = command.at(char_idx);
        bool will_stop = !sm.consume_char(wc);
        // fwprintf(stdout, L"idx %lu, looking at %lu (%c): %d\n", idx, char_idx, (char)wc,
        //          will_stop);
        bool expected_stop = (stops.count(idx) > 0);
        if (will_stop != expected_stop) {
            wcstring tmp = command;
            tmp.insert(idx, L"^");
            const char *dir = (motion == word_motion_left ? "left" : "right");
            if (will_stop) {
                err(L"Word motion: moving %s, unexpected stop at idx %lu: '%ls'", dir, idx,
                    tmp.c_str());
            } else if (!will_stop && expected_stop) {
                err(L"Word motion: moving %s, should have stopped at idx %lu: '%ls'", dir, idx,
                    tmp.c_str());
            }
        }
        // We don't expect to stop here next time.
        if (expected_stop) {
            stops.erase(idx);
        }
        if (will_stop) {
            sm.reset();
        } else {
            idx += (motion == word_motion_left ? -1 : 1);
        }
    }
}

/// Test word motion (forward-word, etc.). Carets represent cursor stops.
static void test_word_motion() {
    say(L"Testing word motion");
    test_1_word_motion(word_motion_left, move_word_style_punctuation, L"^echo ^hello_^world.^txt");
    test_1_word_motion(word_motion_right, move_word_style_punctuation, L"echo^ hello^_world^.txt^");

    test_1_word_motion(word_motion_left, move_word_style_punctuation,
                       L"echo ^foo_^foo_^foo/^/^/^/^/^    ");
    test_1_word_motion(word_motion_right, move_word_style_punctuation,
                       L"echo^ foo^_foo^_foo^/^/^/^/^/    ^");

    test_1_word_motion(word_motion_left, move_word_style_path_components, L"^/^foo/^bar/^baz/");
    test_1_word_motion(word_motion_left, move_word_style_path_components, L"^echo ^--foo ^--bar");
    test_1_word_motion(word_motion_left, move_word_style_path_components,
                       L"^echo ^hi ^> /^dev/^null");

    test_1_word_motion(word_motion_left, move_word_style_path_components,
                       L"^echo /^foo/^bar{^aaa,^bbb,^ccc}^bak/");
    test_1_word_motion(word_motion_right, move_word_style_punctuation, L"a ^bcd^");
}

/// Test is_potential_path.
static void test_is_potential_path() {
    say(L"Testing is_potential_path");

    // Directories
    if (system("mkdir -p test/is_potential_path_test/alpha/")) err(L"mkdir failed");
    if (system("mkdir -p test/is_potential_path_test/beta/")) err(L"mkdir failed");

    // Files
    if (system("touch test/is_potential_path_test/aardvark")) err(L"touch failed");
    if (system("touch test/is_potential_path_test/gamma")) err(L"touch failed");

    const wcstring wd = L"test/is_potential_path_test/";
    const wcstring_list_t wds({L".", wd});

    do_test(is_potential_path(L"al", wds, PATH_REQUIRE_DIR));
    do_test(is_potential_path(L"alpha/", wds, PATH_REQUIRE_DIR));
    do_test(is_potential_path(L"aard", wds, 0));

    do_test(!is_potential_path(L"balpha/", wds, PATH_REQUIRE_DIR));
    do_test(!is_potential_path(L"aard", wds, PATH_REQUIRE_DIR));
    do_test(!is_potential_path(L"aarde", wds, PATH_REQUIRE_DIR));
    do_test(!is_potential_path(L"aarde", wds, 0));

    do_test(is_potential_path(L"test/is_potential_path_test/aardvark", wds, 0));
    do_test(is_potential_path(L"test/is_potential_path_test/al", wds, PATH_REQUIRE_DIR));
    do_test(is_potential_path(L"test/is_potential_path_test/aardv", wds, 0));

    do_test(!is_potential_path(L"test/is_potential_path_test/aardvark", wds, PATH_REQUIRE_DIR));
    do_test(!is_potential_path(L"test/is_potential_path_test/al/", wds, 0));
    do_test(!is_potential_path(L"test/is_potential_path_test/ar", wds, 0));

    do_test(is_potential_path(L"/usr", wds, PATH_REQUIRE_DIR));
}

/// Test the 'test' builtin.
int builtin_test(parser_t &parser, io_streams_t &streams, wchar_t **argv);
static bool run_one_test_test(int expected, wcstring_list_t &lst, bool bracket) {
    parser_t parser;
    size_t i, count = lst.size();
    wchar_t **argv = new wchar_t *[count + 3];
    argv[0] = (wchar_t *)(bracket ? L"[" : L"test");
    for (i = 0; i < count; i++) {
        argv[i + 1] = (wchar_t *)lst.at(i).c_str();
    }
    if (bracket) {
        argv[i + 1] = (wchar_t *)L"]";
        i++;
    }
    argv[i + 1] = NULL;
    io_streams_t streams(0);
    int result = builtin_test(parser, streams, argv);
    delete[] argv;
    return expected == result;
}

static bool run_test_test(int expected, const wcstring &str) {
    using namespace std;
    wcstring_list_t argv;
    completion_list_t comps;

    // We need to tokenize the string in the same manner a normal shell would do. This is because we
    // need to test things like quoted strings that have leading and trailing whitespace.
    parser_t::expand_argument_list(str, 0, &comps);
    for (completion_list_t::const_iterator it = comps.begin(), end = comps.end(); it != end; ++it) {
        argv.push_back(it->completion);
    }

    bool bracket = run_one_test_test(expected, argv, true);
    bool nonbracket = run_one_test_test(expected, argv, false);
    do_test(bracket == nonbracket);
    return nonbracket;
}

static void test_test_brackets() {
    // Ensure [ knows it needs a ].
    parser_t parser;
    io_streams_t streams(0);

    null_terminated_array_t<wchar_t> args;

    args.set({L"[", L"foo"});
    do_test(builtin_test(parser, streams, args.get()) != 0);

    args.set({L"[", L"foo", L"]"});
    do_test(builtin_test(parser, streams, args.get()) == 0);

    args.set({L"[", L"foo", L"]", L"bar"});
    do_test(builtin_test(parser, streams, args.get()) != 0);
}

static void test_test() {
    say(L"Testing test builtin");
    test_test_brackets();

    do_test(run_test_test(0, L"5 -ne 6"));
    do_test(run_test_test(0, L"5 -eq 5"));
    do_test(run_test_test(0, L"0 -eq 0"));
    do_test(run_test_test(0, L"-1 -eq -1"));
    do_test(run_test_test(0, L"1 -ne -1"));
    do_test(run_test_test(1, L"' 2 ' -ne 2"));
    do_test(run_test_test(0, L"' 2' -eq 2"));
    do_test(run_test_test(0, L"'2 ' -eq 2"));
    do_test(run_test_test(0, L"' 2 ' -eq 2"));
    do_test(run_test_test(1, L"' 2x' -eq 2"));
    do_test(run_test_test(1, L"'' -eq 0"));
    do_test(run_test_test(1, L"'' -ne 0"));
    do_test(run_test_test(1, L"'  ' -eq 0"));
    do_test(run_test_test(1, L"'  ' -ne 0"));
    do_test(run_test_test(1, L"'x' -eq 0"));
    do_test(run_test_test(1, L"'x' -ne 0"));
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

    // Test precedence:
    //      '0 == 0 || 0 == 1 && 0 == 2'
    //  should be evaluated as:
    //      '0 == 0 || (0 == 1 && 0 == 2)'
    //  and therefore true. If it were
    //      '(0 == 0 || 0 == 1) && 0 == 2'
    //  it would be false.
    do_test(run_test_test(0, L"0 = 0 -o 0 = 1 -a 0 = 2"));
    do_test(run_test_test(0, L"-n 5 -o 0 = 1 -a 0 = 2"));
    do_test(run_test_test(1, L"( 0 = 0 -o  0 = 1 ) -a 0 = 2"));
    do_test(run_test_test(0, L"0 = 0 -o ( 0 = 1 -a 0 = 2 )"));

    // A few lame tests for permissions; these need to be a lot more complete.
    do_test(run_test_test(0, L"-e /bin/ls"));
    do_test(run_test_test(1, L"-e /bin/ls_not_a_path"));
    do_test(run_test_test(0, L"-x /bin/ls"));
    do_test(run_test_test(1, L"-x /bin/ls_not_a_path"));
    do_test(run_test_test(0, L"-d /bin/"));
    do_test(run_test_test(1, L"-d /bin/ls"));

    // This failed at one point.
    do_test(run_test_test(1, L"-d /bin -a 5 -eq 3"));
    do_test(run_test_test(0, L"-d /bin -o 5 -eq 3"));
    do_test(run_test_test(0, L"-d /bin -a ! 5 -eq 3"));

    // We didn't properly handle multiple "just strings" either.
    do_test(run_test_test(0, L"foo"));
    do_test(run_test_test(0, L"foo -a bar"));

    // These should be errors.
    do_test(run_test_test(1, L"foo bar"));
    do_test(run_test_test(1, L"foo bar baz"));

    // This crashed.
    do_test(run_test_test(1, L"1 = 1 -a = 1"));

    // Make sure we can treat -S as a parameter instead of an operator.
    // https://github.com/fish-shell/fish-shell/issues/601
    do_test(run_test_test(0, L"-S = -S"));
    do_test(run_test_test(1, L"! ! ! A"));

    // Verify that 1. doubles are treated as doubles, and 2. integers that cannot be represented as
    // doubles are still treated as integers.
    do_test(run_test_test(0, L"4611686018427387904 -eq 4611686018427387904"));
    do_test(run_test_test(0, L"4611686018427387904.0 -eq 4611686018427387904.0"));
    do_test(run_test_test(0, L"4611686018427387904.00000000000000001 -eq 4611686018427387904.0"));
    do_test(run_test_test(1, L"4611686018427387904 -eq 4611686018427387905"));
    do_test(run_test_test(0, L"-4611686018427387904 -ne 4611686018427387904"));
    do_test(run_test_test(0, L"-4611686018427387904 -le 4611686018427387904"));
    do_test(run_test_test(1, L"-4611686018427387904 -ge 4611686018427387904"));
    do_test(run_test_test(1, L"4611686018427387904 -gt 4611686018427387904"));
    do_test(run_test_test(0, L"4611686018427387904 -ge 4611686018427387904"));
}

/// Testing colors.
static void test_colors() {
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

static void test_complete() {
    say(L"Testing complete");

    const wchar_t *name_strs[] = {L"Foo1", L"Foo2", L"Foo3", L"Bar1", L"Bar2", L"Bar3"};
    size_t count = sizeof name_strs / sizeof *name_strs;
    const wcstring_list_t names(name_strs, name_strs + count);
    std::vector<completion_t> completions;
    complete_set_variable_names(&names);

    complete(L"$", &completions, COMPLETION_REQUEST_DEFAULT);
    completions_sort_and_prioritize(&completions);
    do_test(completions.size() == 6);
    do_test(completions.at(0).completion == L"Bar1");
    do_test(completions.at(1).completion == L"Bar2");
    do_test(completions.at(2).completion == L"Bar3");
    do_test(completions.at(3).completion == L"Foo1");
    do_test(completions.at(4).completion == L"Foo2");
    do_test(completions.at(5).completion == L"Foo3");

    completions.clear();
    complete(L"$F", &completions, COMPLETION_REQUEST_DEFAULT);
    completions_sort_and_prioritize(&completions);
    do_test(completions.size() == 3);
    do_test(completions.at(0).completion == L"oo1");
    do_test(completions.at(1).completion == L"oo2");
    do_test(completions.at(2).completion == L"oo3");

    completions.clear();
    complete(L"$1", &completions, COMPLETION_REQUEST_DEFAULT);
    completions_sort_and_prioritize(&completions);
    do_test(completions.empty());

    completions.clear();
    complete(L"$1", &completions, COMPLETION_REQUEST_DEFAULT | COMPLETION_REQUEST_FUZZY_MATCH);
    completions_sort_and_prioritize(&completions);
    do_test(completions.size() == 2);
    do_test(completions.at(0).completion == L"$Bar1");
    do_test(completions.at(1).completion == L"$Foo1");

    if (system("mkdir -p 'test/complete_test'")) err(L"mkdir failed");
    if (system("touch 'test/complete_test/has space'")) err(L"touch failed");
    if (system("touch 'test/complete_test/testfile'")) err(L"touch failed");
    if (system("chmod 700 'test/complete_test/testfile'")) err(L"chmod failed");

    completions.clear();
    complete(L"echo (test/complete_test/testfil", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"e");

    completions.clear();
    complete(L"echo (ls test/complete_test/testfil", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"e");

    completions.clear();
    complete(L"echo (command ls test/complete_test/testfil", &completions,
             COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"e");

    // Completing after spaces - see #2447
    completions.clear();
    complete(L"echo (ls test/complete_test/has\\ ", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"space");

    // Add a function and test completing it in various ways.
    // Note we're depending on function_data_t not complaining when given missing parsed_source /
    // body_node.
    struct function_data_t func_data = {};
    func_data.name = L"scuttlebutt";
    function_add(func_data, parser_t::principal_parser());

    // Complete a function name.
    completions.clear();
    complete(L"echo (scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"t");

    // But not with the command prefix.
    completions.clear();
    complete(L"echo (command scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 0);

    // Not with the builtin prefix.
    completions.clear();
    complete(L"echo (builtin scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 0);

    // Not after a redirection.
    completions.clear();
    complete(L"echo hi > scuttlebut", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 0);

    // Trailing spaces (#1261).
    complete_add(L"foobarbaz", false, wcstring(), option_type_args_only, NO_FILES, NULL, L"qux",
                 NULL, COMPLETE_AUTO_SPACE);
    completions.clear();
    complete(L"foobarbaz ", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"qux");

    // Don't complete variable names in single quotes (#1023).
    completions.clear();
    complete(L"echo '$Foo", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.empty());
    completions.clear();
    complete(L"echo \\$Foo", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.empty());

    // File completions.
    completions.clear();
    complete(L"cat test/complete_test/te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"echo sup > test/complete_test/te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"echo sup > test/complete_test/te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");

    if (!pushd("test/complete_test")) return;
    complete(L"cat te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    do_test(!(completions.at(0).flags & COMPLETE_REPLACES_TOKEN));
    do_test(!(completions.at(0).flags & COMPLETE_DUPLICATES_ARGUMENT));
    completions.clear();
    complete(L"cat testfile te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    do_test(completions.at(0).flags & COMPLETE_DUPLICATES_ARGUMENT);
    completions.clear();
    complete(L"cat testfile TE", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"testfile");
    do_test(completions.at(0).flags & COMPLETE_REPLACES_TOKEN);
    do_test(completions.at(0).flags & COMPLETE_DUPLICATES_ARGUMENT);
    completions.clear();
    complete(L"something --abc=te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something -abc=te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something abc=te", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"stfile");
    completions.clear();
    complete(L"something abc=stfile", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.size() == 0);
    completions.clear();
    complete(L"something abc=stfile", &completions, COMPLETION_REQUEST_FUZZY_MATCH);
    do_test(completions.size() == 1);
    do_test(completions.at(0).completion == L"abc=testfile");

    // Zero escapes can cause problems. See issue #1631.
    completions.clear();
    complete(L"cat foo\\0", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.empty());
    completions.clear();
    complete(L"cat foo\\0bar", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.empty());
    completions.clear();
    complete(L"cat \\0", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.empty());
    completions.clear();
    complete(L"cat te\\0", &completions, COMPLETION_REQUEST_DEFAULT);
    do_test(completions.empty());

    popd();
    completions.clear();
    complete_set_variable_names(NULL);

    // Test wraps.
    do_test(comma_join(complete_get_wrap_targets(L"wrapper1")) == L"");
    complete_add_wrapper(L"wrapper1", L"wrapper2");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper1")) == L"wrapper2");
    complete_add_wrapper(L"wrapper2", L"wrapper3");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper1")) == L"wrapper2");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper2")) == L"wrapper3");
    complete_add_wrapper(L"wrapper3", L"wrapper1");  // loop!
    do_test(comma_join(complete_get_wrap_targets(L"wrapper1")) == L"wrapper2");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper2")) == L"wrapper3");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper3")) == L"wrapper1");
    complete_remove_wrapper(L"wrapper1", L"wrapper2");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper1")) == L"");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper2")) == L"wrapper3");
    do_test(comma_join(complete_get_wrap_targets(L"wrapper3")) == L"wrapper1");
}

static void test_1_completion(wcstring line, const wcstring &completion, complete_flags_t flags,
                              bool append_only, wcstring expected, long source_line) {
    // str is given with a caret, which we use to represent the cursor position. Find it.
    const size_t in_cursor_pos = line.find(L'^');
    do_test(in_cursor_pos != wcstring::npos);
    line.erase(in_cursor_pos, 1);

    const size_t out_cursor_pos = expected.find(L'^');
    do_test(out_cursor_pos != wcstring::npos);
    expected.erase(out_cursor_pos, 1);

    size_t cursor_pos = in_cursor_pos;
    wcstring result =
        completion_apply_to_command_line(completion, flags, line, &cursor_pos, append_only);
    if (result != expected) {
        fwprintf(stderr, L"line %ld: %ls + %ls -> [%ls], expected [%ls]\n", source_line,
                 line.c_str(), completion.c_str(), result.c_str(), expected.c_str());
    }
    do_test(result == expected);
    do_test(cursor_pos == out_cursor_pos);
}

static void test_completion_insertions() {
#define TEST_1_COMPLETION(a, b, c, d, e) test_1_completion(a, b, c, d, e, __LINE__)
    say(L"Testing completion insertions");
    TEST_1_COMPLETION(L"foo^", L"bar", 0, false, L"foobar ^");
    // We really do want to insert two spaces here - otherwise it's hidden by the cursor.
    TEST_1_COMPLETION(L"foo^ baz", L"bar", 0, false, L"foobar ^ baz");
    TEST_1_COMPLETION(L"'foo^", L"bar", 0, false, L"'foobar' ^");
    TEST_1_COMPLETION(L"'foo'^", L"bar", 0, false, L"'foobar' ^");
    TEST_1_COMPLETION(L"'foo\\'^", L"bar", 0, false, L"'foo\\'bar' ^");
    TEST_1_COMPLETION(L"foo\\'^", L"bar", 0, false, L"foo\\'bar ^");

    // Test append only.
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

static void perform_one_autosuggestion_cd_test(const wcstring &command, const wcstring &expected,
                                               long line) {
    std::vector<completion_t> comps;
    complete(command, &comps, COMPLETION_REQUEST_AUTOSUGGESTION);

    bool expects_error = (expected == L"<error>");

    if (comps.empty() && !expects_error) {
        fwprintf(stderr, L"line %ld: autosuggest_suggest_special() failed for command %ls\n", line,
                 command.c_str());
        do_test_from(!comps.empty(), line);
        return;
    } else if (!comps.empty() && expects_error) {
        fwprintf(stderr,
                 L"line %ld: autosuggest_suggest_special() was expected to fail but did not, "
                 L"for command %ls\n",
                 line, command.c_str());
        do_test_from(comps.empty(), line);
    }

    if (!comps.empty()) {
        completions_sort_and_prioritize(&comps);
        const completion_t &suggestion = comps.at(0);

        if (suggestion.completion != expected) {
            fwprintf(
                stderr,
                L"line %ld: complete() for cd returned the wrong expected string for command %ls\n",
                line, command.c_str());
            fwprintf(stderr, L"  actual: %ls\n", suggestion.completion.c_str());
            fwprintf(stderr, L"expected: %ls\n", expected.c_str());
            do_test_from(suggestion.completion == expected, line);
        }
    }
}

static void perform_one_completion_cd_test(const wcstring &command, const wcstring &expected,
                                           long line) {
    std::vector<completion_t> comps;
    complete(command, &comps, COMPLETION_REQUEST_DEFAULT);

    bool expects_error = (expected == L"<error>");

    if (comps.empty() && !expects_error) {
        fwprintf(stderr, L"line %ld: autosuggest_suggest_special() failed for command %ls\n", line,
                 command.c_str());
        do_test_from(!comps.empty(), line);
        return;
    } else if (!comps.empty() && expects_error) {
        fwprintf(stderr,
                 L"line %ld: autosuggest_suggest_special() was expected to fail but did not, "
                 L"for command %ls\n",
                 line, command.c_str());
        do_test_from(comps.empty(), line);
    }

    if (!comps.empty()) {
        completions_sort_and_prioritize(&comps);
        const completion_t &suggestion = comps.at(0);

        if (suggestion.completion != expected) {
            fwprintf(stderr,
                     L"line %ld: complete() for cd tab completion returned the wrong expected "
                     L"string for command %ls\n",
                     line, command.c_str());
            fwprintf(stderr, L"  actual: %ls\n", suggestion.completion.c_str());
            fwprintf(stderr, L"expected: %ls\n", expected.c_str());
            do_test_from(suggestion.completion == expected, line);
        }
    }
}

// Testing test_autosuggest_suggest_special, in particular for properly handling quotes and
// backslashes.
static void test_autosuggest_suggest_special() {
    if (system("mkdir -p 'test/autosuggest_test/0foobar'")) err(L"mkdir failed");
    if (system("mkdir -p 'test/autosuggest_test/1foo bar'")) err(L"mkdir failed");
    if (system("mkdir -p 'test/autosuggest_test/2foo  bar'")) err(L"mkdir failed");
    if (system("mkdir -p 'test/autosuggest_test/3foo\\bar'")) err(L"mkdir failed");
    if (system("mkdir -p test/autosuggest_test/4foo\\'bar")) {
        err(L"mkdir failed");  // a path with a single quote
    }
    if (system("mkdir -p test/autosuggest_test/5foo\\\"bar")) {
        err(L"mkdir failed");  // a path with a double quote
    }
    // This is to ensure tilde expansion is handled. See the `cd ~/test_autosuggest_suggest_specia`
    // test below. I really dislike this since it mucks with a persons home directory.
    //
    // The question is how to modify the test so that tilde expansion can be made hermetic to this
    // test.
    if (system("mkdir -p ~/test_autosuggest_suggest_special/")) {
        err(L"mkdir failed");
    }
    if (system("mkdir -p test/autosuggest_test/start/unique2/unique3/multi4")) {
        err(L"mkdir failed");
    }
    if (system("mkdir -p test/autosuggest_test/start/unique2/unique3/multi42")) {
        err(L"mkdir failed");
    }
    if (system("mkdir -p test/autosuggest_test/start/unique2/.hiddenDir/moreStuff")) {
        err(L"mkdir failed");
    }

    const wcstring wd = L"test/autosuggest_test";

    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"test/autosuggest_test/0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 'test/autosuggest_test/0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/1", L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"test/autosuggest_test/1", L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 'test/autosuggest_test/1", L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/2", L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"test/autosuggest_test/2", L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 'test/autosuggest_test/2", L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/3", L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"test/autosuggest_test/3", L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 'test/autosuggest_test/3", L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/4", L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"test/autosuggest_test/4", L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 'test/autosuggest_test/4", L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/5", L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"test/autosuggest_test/5", L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 'test/autosuggest_test/5", L"foo\"bar/", __LINE__);

    env_set_one(L"AUTOSUGGEST_TEST_LOC", ENV_LOCAL, wd);
    perform_one_autosuggestion_cd_test(L"cd $AUTOSUGGEST_TEST_LOC/0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd ~/test_autosuggest_suggest_specia", L"l/", __LINE__);

    perform_one_autosuggestion_cd_test(L"cd test/autosuggest_test/start/", L"unique2/unique3/",
                                       __LINE__);

    if (!pushd(wcs2string(wd).c_str())) return;
    perform_one_autosuggestion_cd_test(L"cd 0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '0", L"foobar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 1", L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"1", L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '1", L"foo bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 2", L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"2", L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '2", L"foo  bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 3", L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"3", L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '3", L"foo\\bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 4", L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"4", L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '4", L"foo'bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd 5", L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd \"5", L"foo\"bar/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd '5", L"foo\"bar/", __LINE__);

    // A single quote should defeat tilde expansion.
    perform_one_autosuggestion_cd_test(L"cd '~/test_autosuggest_suggest_specia'", L"<error>",
                                       __LINE__);

    // Don't crash on ~ (issue #2696). Note this is cwd dependent.
    if (system("mkdir -p '~hahaha/path1/path2/'")) err(L"mkdir failed");
    perform_one_autosuggestion_cd_test(L"cd ~haha", L"ha/path1/path2/", __LINE__);
    perform_one_autosuggestion_cd_test(L"cd ~hahaha/", L"path1/path2/", __LINE__);
    perform_one_completion_cd_test(L"cd ~haha", L"ha/", __LINE__);
    perform_one_completion_cd_test(L"cd ~hahaha/", L"path1/", __LINE__);

    popd();
    (void)system("rmdir ~/test_autosuggest_suggest_special/");
}

static void perform_one_autosuggestion_should_ignore_test(const wcstring &command, long line) {
    completion_list_t comps;
    complete(command, &comps, COMPLETION_REQUEST_AUTOSUGGESTION);
    do_test(comps.empty());
    if (!comps.empty()) {
        const wcstring &suggestion = comps.front().completion;
        fwprintf(stderr, L"line %ld: complete() expected to return nothing for %ls\n", line,
                 command.c_str());
        fwprintf(stderr, L"  instead got: %ls\n", suggestion.c_str());
    }
}

static void test_autosuggestion_ignores() {
    say(L"Testing scenarios that should produce no autosuggestions");
    // Do not do file autosuggestions immediately after certain statement terminators - see #1631.
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST|", __LINE__);
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST&", __LINE__);
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST#comment", __LINE__);
    perform_one_autosuggestion_should_ignore_test(L"echo PIPE_TEST;", __LINE__);
}

static void test_autosuggestion_combining() {
    say(L"Testing autosuggestion combining");
    do_test(combine_command_and_autosuggestion(L"alpha", L"alphabeta") == L"alphabeta");

    // When the last token contains no capital letters, we use the case of the autosuggestion.
    do_test(combine_command_and_autosuggestion(L"alpha", L"ALPHABETA") == L"ALPHABETA");

    // When the last token contains capital letters, we use its case.
    do_test(combine_command_and_autosuggestion(L"alPha", L"alphabeTa") == L"alPhabeTa");

    // If autosuggestion is not longer than input, use the input's case.
    do_test(combine_command_and_autosuggestion(L"alpha", L"ALPHAA") == L"ALPHAA");
    do_test(combine_command_and_autosuggestion(L"alpha", L"ALPHA") == L"alpha");
}

static void test_history_matches(history_search_t &search, const wcstring_list_t &expected,
                                 unsigned from_line) {
    wcstring_list_t found;
    while (search.go_backwards()) {
        found.push_back(search.current_string());
    }
    do_test_from(expected == found, from_line);
    if (expected != found) {
        fprintf(stderr, "Expected %ls, found %ls\n", comma_join(expected).c_str(),
                comma_join(found).c_str());
    }
}

static bool history_contains(history_t *history, const wcstring &txt) {
    bool result = false;
    size_t i;
    for (i = 1;; i++) {
        history_item_t item = history->item_at_index(i);
        if (item.empty()) break;

        if (item.str() == txt) {
            result = true;
            break;
        }
    }
    return result;
}

static bool history_contains(const std::unique_ptr<history_t> &history, const wcstring &txt) {
    return history_contains(history.get(), txt);
}

static void test_input() {
    say(L"Testing input");
    // Ensure sequences are order independent. Here we add two bindings where the first is a prefix
    // of the second, and then emit the second key list. The second binding should be invoked, not
    // the first!
    wcstring prefix_binding = L"qqqqqqqa";
    wcstring desired_binding = prefix_binding + L'a';
    input_mapping_add(prefix_binding.c_str(), L"up-line");
    input_mapping_add(desired_binding.c_str(), L"down-line");

    // Push the desired binding to the queue.
    for (size_t idx = 0; idx < desired_binding.size(); idx++) {
        input_queue_ch(desired_binding.at(idx));
    }

    // Now test.
    wint_t c = input_readch();
    if (c != R_DOWN_LINE) {
        err(L"Expected to read char R_DOWN_LINE, but instead got %ls\n", describe_char(c).c_str());
    }
}

#define UVARS_PER_THREAD 8
#define UVARS_TEST_PATH L"test/fish_uvars_test/varsfile.txt"

static int test_universal_helper(int x) {
    callback_data_list_t callbacks;
    env_universal_t uvars(UVARS_TEST_PATH);
    for (int j = 0; j < UVARS_PER_THREAD; j++) {
        const wcstring key = format_string(L"key_%d_%d", x, j);
        const wcstring val = format_string(L"val_%d_%d", x, j);
        uvars.set(key, {val}, false);
        bool synced = uvars.sync(callbacks);
        if (!synced) {
            err(L"Failed to sync universal variables after modification");
        }
    }

    // Last step is to delete the first key.
    uvars.remove(format_string(L"key_%d_%d", x, 0));
    bool synced = uvars.sync(callbacks);
    if (!synced) {
        err(L"Failed to sync universal variables after deletion");
    }
    return 0;
}

static void test_universal() {
    say(L"Testing universal variables");
    if (system("mkdir -p test/fish_uvars_test/")) err(L"mkdir failed");

    const int threads = 1;
    for (int i = 0; i < threads; i++) {
        iothread_perform([=]() { test_universal_helper(i); });
    }
    iothread_drain_all();

    env_universal_t uvars(UVARS_TEST_PATH);
    callback_data_list_t callbacks;
    bool loaded = uvars.initialize(callbacks);
    if (!loaded) {
        err(L"Failed to load universal variables");
    }
    for (int i = 0; i < threads; i++) {
        for (int j = 0; j < UVARS_PER_THREAD; j++) {
            const wcstring key = format_string(L"key_%d_%d", i, j);
            maybe_t<env_var_t> expected_val;
            if (j == 0) {
                expected_val = none();
            } else {
                expected_val = env_var_t(format_string(L"val_%d_%d", i, j), 0);
            }
            const maybe_t<env_var_t> var = uvars.get(key);
            if (j == 0) assert(!expected_val);
            if (var != expected_val) {
                const wchar_t *missing_desc = L"<missing>";
                err(L"Wrong value for key %ls: expected %ls, got %ls\n", key.c_str(),
                    (expected_val ? expected_val->as_string().c_str() : missing_desc),
                    (var ? var->as_string().c_str() : missing_desc));
            }
        }
    }
    (void)system("rm -Rf test/fish_uvars_test/");
}

static bool callback_data_less_than(const callback_data_t &a, const callback_data_t &b) {
    return a.key < b.key;
}

static void test_universal_callbacks() {
    say(L"Testing universal callbacks");
    if (system("mkdir -p test/fish_uvars_test/")) err(L"mkdir failed");
    callback_data_list_t callbacks;
    env_universal_t uvars1(UVARS_TEST_PATH);
    env_universal_t uvars2(UVARS_TEST_PATH);

    // Put some variables into both.
    uvars1.set(L"alpha", {L"1"}, false);
    uvars1.set(L"beta", {L"1"}, false);
    uvars1.set(L"delta", {L"1"}, false);
    uvars1.set(L"epsilon", {L"1"}, false);
    uvars1.set(L"lambda", {L"1"}, false);
    uvars1.set(L"kappa", {L"1"}, false);
    uvars1.set(L"omicron", {L"1"}, false);

    uvars1.sync(callbacks);
    uvars2.sync(callbacks);

    // Change uvars1.
    uvars1.set(L"alpha", {L"2"}, false);    // changes value
    uvars1.set(L"beta", {L"1"}, true);      // changes export
    uvars1.remove(L"delta");                // erases value
    uvars1.set(L"epsilon", {L"1"}, false);  // changes nothing
    uvars1.sync(callbacks);

    // Change uvars2. It should treat its value as correct and ignore changes from uvars1.
    uvars2.set(L"lambda", {L"1"}, false);  // same value
    uvars2.set(L"kappa", {L"2"}, false);   // different value

    // Now see what uvars2 sees.
    callbacks.clear();
    uvars2.sync(callbacks);

    // Sort them to get them in a predictable order.
    std::sort(callbacks.begin(), callbacks.end(), callback_data_less_than);

    // Should see exactly three changes.
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
    (void)system("rm -Rf test/fish_uvars_test/");
}

bool poll_notifier(const std::unique_ptr<universal_notifier_t> &note) {
    bool result = false;
    if (note->usec_delay_between_polls() > 0) {
        result = note->poll();
    }

    int fd = note->notification_fd();
    if (!result && fd >= 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0, 0};
        if (select(fd + 1, &fds, NULL, NULL, &tv) > 0 && FD_ISSET(fd, &fds)) {
            result = note->notification_fd_became_readable(fd);
        }
    }
    return result;
}

static void trigger_or_wait_for_notification(universal_notifier_t::notifier_strategy_t strategy) {
    switch (strategy) {
        case universal_notifier_t::strategy_shmem_polling: {
            break;  // nothing required
        }
        case universal_notifier_t::strategy_notifyd: {
            // notifyd requires a round trip to the notifyd server, which means we have to wait a
            // little bit to receive it. In practice 40 ms seems to be enough.
            usleep(40000);
            break;
        }
        case universal_notifier_t::strategy_named_pipe: {
            break;  // nothing required
        }
    }
}

static void test_notifiers_with_strategy(universal_notifier_t::notifier_strategy_t strategy) {
    say(L"Testing universal notifiers with strategy %d", (int)strategy);
    std::unique_ptr<universal_notifier_t> notifiers[16];
    size_t notifier_count = sizeof notifiers / sizeof *notifiers;

    // Populate array of notifiers.
    for (size_t i = 0; i < notifier_count; i++) {
        notifiers[i] = universal_notifier_t::new_notifier_for_strategy(strategy, UVARS_TEST_PATH);
    }

    // Nobody should poll yet.
    for (size_t i = 0; i < notifier_count; i++) {
        if (poll_notifier(notifiers[i])) {
            err(L"Universal variable notifier polled true before any changes, with strategy %d",
                (int)strategy);
        }
    }

    // Tweak each notifier. Verify that others see it.
    for (size_t post_idx = 0; post_idx < notifier_count; post_idx++) {
        notifiers[post_idx]->post_notification();

        // Do special stuff to "trigger" a notification for testing.
        trigger_or_wait_for_notification(strategy);

        for (size_t i = 0; i < notifier_count; i++) {
            // We aren't concerned with the one who posted. Poll from it (to drain it), and then
            // skip it.
            if (i == post_idx) {
                poll_notifier(notifiers[i]);
                continue;
            }

            if (!poll_notifier(notifiers[i])) {
                err(L"Universal variable notifier (%lu) %p polled failed to notice changes, with "
                    L"strategy %d",
                    i, notifiers[i].get(), (int)strategy);
            }
        }

        // Named pipes have special cleanup requirements.
        if (strategy == universal_notifier_t::strategy_named_pipe) {
            usleep(1000000 / 10);  // corresponds to NAMED_PIPE_FLASH_DURATION_USEC
            // Have to clean up the posted one first, so that the others see the pipe become no
            // longer readable.
            poll_notifier(notifiers[post_idx]);
            for (size_t i = 0; i < notifier_count; i++) {
                poll_notifier(notifiers[i]);
            }
        }
    }

    // Nobody should poll now.
    for (size_t i = 0; i < notifier_count; i++) {
        if (poll_notifier(notifiers[i])) {
            err(L"Universal variable notifier polled true after all changes, with strategy %d",
                (int)strategy);
        }
    }
}

static void test_universal_notifiers() {
    if (system("mkdir -p test/fish_uvars_test/ && touch test/fish_uvars_test/varsfile.txt")) {
        err(L"mkdir failed");
    }

    auto strategy = universal_notifier_t::resolve_default_strategy();
    test_notifiers_with_strategy(strategy);
}

class history_tests_t {
   public:
    static void test_history();
    static void test_history_merge();
    static void test_history_formats();
    // static void test_history_speed(void);
    static void test_history_races();
    static void test_history_races_pound_on_history(size_t item_count);
};

static wcstring random_string() {
    wcstring result;
    size_t max = 1 + rand() % 32;
    while (max--) {
        wchar_t c = 1 + rand() % ESCAPE_TEST_CHAR;
        result.push_back(c);
    }
    return result;
}

// Helper to lowercase a string.
static wcstring lower(const wcstring &s) {
    wcstring result;
    for (wchar_t c : s) {
        result.push_back(towlower(c));
    }
    return result;
}

void history_tests_t::test_history() {
    history_search_t searcher;
    say(L"Testing history");

    const wcstring_list_t items = {L"Gamma", L"beta",  L"BetA", L"Beta", L"alpha",
                                   L"AlphA", L"Alpha", L"alph", L"ALPH", L"ZZZ"};
    const history_search_flags_t nocase = history_search_ignore_case;

    // Populate a history.
    history_t &history = history_t::history_with_name(L"test_history");
    history.clear();
    for (const wcstring &s : items) {
        history.add(s);
    }

    // Helper to set expected items to those matching a predicate, in reverse order.
    wcstring_list_t expected;
    auto set_expected = [&](const std::function<bool(const wcstring &)> &filt) {
        expected.clear();
        for (const auto &s : items) {
            if (filt(s)) expected.push_back(s);
        }
        std::reverse(expected.begin(), expected.end());
    };

    // Items matching "a", case-sensitive.
    searcher = history_search_t(history, L"a");
    set_expected([](const wcstring &s) { return s.find(L'a') != wcstring::npos; });
    test_history_matches(searcher, expected, __LINE__);

    // Items matching "alpha", case-insensitive.
    searcher = history_search_t(history, L"AlPhA", HISTORY_SEARCH_TYPE_CONTAINS, nocase);
    set_expected([](const wcstring &s) { return lower(s).find(L"alpha") != wcstring::npos; });
    test_history_matches(searcher, expected, __LINE__);

    // Items matching "et", case-sensitive.
    searcher = history_search_t(history, L"et");
    set_expected([](const wcstring &s) { return s.find(L"et") != wcstring::npos; });
    test_history_matches(searcher, expected, __LINE__);

    // Items starting with "be", case-sensitive.
    searcher = history_search_t(history, L"be", HISTORY_SEARCH_TYPE_PREFIX, 0);
    set_expected([](const wcstring &s) { return string_prefixes_string(L"be", s); });
    test_history_matches(searcher, expected, __LINE__);

    // Items starting with "be", case-insensitive.
    searcher = history_search_t(history, L"be", HISTORY_SEARCH_TYPE_PREFIX, nocase);
    set_expected(
        [](const wcstring &s) { return string_prefixes_string_case_insensitive(L"be", s); });
    test_history_matches(searcher, expected, __LINE__);

    // Items exactly matching "alph", case-sensitive.
    searcher = history_search_t(history, L"alph", HISTORY_SEARCH_TYPE_EXACT, 0);
    set_expected([](const wcstring &s) { return s == L"alph"; });
    test_history_matches(searcher, expected, __LINE__);

    // Items exactly matching "alph", case-insensitive.
    searcher = history_search_t(history, L"alph", HISTORY_SEARCH_TYPE_EXACT, nocase);
    set_expected([](const wcstring &s) { return lower(s) == L"alph"; });
    test_history_matches(searcher, expected, __LINE__);

    // Test item removal case-sensitive.
    searcher = history_search_t(history, L"Alpha");
    test_history_matches(searcher, {L"Alpha"}, __LINE__);
    history.remove(L"Alpha");
    searcher = history_search_t(history, L"Alpha");
    test_history_matches(searcher, {}, __LINE__);

    // Test history escaping and unescaping, yaml, etc.
    history_item_list_t before, after;
    history.clear();
    size_t i, max = 100;
    for (i = 1; i <= max; i++) {
        // Generate a value.
        wcstring value = wcstring(L"test item ") + to_string(i);

        // Maybe add some backslashes.
        if (i % 3 == 0) value.append(L"(slashies \\\\\\ slashies)");

        // Generate some paths.
        path_list_t paths;
        size_t count = rand() % 6;
        while (count--) {
            paths.push_back(random_string());
        }

        // Record this item.
        history_item_t item(value, time(NULL));
        item.required_paths = paths;
        before.push_back(item);
        history.add(item);
    }
    history.save();

    // Read items back in reverse order and ensure they're the same.
    for (i = 100; i >= 1; i--) {
        history_item_t item = history.item_at_index(i);
        do_test(!item.empty());
        after.push_back(item);
    }
    do_test(before.size() == after.size());
    for (size_t i = 0; i < before.size(); i++) {
        const history_item_t &bef = before.at(i), &aft = after.at(i);
        do_test(bef.contents == aft.contents);
        do_test(bef.creation_timestamp == aft.creation_timestamp);
        do_test(bef.required_paths == aft.required_paths);
    }

    // Clean up after our tests.
    history.clear();
}

// Wait until the next second.
static void time_barrier() {
    time_t start = time(NULL);
    do {
        usleep(1000);
    } while (time(NULL) == start);
}

static wcstring_list_t generate_history_lines(size_t item_count, int pid) {
    wcstring_list_t result;
    result.reserve(item_count);
    for (unsigned long i = 0; i < item_count; i++) {
        result.push_back(format_string(L"%ld %lu", (long)pid, (unsigned long)i));
    }
    return result;
}

void history_tests_t::test_history_races_pound_on_history(size_t item_count) {
    // Called in child process to modify history.
    history_t hist(L"race_test");
    hist.chaos_mode = !true;
    const wcstring_list_t hist_lines = generate_history_lines(item_count, getpid());
    for (const wcstring &line : hist_lines) {
        hist.add(line);
        hist.save();
    }
}

void history_tests_t::test_history_races() {
    say(L"Testing history race conditions");

    // Test concurrent history writing.
    // How many concurrent writers we have
    constexpr size_t RACE_COUNT = 4;

    // How many items each writer makes
    constexpr size_t ITEM_COUNT = 256;

    // Ensure history is clear.
    history_t(L"race_test").clear();

    pid_t children[RACE_COUNT];
    for (size_t i = 0; i < RACE_COUNT; i++) {
        pid_t pid = fork();
        if (!pid) {
            // Child process.
            setup_fork_guards();
            test_history_races_pound_on_history(ITEM_COUNT);
            exit_without_destructors(0);
        } else {
            // Parent process.
            children[i] = pid;
        }
    }

    // Wait for all children.
    for (size_t i = 0; i < RACE_COUNT; i++) {
        int stat;
        waitpid(children[i], &stat, WUNTRACED);
    }

    // Compute the expected lines.
    std::array<wcstring_list_t, RACE_COUNT> expected_lines;
    for (size_t i = 0; i < RACE_COUNT; i++) {
        expected_lines[i] = generate_history_lines(ITEM_COUNT, children[i]);
    }

    // Ensure we consider the lines that have been outputted as part of our history.
    time_barrier();

    // Ensure that we got sane, sorted results.
    history_t hist(L"race_test");
    hist.chaos_mode = !true;

    // History is enumerated from most recent to least
    // Every item should be the last item in some array
    size_t hist_idx;
    for (hist_idx = 1;; hist_idx++) {
        history_item_t item = hist.item_at_index(hist_idx);
        if (item.empty()) break;

        bool found = false;
        for (wcstring_list_t &list : expected_lines) {
            auto iter = std::find(list.begin(), list.end(), item.contents);
            if (iter != list.end()) {
                found = true;

                // Remove everything from this item on
                auto cursor = list.end();
                if (cursor + 1 != list.end()) {
                    while (--cursor != iter) {
                        err(L"Item dropped from history: %ls", cursor->c_str());
                    }
                }
                list.erase(iter, list.end());
                break;
            }
        }
        if (!found) {
            err(L"Line '%ls' found in history, but not found in some array", item.str().c_str());
            for (wcstring_list_t &list : expected_lines) {
                if (!list.empty()) {
                    fprintf(stderr, "\tRemaining: %ls\n", list.back().c_str());
                }
            }
        }
    }

    // +1 to account for history's 1-based offset
    size_t expected_idx = RACE_COUNT * ITEM_COUNT + 1;
    if (hist_idx != expected_idx) {
        err(L"Expected %lu items, but instead got %lu items", expected_idx, hist_idx);
    }

    // See if anything is left in the arrays
    for (const wcstring_list_t &list : expected_lines) {
        for (const wcstring &str : list) {
            err(L"Line '%ls' still left in the array", str.c_str());
        }
    }
    hist.clear();
}

void history_tests_t::test_history_merge() {
    // In a single fish process, only one history is allowed to exist with the given name But it's
    // common to have multiple history instances with the same name active in different processes,
    // e.g. when you have multiple shells open. We try to get that right and merge all their history
    // together. Test that case.
    say(L"Testing history merge");
    const size_t count = 3;
    const wcstring name = L"merge_test";
    std::unique_ptr<history_t> hists[count] = {
        make_unique<history_t>(name), make_unique<history_t>(name), make_unique<history_t>(name)};
    const wcstring texts[count] = {L"History 1", L"History 2", L"History 3"};
    const wcstring alt_texts[count] = {L"History Alt 1", L"History Alt 2", L"History Alt 3"};

    // Make sure history is clear.
    for (size_t i = 0; i < count; i++) {
        hists[i]->clear();
    }

    // Make sure we don't add an item in the same second as we created the history.
    time_barrier();

    // Add a different item to each.
    for (size_t i = 0; i < count; i++) {
        hists[i]->add(texts[i]);
    }

    // Save them.
    for (size_t i = 0; i < count; i++) {
        hists[i]->save();
    }

    // Make sure each history contains what it ought to, but they have not leaked into each other.
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < count; j++) {
            bool does_contain = history_contains(hists[i], texts[j]);
            bool should_contain = (i == j);
            do_test(should_contain == does_contain);
        }
    }

    // Make a new history. It should contain everything. The time_barrier() is so that the timestamp
    // is newer, since we only pick up items whose timestamp is before the birth stamp.
    time_barrier();
    std::unique_ptr<history_t> everything = make_unique<history_t>(name);
    for (size_t i = 0; i < count; i++) {
        do_test(history_contains(everything, texts[i]));
    }

    // Tell all histories to merge. Now everybody should have everything.
    for (size_t i = 0; i < count; i++) {
        hists[i]->incorporate_external_changes();
    }

    // Everyone should also have items in the same order (#2312)
    wcstring_list_t hist_vals1;
    hists[0]->get_history(hist_vals1);
    for (size_t i = 0; i < count; i++) {
        wcstring_list_t hist_vals2;
        hists[i]->get_history(hist_vals2);
        do_test(hist_vals1 == hist_vals2);
    }

    // Add some more per-history items.
    for (size_t i = 0; i < count; i++) {
        hists[i]->add(alt_texts[i]);
    }
    // Everybody should have old items, but only one history should have each new item.
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < count; j++) {
            // Old item.
            do_test(history_contains(hists[i], texts[j]));

            // New item.
            bool does_contain = history_contains(hists[i], alt_texts[j]);
            bool should_contain = (i == j);
            do_test(should_contain == does_contain);
        }
    }

    // Make sure incorporate_external_changes doesn't drop items! (#3496)
    history_t *const writer = hists[0].get();
    history_t *const reader = hists[1].get();
    const wcstring more_texts[] = {L"Item_#3496_1", L"Item_#3496_2", L"Item_#3496_3",
                                   L"Item_#3496_4", L"Item_#3496_5", L"Item_#3496_6"};
    for (size_t i = 0; i < sizeof more_texts / sizeof *more_texts; i++) {
        // time_barrier because merging will ignore items that may be newer
        if (i > 0) time_barrier();
        writer->add(more_texts[i]);
        writer->incorporate_external_changes();
        reader->incorporate_external_changes();
        for (size_t j = 0; j < i; j++) {
            do_test(history_contains(reader, more_texts[j]));
        }
    }
    everything->clear();
}

static bool install_sample_history(const wchar_t *name) {
    wcstring path;
    if (!path_get_data(path)) {
        err(L"Failed to get data directory");
        return false;
    }
    char command[512];
    snprintf(command, sizeof command, "cp tests/%ls %ls/%ls_history", name, path.c_str(), name);
    if (system(command)) {
        err(L"Failed to copy sample history");
        return false;
    }
    return true;
}

/// Indicates whether the history is equal to the given null-terminated array of strings.
static bool history_equals(history_t &hist, const wchar_t *const *strings) {
    // Count our expected items.
    size_t expected_count = 0;
    while (strings[expected_count]) {
        expected_count++;
    }

    // Ensure the contents are the same.
    size_t history_idx = 1;
    size_t array_idx = 0;
    for (;;) {
        const wchar_t *expected = strings[array_idx];
        history_item_t item = hist.item_at_index(history_idx);
        if (expected == NULL) {
            if (!item.empty()) {
                err(L"Expected empty item at history index %lu", history_idx);
            }
            break;
        } else {
            if (item.str() != expected) {
                err(L"Expected '%ls', found '%ls' at index %lu", expected, item.str().c_str(),
                    history_idx);
            }
        }
        history_idx++;
        array_idx++;
    }

    return true;
}

void history_tests_t::test_history_formats() {
    const wchar_t *name;

    // Test inferring and reading legacy and bash history formats.
    name = L"history_sample_fish_1_x";
    say(L"Testing %ls", name);
    if (!install_sample_history(name)) {
        err(L"Couldn't open file tests/%ls", name);
    } else {
        // Note: This is backwards from what appears in the file.
        const wchar_t *const expected[] = {
            L"#def", L"echo #abc", L"function yay\necho hi\nend", L"cd foobar", L"ls /", NULL};

        history_t &test_history = history_t::history_with_name(name);
        if (!history_equals(test_history, expected)) {
            err(L"test_history_formats failed for %ls\n", name);
        }
        test_history.clear();
    }

    name = L"history_sample_fish_2_0";
    say(L"Testing %ls", name);
    if (!install_sample_history(name)) {
        err(L"Couldn't open file tests/%ls", name);
    } else {
        const wchar_t *const expected[] = {L"echo this has\\\nbackslashes",
                                           L"function foo\necho bar\nend", L"echo alpha", NULL};

        history_t &test_history = history_t::history_with_name(name);
        if (!history_equals(test_history, expected)) {
            err(L"test_history_formats failed for %ls\n", name);
        }
        test_history.clear();
    }

    say(L"Testing bash import");
    FILE *f = fopen("tests/history_sample_bash", "r");
    if (!f) {
        err(L"Couldn't open file tests/history_sample_bash");
    } else {
        // The results are in the reverse order that they appear in the bash history file.
        // We don't expect whitespace to be elided (#4908: except for leading/trailing whitespace)
        const wchar_t *expected[] = {L"sleep 123",
                                     L"final line",
                                     L"echo supsup",
                                     L"export XVAR='exported'",
                                     L"history --help",
                                     L"echo foo",
                                     NULL};
        history_t &test_history = history_t::history_with_name(L"bash_import");
        test_history.populate_from_bash(f);
        if (!history_equals(test_history, expected)) {
            err(L"test_history_formats failed for bash import\n");
        }
        test_history.clear();
        fclose(f);
    }

    name = L"history_sample_corrupt1";
    say(L"Testing %ls", name);
    if (!install_sample_history(name)) {
        err(L"Couldn't open file tests/%ls", name);
    } else {
        // We simply invoke get_string_representation. If we don't die, the test is a success.
        history_t &test_history = history_t::history_with_name(name);
        const wchar_t *expected[] = {L"no_newline_at_end_of_file", L"corrupt_prefix",
                                     L"this_command_is_ok", NULL};
        if (!history_equals(test_history, expected)) {
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
    std::unique_ptr<history_t> hist = make_unique<history_t>(L"speed_test");
    wcstring item = L"History Speed Test - X";

    // Test for 10 seconds.
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
    fwprintf(stdout, L"%lu items - %.2f msec per item\n", (unsigned long)count,
             (stop - start) * 1E6 / count);
    hist->clear();
}
#endif

static void test_new_parser_correctness() {
    say(L"Testing parser correctness");
    const struct parser_test_t {
        const wchar_t *src;
        bool ok;
    } parser_tests[] = {
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
        {L"true && false || false", true},
        {L"true || false; and true", true},
        {L"true || ||", false},
        {L"|| true", false},
        {L"true || \n\n false", true},
    };

    for (size_t i = 0; i < sizeof parser_tests / sizeof *parser_tests; i++) {
        const parser_test_t *test = &parser_tests[i];

        parse_node_tree_t parse_tree;
        bool success = parse_tree_from_string(test->src, parse_flag_none, &parse_tree, NULL);
        if (success && !test->ok) {
            err(L"\"%ls\" should NOT have parsed, but did", test->src);
        } else if (!success && test->ok) {
            err(L"\"%ls\" should have parsed, but failed", test->src);
        }
    }
    say(L"Parse tests complete");
}

// Given that we have an array of 'fuzz_count' strings, we wish to enumerate all permutations of
// 'len' values. We do this by incrementing an integer, interpreting it as "base fuzz_count".
static inline bool string_for_permutation(const wcstring *fuzzes, size_t fuzz_count, size_t len,
                                          size_t permutation, wcstring *out_str) {
    out_str->clear();

    size_t remaining_permutation = permutation;
    for (size_t i = 0; i < len; i++) {
        size_t idx = remaining_permutation % fuzz_count;
        remaining_permutation /= fuzz_count;

        out_str->append(fuzzes[idx]);
        out_str->push_back(L' ');
    }
    // Return false if we wrapped.
    return remaining_permutation == 0;
}

static void test_new_parser_fuzzing() {
    say(L"Fuzzing parser (node size: %lu)", sizeof(parse_node_t));
    const wcstring fuzzes[] = {
        L"if",      L"else", L"for", L"in",  L"while", L"begin", L"function",
        L"switch",  L"case", L"end", L"and", L"or",    L"not",   L"command",
        L"builtin", L"foo",  L"|",   L"^",   L"&",     L";",
    };

    // Generate a list of strings of all keyword / token combinations.
    wcstring src;
    src.reserve(128);

    parse_node_tree_t node_tree;
    parse_error_list_t errors;

    double start = timef();
    bool log_it = true;
    unsigned long max_len = 5;
    for (unsigned long len = 0; len < max_len; len++) {
        if (log_it) fwprintf(stderr, L"%lu / %lu...", len, max_len);

        // We wish to look at all permutations of 4 elements of 'fuzzes' (with replacement).
        // Construct an int and keep incrementing it.
        unsigned long permutation = 0;
        while (string_for_permutation(fuzzes, sizeof fuzzes / sizeof *fuzzes, len, permutation++,
                                      &src)) {
            parse_tree_from_string(src, parse_flag_continue_after_error, &node_tree, &errors);
        }
        if (log_it) fwprintf(stderr, L"done (%lu)\n", permutation);
    }
    double end = timef();
    if (log_it) say(L"All fuzzed in %f seconds!", end - start);
}

// Parse a statement, returning the command, args (joined by spaces), and the decoration. Returns
// true if successful.
static bool test_1_parse_ll2(const wcstring &src, wcstring *out_cmd, wcstring *out_joined_args,
                             enum parse_statement_decoration_t *out_deco) {
    out_cmd->clear();
    out_joined_args->clear();
    *out_deco = parse_statement_decoration_none;

    parse_node_tree_t tree;
    if (!parse_tree_from_string(src, parse_flag_none, &tree, NULL)) {
        return false;
    }

    // Get the statement. Should only have one.
    tnode_t<grammar::job_list> job_list{&tree, &tree.at(0)};
    auto stmts = job_list.descendants<grammar::plain_statement>();
    if (stmts.size() != 1) {
        say(L"Unexpected number of statements (%lu) found in '%ls'", stmts.size(), src.c_str());
        return false;
    }
    tnode_t<grammar::plain_statement> stmt = stmts.at(0);

    // Return its decoration and command.
    *out_deco = get_decoration(stmt);
    *out_cmd = *command_for_plain_statement(stmt, src);

    // Return arguments separated by spaces.
    bool first = true;
    for (auto arg_node : stmt.descendants<grammar::argument>()) {
        if (!first) out_joined_args->push_back(L' ');
        out_joined_args->append(arg_node.get_source(src));
        first = false;
    }

    return true;
}

// Verify that 'function -h' and 'function --help' are plain statements but 'function --foo' is
// not (issue #1240).
template <typename Type>
static void check_function_help(const wchar_t *src) {
    parse_node_tree_t tree;
    if (!parse_tree_from_string(src, parse_flag_none, &tree, NULL)) {
        err(L"Failed to parse '%ls'", src);
    }

    tnode_t<grammar::job_list> node{&tree, &tree.at(0)};
    auto node_list = node.descendants<Type>();
    if (node_list.size() == 0) {
        err(L"Failed to find node of type '%ls'", token_type_description(Type::token));
    } else if (node_list.size() > 1) {
        err(L"Found too many nodes of type '%ls'", token_type_description(Type::token));
    }
}

// Test the LL2 (two token lookahead) nature of the parser by exercising the special builtin and
// command handling. In particular, 'command foo' should be a decorated statement 'foo' but 'command
// -help' should be an undecorated statement 'command' with argument '--help', and NOT attempt to
// run a command called '--help'.
static void test_new_parser_ll2() {
    say(L"Testing parser two-token lookahead");

    const struct {
        wcstring src;
        wcstring cmd;
        wcstring args;
        enum parse_statement_decoration_t deco;
    } tests[] = {
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
        {L"function --help", L"function", L"--help", parse_statement_decoration_none}};

    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        wcstring cmd, args;
        enum parse_statement_decoration_t deco = parse_statement_decoration_none;
        bool success = test_1_parse_ll2(tests[i].src, &cmd, &args, &deco);
        if (!success)
            err(L"Parse of '%ls' failed on line %ld", tests[i].cmd.c_str(), (long)__LINE__);
        if (cmd != tests[i].cmd)
            err(L"When parsing '%ls', expected command '%ls' but got '%ls' on line %ld",
                tests[i].src.c_str(), tests[i].cmd.c_str(), cmd.c_str(), (long)__LINE__);
        if (args != tests[i].args)
            err(L"When parsing '%ls', expected args '%ls' but got '%ls' on line %ld",
                tests[i].src.c_str(), tests[i].args.c_str(), args.c_str(), (long)__LINE__);
        if (deco != tests[i].deco)
            err(L"When parsing '%ls', expected decoration %d but got %d on line %ld",
                tests[i].src.c_str(), (int)tests[i].deco, (int)deco, (long)__LINE__);
    }

    check_function_help<grammar::plain_statement>(L"function -h");
    check_function_help<grammar::plain_statement>(L"function --help");
    check_function_help<grammar::function_header>(L"function --foo; end");
    check_function_help<grammar::function_header>(L"function foo; end");
}

static void test_new_parser_ad_hoc() {
    // Very ad-hoc tests for issues encountered.
    say(L"Testing new parser ad hoc tests");

    // Ensure that 'case' terminates a job list.
    const wcstring src = L"switch foo ; case bar; case baz; end";
    parse_node_tree_t parse_tree;
    bool success = parse_tree_from_string(src, parse_flag_none, &parse_tree, NULL);
    if (!success) {
        err(L"Parsing failed");
    }

    // Expect three case_item_lists: one for each case, and a terminal one. The bug was that we'd
    // try to run a command 'case'.
    tnode_t<grammar::job_list> root{&parse_tree, &parse_tree.at(0)};
    auto node_list = root.descendants<grammar::case_item_list>();
    if (node_list.size() != 3) {
        err(L"Expected 3 case item nodes, found %lu", node_list.size());
    }
}

static void test_new_parser_errors() {
    say(L"Testing new parser error reporting");
    const struct {
        const wchar_t *src;
        parse_error_code_t code;
    } tests[] = {
        {L"echo 'abc", parse_error_tokenizer_unterminated_quote},
        {L"'", parse_error_tokenizer_unterminated_quote},
        {L"echo (abc", parse_error_tokenizer_unterminated_subshell},

        {L"end", parse_error_unbalancing_end},
        {L"echo hi ; end", parse_error_unbalancing_end},

        {L"else", parse_error_unbalancing_else},
        {L"if true ; end ; else", parse_error_unbalancing_else},

        {L"case", parse_error_unbalancing_case},
        {L"if true ; case ; end", parse_error_unbalancing_case},
    };

    for (size_t i = 0; i < sizeof tests / sizeof *tests; i++) {
        const wcstring src = tests[i].src;
        parse_error_code_t expected_code = tests[i].code;

        parse_error_list_t errors;
        parse_node_tree_t parse_tree;
        bool success = parse_tree_from_string(src, parse_flag_none, &parse_tree, &errors);
        if (success) {
            err(L"Source '%ls' was expected to fail to parse, but succeeded", src.c_str());
        }

        if (errors.size() != 1) {
            err(L"Source '%ls' was expected to produce 1 error, but instead produced %lu errors",
                src.c_str(), errors.size());
        } else if (errors.at(0).code != expected_code) {
            err(L"Source '%ls' was expected to produce error code %lu, but instead produced error "
                L"code %lu",
                src.c_str(), expected_code, (unsigned long)errors.at(0).code);
            for (size_t i = 0; i < errors.size(); i++) {
                err(L"\t\t%ls", errors.at(i).describe(src).c_str());
            }
        }
    }
}

// Given a format string, returns a list of non-empty strings separated by format specifiers. The
// format specifiers themselves are omitted.
static wcstring_list_t separate_by_format_specifiers(const wchar_t *format) {
    wcstring_list_t result;
    const wchar_t *cursor = format;
    const wchar_t *end = format + wcslen(format);
    while (cursor < end) {
        const wchar_t *next_specifier = wcschr(cursor, '%');
        if (next_specifier == NULL) {
            next_specifier = end;
        }
        assert(next_specifier != NULL);

        // Don't return empty strings.
        if (next_specifier > cursor) {
            result.push_back(wcstring(cursor, next_specifier - cursor));
        }

        // Walk over the format specifier (if any).
        cursor = next_specifier;
        if (*cursor != '%') {
            continue;
        }

        cursor++;
        // Flag
        if (wcschr(L"#0- +'", *cursor)) cursor++;
        // Minimum field width
        while (iswdigit(*cursor)) cursor++;
        // Precision
        if (*cursor == L'.') {
            cursor++;
            while (iswdigit(*cursor)) cursor++;
        }
        // Length modifier
        if (!wcsncmp(cursor, L"ll", 2) || !wcsncmp(cursor, L"hh", 2)) {
            cursor += 2;
        } else if (wcschr(L"hljtzqL", *cursor)) {
            cursor++;
        }
        // The format specifier itself. We allow any character except NUL.
        if (*cursor != L'\0') {
            cursor += 1;
        }
        assert(cursor <= end);
    }
    return result;
}

// Given a format string 'format', return true if the string may have been produced by that format
// string. We do this by splitting the format string around the format specifiers, and then ensuring
// that each of the remaining chunks is found (in order) in the string.
static bool string_matches_format(const wcstring &string, const wchar_t *format) {
    bool result = true;
    wcstring_list_t components = separate_by_format_specifiers(format);
    size_t idx = 0;
    for (size_t i = 0; i < components.size(); i++) {
        const wcstring &component = components.at(i);
        size_t where = string.find(component, idx);
        if (where == wcstring::npos) {
            result = false;
            break;
        }
        idx = where + component.size();
        assert(idx <= string.size());
    }
    return result;
}

static void test_error_messages() {
    say(L"Testing error messages");
    const struct error_test_t {
        const wchar_t *src;
        const wchar_t *error_text_format;
    } error_tests[] = {{L"echo $^", ERROR_BAD_VAR_CHAR1},
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
                       {L"echo \"foo$(foo)bar\"", ERROR_BAD_VAR_SUBCOMMAND1}};

    parse_error_list_t errors;
    for (size_t i = 0; i < sizeof error_tests / sizeof *error_tests; i++) {
        const struct error_test_t *test = &error_tests[i];
        errors.clear();
        parse_util_detect_errors(test->src, &errors, false /* allow_incomplete */);
        do_test(!errors.empty());
        if (!errors.empty()) {
            do_test1(string_matches_format(errors.at(0).text, test->error_text_format), test->src);
        }
    }
}

static void test_highlighting() {
    say(L"Testing syntax highlighting");
    if (system("mkdir -p test/fish_highlight_test/")) err(L"mkdir failed");
    if (system("touch test/fish_highlight_test/foo")) err(L"touch failed");
    if (system("touch test/fish_highlight_test/bar")) err(L"touch failed");

    // Here are the components of our source and the colors we expect those to be.
    struct highlight_component_t {
        const wchar_t *txt;
        int color;
        bool nospace;
        highlight_component_t(const wchar_t *txt, int color, bool nospace = false)
            : txt(txt), color(color), nospace(nospace) {}
    };
    const bool ns = true;

    using highlight_component_list_t = std::vector<highlight_component_t>;
    std::vector<highlight_component_list_t> highlight_tests;

    highlight_tests.push_back(
        {{L"echo", highlight_spec_command},
         {L"test/fish_highlight_test/foo", highlight_spec_param | highlight_modifier_valid_path},
         {L"&", highlight_spec_statement_terminator}});

    highlight_tests.push_back({
        {L"command", highlight_spec_command},
        {L"echo", highlight_spec_command},
        {L"abc", highlight_spec_param},
        {L"test/fish_highlight_test/foo", highlight_spec_param | highlight_modifier_valid_path},
        {L"&", highlight_spec_statement_terminator},
    });

    highlight_tests.push_back({
        {L"if command ls", highlight_spec_command},
        {L"; ", highlight_spec_statement_terminator},
        {L"echo", highlight_spec_command},
        {L"abc", highlight_spec_param},
        {L"; ", highlight_spec_statement_terminator},
        {L"/bin/definitely_not_a_command", highlight_spec_error},
        {L"; ", highlight_spec_statement_terminator},
        {L"end", highlight_spec_command},
    });

    // Verify that cd shows errors for non-directories.
    highlight_tests.push_back({
        {L"cd", highlight_spec_command},
        {L"test/fish_highlight_test", highlight_spec_param | highlight_modifier_valid_path},
    });

    highlight_tests.push_back({
        {L"cd", highlight_spec_command},
        {L"test/fish_highlight_test/foo", highlight_spec_error},
    });

    highlight_tests.push_back({
        {L"cd", highlight_spec_command},
        {L"--help", highlight_spec_param},
        {L"-h", highlight_spec_param},
        {L"definitely_not_a_directory", highlight_spec_error},
    });

    // Command substitutions.
    highlight_tests.push_back({
        {L"echo", highlight_spec_command},
        {L"param1", highlight_spec_param},
        {L"(", highlight_spec_operator},
        {L"ls", highlight_spec_command},
        {L"param2", highlight_spec_param},
        {L")", highlight_spec_operator},
        {L"|", highlight_spec_statement_terminator},
        {L"cat", highlight_spec_command},
    });

    // Redirections substitutions.
    highlight_tests.push_back({
        {L"echo", highlight_spec_command},
        {L"param1", highlight_spec_param},

        // Input redirection.
        {L"<", highlight_spec_redirection},
        {L"/bin/echo", highlight_spec_redirection},

        // Output redirection to a valid fd.
        {L"1>&2", highlight_spec_redirection},

        // Output redirection to an invalid fd.
        {L"2>&", highlight_spec_redirection},
        {L"LOL", highlight_spec_error},

        // Just a param, not a redirection.
        {L"test/blah", highlight_spec_param},

        // Input redirection from directory.
        {L"<", highlight_spec_redirection},
        {L"test/", highlight_spec_error},

        // Output redirection to an invalid path.
        {L"3>", highlight_spec_redirection},
        {L"/not/a/valid/path/nope", highlight_spec_error},

        // Output redirection to directory.
        {L"3>", highlight_spec_redirection},
        {L"test/nope/", highlight_spec_error},

        // Redirections to overflow fd.
        {L"99999999999999999999>&2", highlight_spec_error},
        {L"2>&", highlight_spec_redirection},
        {L"99999999999999999999", highlight_spec_error},

        // Output redirection containing a command substitution.
        {L"4>", highlight_spec_redirection},
        {L"(", highlight_spec_operator},
        {L"echo", highlight_spec_command},
        {L"test/somewhere", highlight_spec_param},
        {L")", highlight_spec_operator},

        // Just another param.
        {L"param2", highlight_spec_param},
    });

    highlight_tests.push_back({
        {L"end", highlight_spec_error},
        {L";", highlight_spec_statement_terminator},
        {L"if", highlight_spec_command},
        {L"end", highlight_spec_error},
    });

    highlight_tests.push_back({
        {L"echo", highlight_spec_command},
        {L"'single_quote", highlight_spec_error},
    });

    highlight_tests.push_back({
        {L"echo", highlight_spec_command},
        {L"$foo", highlight_spec_operator},
        {L"\"", highlight_spec_quote},
        {L"$bar", highlight_spec_operator},
        {L"\"", highlight_spec_quote},
        {L"$baz[", highlight_spec_operator},
        {L"1 2..3", highlight_spec_param},
        {L"]", highlight_spec_operator},
    });

    highlight_tests.push_back({
        {L"for", highlight_spec_command},
        {L"i", highlight_spec_param},
        {L"in", highlight_spec_command},
        {L"1 2 3", highlight_spec_param},
        {L";", highlight_spec_statement_terminator},
        {L"end", highlight_spec_command},
    });

    highlight_tests.push_back({
        {L"echo", highlight_spec_command},
        {L"$$foo[", highlight_spec_operator},
        {L"1", highlight_spec_param},
        {L"][", highlight_spec_operator},
        {L"2", highlight_spec_param},
        {L"]", highlight_spec_operator},
        {L"[3]", highlight_spec_param},  // two dollar signs, so last one is not an expansion
    });

    highlight_tests.push_back({
        {L"cat", highlight_spec_command},
        {L"/dev/null", highlight_spec_param},
        {L"|", highlight_spec_statement_terminator},
        {L"less", highlight_spec_command},
        {L"2>", highlight_spec_redirection},
    });

    highlight_tests.push_back({
        {L"if", highlight_spec_command},
        {L"true", highlight_spec_command},
        {L"&&", highlight_spec_operator},
        {L"false", highlight_spec_command},
        {L";", highlight_spec_statement_terminator},
        {L"or", highlight_spec_operator},
        {L"false", highlight_spec_command},
        {L"||", highlight_spec_operator},
        {L"true", highlight_spec_command},
        {L";", highlight_spec_statement_terminator},
        {L"and", highlight_spec_operator},
        {L"not", highlight_spec_operator},
        {L"!", highlight_spec_operator},
        {L"true", highlight_spec_command},
        {L";", highlight_spec_statement_terminator},
        {L"end", highlight_spec_command},
    });

    // Verify variables and wildcards in commands using /bin/cat.
    env_set(L"VARIABLE_IN_COMMAND", ENV_LOCAL, {L"a"});
    env_set(L"VARIABLE_IN_COMMAND2", ENV_LOCAL, {L"at"});
    highlight_tests.push_back(
        {{L"/bin/ca", highlight_spec_command, ns}, {L"*", highlight_spec_operator, ns}});

    highlight_tests.push_back({{L"/bin/c", highlight_spec_command, ns},
                               {L"{$VARIABLE_IN_COMMAND}", highlight_spec_operator, ns},
                               {L"*", highlight_spec_operator, ns}});

    highlight_tests.push_back({{L"/bin/c", highlight_spec_command, ns},
                               {L"{$VARIABLE_IN_COMMAND}", highlight_spec_operator, ns},
                               {L"*", highlight_spec_operator, ns}});

    highlight_tests.push_back({{L"/bin/c", highlight_spec_command, ns},
                               {L"$VARIABLE_IN_COMMAND2", highlight_spec_operator, ns}});

    highlight_tests.push_back({{L"$EMPTY_VARIABLE", highlight_spec_error}});
    highlight_tests.push_back({{L"\"$EMPTY_VARIABLE\"", highlight_spec_error}});

    for (const highlight_component_list_t &components : highlight_tests) {
        // Generate the text.
        wcstring text;
        std::vector<highlight_spec_t> expected_colors;
        for (const highlight_component_t &comp : components) {
            if (!text.empty() && !comp.nospace) {
                text.push_back(L' ');
                expected_colors.push_back(0);
            }
            text.append(comp.txt);
            expected_colors.resize(text.size(), comp.color);
        }
        do_test(expected_colors.size() == text.size());

        std::vector<highlight_spec_t> colors(text.size());
        highlight_shell(text, colors, 20, NULL, env_vars_snapshot_t::current());

        if (expected_colors.size() != colors.size()) {
            err(L"Color vector has wrong size! Expected %lu, actual %lu", expected_colors.size(),
                colors.size());
        }
        do_test(expected_colors.size() == colors.size());
        for (size_t i = 0; i < text.size(); i++) {
            // Hackish space handling. We don't care about the colors in spaces.
            if (text.at(i) == L' ') continue;

            if (expected_colors.at(i) != colors.at(i)) {
                const wcstring spaces(i, L' ');
                err(L"Wrong color in test at index %lu in text (expected %#x, actual "
                    L"%#x):\n%ls\n%ls^",
                    i, expected_colors.at(i), colors.at(i), text.c_str(), spaces.c_str());
            }
        }
    }
    env_remove(L"VARIABLE_IN_COMMAND", ENV_DEFAULT);
    env_remove(L"VARIABLE_IN_COMMAND2", ENV_DEFAULT);
}

static void test_wcstring_tok() {
    say(L"Testing wcstring_tok");
    wcstring buff = L"hello world";
    wcstring needle = L" \t\n";
    wcstring_range loc = wcstring_tok(buff, needle);
    if (loc.first == wcstring::npos || buff.substr(loc.first, loc.second) != L"hello") {
        err(L"Wrong results from first wcstring_tok(): {%zu, %zu}", loc.first, loc.second);
    }
    loc = wcstring_tok(buff, needle, loc);
    if (loc.first == wcstring::npos || buff.substr(loc.first, loc.second) != L"world") {
        err(L"Wrong results from second wcstring_tok(): {%zu, %zu}", loc.first, loc.second);
    }
    loc = wcstring_tok(buff, needle, loc);
    if (loc.first != wcstring::npos) {
        err(L"Wrong results from third wcstring_tok(): {%zu, %zu}", loc.first, loc.second);
    }

    buff = L"hello world";
    loc = wcstring_tok(buff, needle);
    // loc is "hello" again
    loc = wcstring_tok(buff, L"", loc);
    if (loc.first == wcstring::npos || buff.substr(loc.first, loc.second) != L"world") {
        err(L"Wrong results from wcstring_tok with empty needle: {%zu, %zu}", loc.first,
            loc.second);
    }
}

int builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv);
static void run_one_string_test(const wchar_t *const *argv, int expected_rc,
                                const wchar_t *expected_out) {
    parser_t parser;
    io_streams_t streams(0);
    streams.stdin_is_directly_redirected = false;  // read from argv instead of stdin
    int rc = builtin_string(parser, streams, const_cast<wchar_t **>(argv));
    wcstring args;
    for (int i = 0; argv[i] != NULL; i++) {
        args += escape_string(argv[i], ESCAPE_ALL) + L' ';
    }
    args.resize(args.size() - 1);
    if (rc != expected_rc) {
        err(L"Test failed on line %lu: [%ls]: expected return code %d but got %d", __LINE__,
            args.c_str(), expected_rc, rc);
    } else if (streams.out.contents() != expected_out) {
        err(L"Test failed on line %lu: [%ls]: expected [%ls] but got [%ls]", __LINE__, args.c_str(),
            escape_string(expected_out, ESCAPE_ALL).c_str(),
            escape_string(streams.out.contents(), ESCAPE_ALL).c_str());
    }
}

static void test_string() {
    say(L"Testing builtin_string");
    const struct string_test {
        const wchar_t *argv[15];
        int expected_rc;
        const wchar_t *expected_out;
    } string_tests[] = {
        {{L"string", L"escape", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"escape", L"", 0}, STATUS_CMD_OK, L"''\n"},
        {{L"string", L"escape", L"-n", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"escape", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"escape", L"\x07", 0}, STATUS_CMD_OK, L"\\cg\n"},
        {{L"string", L"escape", L"\"x\"", 0}, STATUS_CMD_OK, L"'\"x\"'\n"},
        {{L"string", L"escape", L"hello world", 0}, STATUS_CMD_OK, L"'hello world'\n"},
        {{L"string", L"escape", L"-n", L"hello world", 0}, STATUS_CMD_OK, L"hello\\ world\n"},
        {{L"string", L"escape", L"hello", L"world", 0}, STATUS_CMD_OK, L"hello\nworld\n"},
        {{L"string", L"escape", L"-n", L"~", 0}, STATUS_CMD_OK, L"\\~\n"},

        {{L"string", L"join", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"join", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"join", L"", L"", L"", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"join", L"", L"a", L"b", L"c", 0}, STATUS_CMD_OK, L"abc\n"},
        {{L"string", L"join", L".", L"fishshell", L"com", 0}, STATUS_CMD_OK, L"fishshell.com\n"},
        {{L"string", L"join", L"/", L"usr", 0}, STATUS_CMD_ERROR, L"usr\n"},
        {{L"string", L"join", L"/", L"usr", L"local", L"bin", 0},
         STATUS_CMD_OK,
         L"usr/local/bin\n"},
        {{L"string", L"join", L"...", L"3", L"2", L"1", 0}, STATUS_CMD_OK, L"3...2...1\n"},
        {{L"string", L"join", L"-q", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"join", L"-q", L".", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"join", L"-q", L".", L".", 0}, STATUS_CMD_ERROR, L""},

        {{L"string", L"length", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"length", L"", 0}, STATUS_CMD_ERROR, L"0\n"},
        {{L"string", L"length", L"", L"", L"", 0}, STATUS_CMD_ERROR, L"0\n0\n0\n"},
        {{L"string", L"length", L"a", 0}, STATUS_CMD_OK, L"1\n"},
        {{L"string", L"length", L"\U0002008A", 0}, STATUS_CMD_OK, L"1\n"},
        {{L"string", L"length", L"um", L"dois", L"três", 0}, STATUS_CMD_OK, L"2\n4\n4\n"},
        {{L"string", L"length", L"um", L"dois", L"três", 0}, STATUS_CMD_OK, L"2\n4\n4\n"},
        {{L"string", L"length", L"-q", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"length", L"-q", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"length", L"-q", L"a", 0}, STATUS_CMD_OK, L""},

        {{L"string", L"match", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"match", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"match", L"?", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"match", L"*", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"match", L"**", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"match", L"*", L"xyzzy", 0}, STATUS_CMD_OK, L"xyzzy\n"},
        {{L"string", L"match", L"**", L"plugh", 0}, STATUS_CMD_OK, L"plugh\n"},
        {{L"string", L"match", L"a*b", L"axxb", 0}, STATUS_CMD_OK, L"axxb\n"},
        {{L"string", L"match", L"a??b", L"axxb", 0}, STATUS_CMD_OK, L"axxb\n"},
        {{L"string", L"match", L"-i", L"a??B", L"axxb", 0}, STATUS_CMD_OK, L"axxb\n"},
        {{L"string", L"match", L"-i", L"a??b", L"Axxb", 0}, STATUS_CMD_OK, L"Axxb\n"},
        {{L"string", L"match", L"a*", L"axxb", 0}, STATUS_CMD_OK, L"axxb\n"},
        {{L"string", L"match", L"*a", L"xxa", 0}, STATUS_CMD_OK, L"xxa\n"},
        {{L"string", L"match", L"*a*", L"axa", 0}, STATUS_CMD_OK, L"axa\n"},
        {{L"string", L"match", L"*a*", L"xax", 0}, STATUS_CMD_OK, L"xax\n"},
        {{L"string", L"match", L"*a*", L"bxa", 0}, STATUS_CMD_OK, L"bxa\n"},
        {{L"string", L"match", L"*a", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"match", L"a*", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"match", L"a*b*c", L"axxbyyc", 0}, STATUS_CMD_OK, L"axxbyyc\n"},
        {{L"string", L"match", L"\\*", L"*", 0}, STATUS_CMD_OK, L"*\n"},
        {{L"string", L"match", L"a*\\", L"abc\\", 0}, STATUS_CMD_OK, L"abc\\\n"},
        {{L"string", L"match", L"a*\\?", L"abc?", 0}, STATUS_CMD_OK, L"abc?\n"},

        {{L"string", L"match", L"?", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"?", L"ab", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"??", L"a", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"?a", L"a", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"a?", L"a", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"a??B", L"axxb", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"a*b", L"axxbc", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"*b", L"bbba", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"0x[0-9a-fA-F][0-9a-fA-F]", L"0xbad", 0}, STATUS_CMD_ERROR, L""},

        {{L"string", L"match", L"-a", L"*", L"ab", L"cde", 0}, STATUS_CMD_OK, L"ab\ncde\n"},
        {{L"string", L"match", L"*", L"ab", L"cde", 0}, STATUS_CMD_OK, L"ab\ncde\n"},
        {{L"string", L"match", L"-n", L"*d*", L"cde", 0}, STATUS_CMD_OK, L"1 3\n"},
        {{L"string", L"match", L"-n", L"*x*", L"cde", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"-q", L"a*", L"b", L"c", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"-q", L"a*", L"b", L"a", 0}, STATUS_CMD_OK, L""},

        {{L"string", L"match", L"-r", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"match", L"-r", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"-r", L"", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"match", L"-r", L".", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"match", L"-r", L".*", L"", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"match", L"-r", L"a*b", L"b", 0}, STATUS_CMD_OK, L"b\n"},
        {{L"string", L"match", L"-r", L"a*b", L"aab", 0}, STATUS_CMD_OK, L"aab\n"},
        {{L"string", L"match", L"-r", L"-i", L"a*b", L"Aab", 0}, STATUS_CMD_OK, L"Aab\n"},
        {{L"string", L"match", L"-r", L"-a", L"a[bc]", L"abadac", 0}, STATUS_CMD_OK, L"ab\nac\n"},
        {{L"string", L"match", L"-r", L"a", L"xaxa", L"axax", 0}, STATUS_CMD_OK, L"a\na\n"},
        {{L"string", L"match", L"-r", L"-a", L"a", L"xaxa", L"axax", 0},
         STATUS_CMD_OK,
         L"a\na\na\na\n"},
        {{L"string", L"match", L"-r", L"a[bc]", L"abadac", 0}, STATUS_CMD_OK, L"ab\n"},
        {{L"string", L"match", L"-r", L"-q", L"a[bc]", L"abadac", 0}, STATUS_CMD_OK, L""},
        {{L"string", L"match", L"-r", L"-q", L"a[bc]", L"ad", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"-r", L"(a+)b(c)", L"aabc", 0}, STATUS_CMD_OK, L"aabc\naa\nc\n"},
        {{L"string", L"match", L"-r", L"-a", L"(a)b(c)", L"abcabc", 0},
         STATUS_CMD_OK,
         L"abc\na\nc\nabc\na\nc\n"},
        {{L"string", L"match", L"-r", L"(a)b(c)", L"abcabc", 0}, STATUS_CMD_OK, L"abc\na\nc\n"},
        {{L"string", L"match", L"-r", L"(a|(z))(bc)", L"abc", 0}, STATUS_CMD_OK, L"abc\na\nbc\n"},
        {{L"string", L"match", L"-r", L"-n", L"a", L"ada", L"dad", 0},
         STATUS_CMD_OK,
         L"1 1\n2 1\n"},
        {{L"string", L"match", L"-r", L"-n", L"-a", L"a", L"bacadae", 0},
         STATUS_CMD_OK,
         L"2 1\n4 1\n6 1\n"},
        {{L"string", L"match", L"-r", L"-n", L"(a).*(b)", L"a---b", 0},
         STATUS_CMD_OK,
         L"1 5\n1 1\n5 1\n"},
        {{L"string", L"match", L"-r", L"-n", L"(a)(b)", L"ab", 0},
         STATUS_CMD_OK,
         L"1 2\n1 1\n2 1\n"},
        {{L"string", L"match", L"-r", L"-n", L"(a)(b)", L"abab", 0},
         STATUS_CMD_OK,
         L"1 2\n1 1\n2 1\n"},
        {{L"string", L"match", L"-r", L"-n", L"-a", L"(a)(b)", L"abab", 0},
         STATUS_CMD_OK,
         L"1 2\n1 1\n2 1\n3 2\n3 1\n4 1\n"},
        {{L"string", L"match", L"-r", L"*", L"", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"match", L"-r", L"-a", L"a*", L"b", 0}, STATUS_CMD_OK, L"\n\n"},
        {{L"string", L"match", L"-r", L"foo\\Kbar", L"foobar", 0}, STATUS_CMD_OK, L"bar\n"},
        {{L"string", L"match", L"-r", L"(foo)\\Kbar", L"foobar", 0}, STATUS_CMD_OK, L"bar\nfoo\n"},
        {{L"string", L"match", L"-r", L"(?=ab\\K)", L"ab", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"match", L"-r", L"(?=ab\\K)..(?=cd\\K)", L"abcd", 0}, STATUS_CMD_OK, L"\n"},

        {{L"string", L"replace", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"replace", L"", L"", L"", 0}, STATUS_CMD_ERROR, L"\n"},
        {{L"string", L"replace", L"", L"", L" ", 0}, STATUS_CMD_ERROR, L" \n"},
        {{L"string", L"replace", L"a", L"b", L"", 0}, STATUS_CMD_ERROR, L"\n"},
        {{L"string", L"replace", L"a", L"b", L"a", 0}, STATUS_CMD_OK, L"b\n"},
        {{L"string", L"replace", L"a", L"b", L"xax", 0}, STATUS_CMD_OK, L"xbx\n"},
        {{L"string", L"replace", L"a", L"b", L"xax", L"axa", 0}, STATUS_CMD_OK, L"xbx\nbxa\n"},
        {{L"string", L"replace", L"bar", L"x", L"red barn", 0}, STATUS_CMD_OK, L"red xn\n"},
        {{L"string", L"replace", L"x", L"bar", L"red xn", 0}, STATUS_CMD_OK, L"red barn\n"},
        {{L"string", L"replace", L"--", L"x", L"-", L"xyz", 0}, STATUS_CMD_OK, L"-yz\n"},
        {{L"string", L"replace", L"--", L"y", L"-", L"xyz", 0}, STATUS_CMD_OK, L"x-z\n"},
        {{L"string", L"replace", L"--", L"z", L"-", L"xyz", 0}, STATUS_CMD_OK, L"xy-\n"},
        {{L"string", L"replace", L"-i", L"z", L"X", L"_Z_", 0}, STATUS_CMD_OK, L"_X_\n"},
        {{L"string", L"replace", L"-a", L"a", L"A", L"aaa", 0}, STATUS_CMD_OK, L"AAA\n"},
        {{L"string", L"replace", L"-i", L"a", L"z", L"AAA", 0}, STATUS_CMD_OK, L"zAA\n"},
        {{L"string", L"replace", L"-q", L"x", L">x<", L"x", 0}, STATUS_CMD_OK, L""},
        {{L"string", L"replace", L"-a", L"x", L"", L"xxx", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"replace", L"-a", L"***", L"_", L"*****", 0}, STATUS_CMD_OK, L"_**\n"},
        {{L"string", L"replace", L"-a", L"***", L"***", L"******", 0}, STATUS_CMD_OK, L"******\n"},
        {{L"string", L"replace", L"-a", L"a", L"b", L"xax", L"axa", 0},
         STATUS_CMD_OK,
         L"xbx\nbxb\n"},

        {{L"string", L"replace", L"-r", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"-r", L"", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"-r", L"", L"", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"replace", L"-r", L"", L"", L"", 0}, STATUS_CMD_OK, L"\n"},  // pcre2 behavior
        {{L"string", L"replace", L"-r", L"", L"", L" ", 0},
         STATUS_CMD_OK,
         L" \n"},  // pcre2 behavior
        {{L"string", L"replace", L"-r", L"a", L"b", L"", 0}, STATUS_CMD_ERROR, L"\n"},
        {{L"string", L"replace", L"-r", L"a", L"b", L"a", 0}, STATUS_CMD_OK, L"b\n"},
        {{L"string", L"replace", L"-r", L".", L"x", L"abc", 0}, STATUS_CMD_OK, L"xbc\n"},
        {{L"string", L"replace", L"-r", L".", L"", L"abc", 0}, STATUS_CMD_OK, L"bc\n"},
        {{L"string", L"replace", L"-r", L"(\\w)(\\w)", L"$2$1", L"ab", 0}, STATUS_CMD_OK, L"ba\n"},
        {{L"string", L"replace", L"-r", L"(\\w)", L"$1$1", L"ab", 0}, STATUS_CMD_OK, L"aab\n"},
        {{L"string", L"replace", L"-r", L"-a", L".", L"x", L"abc", 0}, STATUS_CMD_OK, L"xxx\n"},
        {{L"string", L"replace", L"-r", L"-a", L"(\\w)", L"$1$1", L"ab", 0},
         STATUS_CMD_OK,
         L"aabb\n"},
        {{L"string", L"replace", L"-r", L"-a", L".", L"", L"abc", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"replace", L"-r", L"a", L"x", L"bc", L"cd", L"de", 0},
         STATUS_CMD_ERROR,
         L"bc\ncd\nde\n"},
        {{L"string", L"replace", L"-r", L"a", L"x", L"aba", L"caa", 0},
         STATUS_CMD_OK,
         L"xba\ncxa\n"},
        {{L"string", L"replace", L"-r", L"-a", L"a", L"x", L"aba", L"caa", 0},
         STATUS_CMD_OK,
         L"xbx\ncxx\n"},
        {{L"string", L"replace", L"-r", L"-i", L"A", L"b", L"xax", 0}, STATUS_CMD_OK, L"xbx\n"},
        {{L"string", L"replace", L"-r", L"-i", L"[a-z]", L".", L"1A2B", 0},
         STATUS_CMD_OK,
         L"1.2B\n"},
        {{L"string", L"replace", L"-r", L"A", L"b", L"xax", 0}, STATUS_CMD_ERROR, L"xax\n"},
        {{L"string", L"replace", L"-r", L"a", L"$1", L"a", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"-r", L"(a)", L"$2", L"a", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"-r", L"*", L".", L"a", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"replace", L"-ra", L"x", L"\\c", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"replace", L"-r", L"^(.)", L"\t$1", L"abc", L"x", 0},
         STATUS_CMD_OK,
         L"\tabc\n\tx\n"},

        {{L"string", L"split", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"split", L":", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"split", L".", L"www.ch.ic.ac.uk", 0},
         STATUS_CMD_OK,
         L"www\nch\nic\nac\nuk\n"},
        {{L"string", L"split", L"..", L"....", 0}, STATUS_CMD_OK, L"\n\n\n"},
        {{L"string", L"split", L"-m", L"x", L"..", L"....", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"split", L"-m1", L"..", L"....", 0}, STATUS_CMD_OK, L"\n..\n"},
        {{L"string", L"split", L"-m0", L"/", L"/usr/local/bin/fish", 0},
         STATUS_CMD_ERROR,
         L"/usr/local/bin/fish\n"},
        {{L"string", L"split", L"-m2", L":", L"a:b:c:d", L"e:f:g:h", 0},
         STATUS_CMD_OK,
         L"a\nb\nc:d\ne\nf\ng:h\n"},
        {{L"string", L"split", L"-m1", L"-r", L"/", L"/usr/local/bin/fish", 0},
         STATUS_CMD_OK,
         L"/usr/local/bin\nfish\n"},
        {{L"string", L"split", L"-r", L".", L"www.ch.ic.ac.uk", 0},
         STATUS_CMD_OK,
         L"www\nch\nic\nac\nuk\n"},
        {{L"string", L"split", L"--", L"--", L"a--b---c----d", 0},
         STATUS_CMD_OK,
         L"a\nb\n-c\n\nd\n"},
        {{L"string", L"split", L"-r", L"..", L"....", 0}, STATUS_CMD_OK, L"\n\n\n"},
        {{L"string", L"split", L"-r", L"--", L"--", L"a--b---c----d", 0},
         STATUS_CMD_OK,
         L"a\nb-\nc\n\nd\n"},
        {{L"string", L"split", L"", L"", 0}, STATUS_CMD_ERROR, L"\n"},
        {{L"string", L"split", L"", L"a", 0}, STATUS_CMD_ERROR, L"a\n"},
        {{L"string", L"split", L"", L"ab", 0}, STATUS_CMD_OK, L"a\nb\n"},
        {{L"string", L"split", L"", L"abc", 0}, STATUS_CMD_OK, L"a\nb\nc\n"},
        {{L"string", L"split", L"-m1", L"", L"abc", 0}, STATUS_CMD_OK, L"a\nbc\n"},
        {{L"string", L"split", L"-r", L"", L"", 0}, STATUS_CMD_ERROR, L"\n"},
        {{L"string", L"split", L"-r", L"", L"a", 0}, STATUS_CMD_ERROR, L"a\n"},
        {{L"string", L"split", L"-r", L"", L"ab", 0}, STATUS_CMD_OK, L"a\nb\n"},
        {{L"string", L"split", L"-r", L"", L"abc", 0}, STATUS_CMD_OK, L"a\nb\nc\n"},
        {{L"string", L"split", L"-r", L"-m1", L"", L"abc", 0}, STATUS_CMD_OK, L"ab\nc\n"},
        {{L"string", L"split", L"-q", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"split", L"-q", L":", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"split", L"-q", L"x", L"axbxc", 0}, STATUS_CMD_OK, L""},

        {{L"string", L"sub", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"sub", L"abcde", 0}, STATUS_CMD_OK, L"abcde\n"},
        {{L"string", L"sub", L"-l", L"x", L"abcde", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"sub", L"-s", L"x", L"abcde", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"sub", L"-l0", L"abcde", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"sub", L"-l2", L"abcde", 0}, STATUS_CMD_OK, L"ab\n"},
        {{L"string", L"sub", L"-l5", L"abcde", 0}, STATUS_CMD_OK, L"abcde\n"},
        {{L"string", L"sub", L"-l6", L"abcde", 0}, STATUS_CMD_OK, L"abcde\n"},
        {{L"string", L"sub", L"-l-1", L"abcde", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"sub", L"-s0", L"abcde", 0}, STATUS_INVALID_ARGS, L""},
        {{L"string", L"sub", L"-s1", L"abcde", 0}, STATUS_CMD_OK, L"abcde\n"},
        {{L"string", L"sub", L"-s5", L"abcde", 0}, STATUS_CMD_OK, L"e\n"},
        {{L"string", L"sub", L"-s6", L"abcde", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"sub", L"-s-1", L"abcde", 0}, STATUS_CMD_OK, L"e\n"},
        {{L"string", L"sub", L"-s-5", L"abcde", 0}, STATUS_CMD_OK, L"abcde\n"},
        {{L"string", L"sub", L"-s-6", L"abcde", 0}, STATUS_CMD_OK, L"abcde\n"},
        {{L"string", L"sub", L"-s1", L"-l0", L"abcde", 0}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"sub", L"-s1", L"-l1", L"abcde", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"sub", L"-s2", L"-l2", L"abcde", 0}, STATUS_CMD_OK, L"bc\n"},
        {{L"string", L"sub", L"-s-1", L"-l1", L"abcde", 0}, STATUS_CMD_OK, L"e\n"},
        {{L"string", L"sub", L"-s-1", L"-l2", L"abcde", 0}, STATUS_CMD_OK, L"e\n"},
        {{L"string", L"sub", L"-s-3", L"-l2", L"abcde", 0}, STATUS_CMD_OK, L"cd\n"},
        {{L"string", L"sub", L"-s-3", L"-l4", L"abcde", 0}, STATUS_CMD_OK, L"cde\n"},
        {{L"string", L"sub", L"-q", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"sub", L"-q", L"abcde", 0}, STATUS_CMD_OK, L""},

        {{L"string", L"trim", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"trim", L""}, STATUS_CMD_ERROR, L"\n"},
        {{L"string", L"trim", L" "}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"trim", L"  \f\n\r\t"}, STATUS_CMD_OK, L"\n"},
        {{L"string", L"trim", L" a"}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"a "}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L" a "}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-l", L" a"}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-l", L"a "}, STATUS_CMD_ERROR, L"a \n"},
        {{L"string", L"trim", L"-l", L" a "}, STATUS_CMD_OK, L"a \n"},
        {{L"string", L"trim", L"-r", L" a"}, STATUS_CMD_ERROR, L" a\n"},
        {{L"string", L"trim", L"-r", L"a "}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-r", L" a "}, STATUS_CMD_OK, L" a\n"},
        {{L"string", L"trim", L"-c", L".", L" a"}, STATUS_CMD_ERROR, L" a\n"},
        {{L"string", L"trim", L"-c", L".", L"a "}, STATUS_CMD_ERROR, L"a \n"},
        {{L"string", L"trim", L"-c", L".", L" a "}, STATUS_CMD_ERROR, L" a \n"},
        {{L"string", L"trim", L"-c", L".", L".a"}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-c", L".", L"a."}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-c", L".", L".a."}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-c", L"\\/", L"/a\\"}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-c", L"\\/", L"a/"}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-c", L"\\/", L"\\a/"}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"trim", L"-c", L"", L".a."}, STATUS_CMD_ERROR, L".a.\n"}};

    for (const auto &t : string_tests) {
        run_one_string_test(t.argv, t.expected_rc, t.expected_out);
    }

    const auto saved_flags = fish_features();
    const struct string_test qmark_noglob_tests[] = {
        {{L"string", L"match", L"a*b?c", L"axxb?c", 0}, STATUS_CMD_OK, L"axxb?c\n"},
        {{L"string", L"match", L"*?", L"a", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"*?", L"ab", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"?*", L"a", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"?*", L"ab", 0}, STATUS_CMD_ERROR, L""},
        {{L"string", L"match", L"a*\\?", L"abc?", 0}, STATUS_CMD_ERROR, L""}};
    mutable_fish_features().set(features_t::qmark_noglob, true);
    for (const auto &t : qmark_noglob_tests) {
        run_one_string_test(t.argv, t.expected_rc, t.expected_out);
    }

    const struct string_test qmark_glob_tests[] = {
        {{L"string", L"match", L"a*b?c", L"axxbyc", 0}, STATUS_CMD_OK, L"axxbyc\n"},
        {{L"string", L"match", L"*?", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"match", L"*?", L"ab", 0}, STATUS_CMD_OK, L"ab\n"},
        {{L"string", L"match", L"?*", L"a", 0}, STATUS_CMD_OK, L"a\n"},
        {{L"string", L"match", L"?*", L"ab", 0}, STATUS_CMD_OK, L"ab\n"},
        {{L"string", L"match", L"a*\\?", L"abc?", 0}, STATUS_CMD_OK, L"abc?\n"}};
    mutable_fish_features().set(features_t::qmark_noglob, false);
    for (const auto &t : qmark_glob_tests) {
        run_one_string_test(t.argv, t.expected_rc, t.expected_out);
    }
    mutable_fish_features() = saved_flags;
}

/// Helper for test_timezone_env_vars().
long return_timezone_hour(time_t tstamp, const wchar_t *timezone) {
    struct tm ltime;
    char ltime_str[3];
    char *str_ptr;
    size_t n;

    env_set_one(L"TZ", ENV_EXPORT, timezone);

    const auto var = env_get(L"TZ", ENV_DEFAULT);
    (void)var;

    localtime_r(&tstamp, &ltime);
    n = strftime(ltime_str, 3, "%H", &ltime);
    if (n != 2) {
        err(L"strftime() returned %d, expected 2", n);
        return 0;
    }
    return strtol(ltime_str, &str_ptr, 10);
}

/// Verify that setting special env vars have the expected effect on the current shell process.
static void test_timezone_env_vars() {
    // Confirm changing the timezone affects fish's idea of the local time.
    time_t tstamp = time(NULL);

    long first_tstamp = return_timezone_hour(tstamp, L"UTC-1");
    long second_tstamp = return_timezone_hour(tstamp, L"UTC-2");
    long delta = second_tstamp - first_tstamp;
    if (delta != 1 && delta != -23) {
        err(L"expected a one hour timezone delta got %ld", delta);
    }
}

/// Verify that setting special env vars have the expected effect on the current shell process.
static void test_env_vars() {
    test_timezone_env_vars();
    // TODO: Add tests for the locale and ncurses vars.
}

static void test_illegal_command_exit_code() {
    say(L"Testing illegal command exit code");

    // We need to be in an empty directory so that none of the wildcards match a file that might be
    // in the fish source tree. In particular we need to ensure that "?" doesn't match a file
    // named by a single character. See issue #3852.
    if (!pushd("test/temp")) return;

    struct command_result_tuple_t {
        const wchar_t *txt;
        int result;
    };

    const command_result_tuple_t tests[] = {
        {L"echo -n", STATUS_CMD_OK},
        {L"pwd", STATUS_CMD_OK},
        {L"UNMATCHABLE_WILDCARD*", STATUS_UNMATCHED_WILDCARD},
        {L"UNMATCHABLE_WILDCARD**", STATUS_UNMATCHED_WILDCARD},
        {L"?", STATUS_UNMATCHED_WILDCARD},
        {L"abc?def", STATUS_UNMATCHED_WILDCARD},
    };

    int res = 0;
    const io_chain_t empty_ios;
    parser_t &parser = parser_t::principal_parser();

    for (const auto &test : tests) {
        res = parser.eval(test.txt, empty_ios, TOP);

        int exit_status = proc_get_last_status();
        if (exit_status != test.result) {
            err(L"command '%ls': expected exit code %d, got %d", test.txt, test.result,
                exit_status);
        }
    }

    popd();
}

void test_maybe() {
    say(L"Testing maybe_t");
    do_test(!bool(maybe_t<int>()));
    maybe_t<int> m(3);
    do_test(m.has_value());
    do_test(m.value() == 3);
    m.reset();
    do_test(!m.has_value());
    m = 123;
    do_test(m.has_value());
    do_test(m.has_value() && m.value() == 123);
    m = maybe_t<int>();
    do_test(!m.has_value());
    m = maybe_t<int>(64);
    do_test(m.has_value() && m.value() == 64);
    m = 123;
    do_test(m == maybe_t<int>(123));
    do_test(m != maybe_t<int>());
    do_test(maybe_t<int>() == none());
    do_test(!maybe_t<int>(none()).has_value());
    m = none();
    do_test(!bool(m));

    maybe_t<std::string> m2("abc");
    do_test(!m2.missing_or_empty());
    m2 = "";
    do_test(m2.missing_or_empty());
    m2 = none();
    do_test(m2.missing_or_empty());

    maybe_t<std::string> m0 = none();
    maybe_t<std::string> m3("hi");
    maybe_t<std::string> m4 = m3;
    do_test(m4 && *m4 == "hi");
    maybe_t<std::string> m5 = m0;
    do_test(!m5);

    maybe_t<std::string> acquire_test("def");
    do_test(acquire_test);
    std::string res = acquire_test.acquire();
    do_test(!acquire_test);
    do_test(res == "def");
}

void test_layout_cache() {
    layout_cache_t seqs;

    // Verify escape code cache.
    do_test(seqs.find_escape_code(L"abc") == 0);
    seqs.add_escape_code(L"abc");
    seqs.add_escape_code(L"abc");
    do_test(seqs.esc_cache_size() == 1);
    do_test(seqs.find_escape_code(L"abc") == 3);
    do_test(seqs.find_escape_code(L"abcd") == 3);
    do_test(seqs.find_escape_code(L"abcde") == 3);
    do_test(seqs.find_escape_code(L"xabcde") == 0);
    seqs.add_escape_code(L"ac");
    do_test(seqs.find_escape_code(L"abcd") == 3);
    do_test(seqs.find_escape_code(L"acbd") == 2);
    seqs.add_escape_code(L"wxyz");
    do_test(seqs.find_escape_code(L"abc") == 3);
    do_test(seqs.find_escape_code(L"abcd") == 3);
    do_test(seqs.find_escape_code(L"wxyz123") == 4);
    do_test(seqs.find_escape_code(L"qwxyz123") == 0);
    do_test(seqs.esc_cache_size() == 3);
    seqs.clear();
    do_test(seqs.esc_cache_size() == 0);
    do_test(seqs.find_escape_code(L"abcd") == 0);

    // Verify prompt layout cache.
    for (size_t i = 0; i < layout_cache_t::prompt_cache_max_size; i++) {
        wcstring input = std::to_wstring(i);
        do_test(!seqs.find_prompt_layout(input));
        seqs.add_prompt_layout(input, {i});
        do_test(seqs.find_prompt_layout(input)->line_count == i);
    }

    size_t expected_evictee = 3;
    for (size_t i = 0; i < layout_cache_t::prompt_cache_max_size; i++) {
        if (i != expected_evictee)
            do_test(seqs.find_prompt_layout(std::to_wstring(i))->line_count == i);
    }

    seqs.add_prompt_layout(L"whatever", {100});
    do_test(!seqs.find_prompt_layout(std::to_wstring(expected_evictee)));
    do_test(seqs.find_prompt_layout(L"whatever")->line_count == 100);
}

void test_normalize_path() {
    say(L"Testing path normalization");
    do_test(normalize_path(L"") == L".");
    do_test(normalize_path(L"..") == L"..");
    do_test(normalize_path(L"./") == L".");
    do_test(normalize_path(L"////abc") == L"//abc");
    do_test(normalize_path(L"/abc") == L"/abc");
    do_test(normalize_path(L"/abc/") == L"/abc");
    do_test(normalize_path(L"/abc/..def/") == L"/abc/..def");
    do_test(normalize_path(L"//abc/../def/") == L"//def");
    do_test(normalize_path(L"abc/../abc/../abc/../abc") == L"abc");
    do_test(normalize_path(L"../../") == L"../..");
    do_test(normalize_path(L"foo/./bar") == L"foo/bar");
    do_test(normalize_path(L"foo/././bar/.././baz") == L"foo/baz");
}

/// Main test.
int main(int argc, char **argv) {
    UNUSED(argc);
    // Look for the file tests/test.fish. We expect to run in a directory containing that file.
    // If we don't find it, walk up the directory hierarchy until we do, or error.
    while (access("./tests/test.fish", F_OK) != 0) {
        char wd[PATH_MAX + 1] = {};
        if (!getcwd(wd, sizeof wd)) {
            perror("getcwd");
            exit(-1);
        }
        if (!strcmp(wd, "/")) {
            fwprintf(stderr,
                     L"Unable to find 'tests' directory, which should contain file test.fish\n");
            exit(EXIT_FAILURE);
        }
        if (chdir(dirname(wd)) < 0) {
            perror("chdir");
        }
    }

    srand((unsigned int)time(NULL));
    configure_thread_assertions_for_testing();

    // Set the program name to this sentinel value
    // This will prevent some misleading stderr output during the tests
    program_name = TESTS_PROGRAM_NAME;
    s_arguments = argv + 1;

    struct utsname uname_info;
    uname(&uname_info);

    say(L"Testing low-level functionality");
    set_main_thread();
    setup_fork_guards();
    proc_init();
    builtin_init();
    env_init();
    misc_init();
    reader_init();

    // Set default signal handlers, so we can ctrl-C out of this.
    signal_reset_handlers();

    if (should_test_function("utility_functions")) test_utility_functions();
    if (should_test_function("wcstring_tok")) test_wcstring_tok();
    if (should_test_function("env_vars")) test_env_vars();
    if (should_test_function("str_to_num")) test_str_to_num();
    if (should_test_function("highlighting")) test_highlighting();
    if (should_test_function("new_parser_ll2")) test_new_parser_ll2();
    if (should_test_function("new_parser_fuzzing"))
        test_new_parser_fuzzing();  // fuzzing is expensive
    if (should_test_function("new_parser_correctness")) test_new_parser_correctness();
    if (should_test_function("new_parser_ad_hoc")) test_new_parser_ad_hoc();
    if (should_test_function("new_parser_errors")) test_new_parser_errors();
    if (should_test_function("error_messages")) test_error_messages();
    if (should_test_function("escape")) test_unescape_sane();
    if (should_test_function("escape")) test_escape_crazy();
    if (should_test_function("escape")) test_escape_quotes();
    if (should_test_function("format")) test_format();
    if (should_test_function("convert")) test_convert();
    if (should_test_function("convert_nulls")) test_convert_nulls();
    if (should_test_function("tok")) test_tokenizer();
    if (should_test_function("iothread")) test_iothread();
    if (should_test_function("parser")) test_parser();
    if (should_test_function("cancellation")) test_cancellation();
    if (should_test_function("indents")) test_indents();
    if (should_test_function("utf8")) test_utf8();
    if (should_test_function("feature_flags")) test_feature_flags();
    if (should_test_function("escape_sequences")) test_escape_sequences();
    if (should_test_function("lru")) test_lru();
    if (should_test_function("expand")) test_expand();
    if (should_test_function("fuzzy_match")) test_fuzzy_match();
    if (should_test_function("abbreviations")) test_abbreviations();
    if (should_test_function("test")) test_test();
    if (should_test_function("path")) test_path();
    if (should_test_function("pager_navigation")) test_pager_navigation();
    if (should_test_function("pager_layout")) test_pager_layout();
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
    if (should_test_function("history")) history_tests_t::test_history();
    if (should_test_function("history_merge")) history_tests_t::test_history_merge();
    if (should_test_function("history_races")) history_tests_t::test_history_races();
    if (should_test_function("history_formats")) history_tests_t::test_history_formats();
    if (should_test_function("string")) test_string();
    if (should_test_function("illegal_command_exit_code")) test_illegal_command_exit_code();
    if (should_test_function("maybe")) test_maybe();
    if (should_test_function("layout_cache")) test_layout_cache();
    if (should_test_function("normalize")) test_normalize_path();
    // history_tests_t::test_history_speed();

    say(L"Encountered %d errors in low-level tests", err_count);
    if (s_test_run_count == 0) say(L"*** No Tests Were Actually Run! ***");

    proc_destroy();

    if (err_count != 0) {
        return 1;
    }
}
