/** \file builtin_string.cpp
  Implementation of the string builtin.
*/

#include "config.h" // IWYU pragma: keep

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif
#include "pcre2.h"

#include "builtin.h"
#include "common.h"
#include "parser.h"
#include "parse_util.h"
#include "wgetopt.h"
#include "wildcard.h"
#include "wutil.h"
#include <unistd.h>

#define MAX_REPLACE_SIZE    size_t(1048576)  // pcre2_substitute maximum output size in wchar_t
#define STRING_ERR_MISSING  _(L"%ls: Expected argument\n")

/* externs from builtin.cpp */
extern int builtin_count_args(const wchar_t * const * argv);
extern wcstring stdout_buffer, stderr_buffer;
void builtin_print_help(parser_t &parser, const wchar_t *cmd, wcstring &b);
extern int builtin_stdin;


enum
{
    BUILTIN_STRING_OK = 0,
    BUILTIN_STRING_NONE = 1,
    BUILTIN_STRING_ERROR = 2
};

static void string_error(const wchar_t *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    stderr_buffer.append(L"string ");
    append_formatv(stderr_buffer, fmt, va);
    va_end(va);
}

static void string_unknown_option(parser_t &parser, const wchar_t *subcmd, const wchar_t *opt)
{
    string_error(BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_help(parser, L"string", stderr_buffer);
}

static bool string_args_from_stdin()
{
    return builtin_stdin != STDIN_FILENO || !isatty(builtin_stdin);
}

static const wchar_t *string_get_arg_stdin(wcstring *storage)
{
    std::string arg;
    for (;;)
    {
        char ch = '\0';
        long rc = read_blocked(builtin_stdin, &ch, 1);

        if (rc < 0)
        {
            // failure
            return 0;
        }

        if (rc == 0)
        {
            // eof
            if (arg.empty())
            {
                return 0;
            }
            else
            {
                break;
            }
        }

        if (ch == '\n')
        {
            break;
        }

        arg += ch;
    }
    
    *storage = str2wcstring(arg);
    return storage->c_str();
}

static const wchar_t *string_get_arg_argv(int *argidx, wchar_t **argv)
{
    return (argv && argv[*argidx]) ? argv[(*argidx)++] : 0;
}

static const wchar_t *string_get_arg(int *argidx, wchar_t **argv, wcstring *storage)
{
    if (string_args_from_stdin())
    {
        return string_get_arg_stdin(storage);
    }
    else
    {
        return string_get_arg_argv(argidx, argv);
    }
}

static int string_escape(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"n";
    const struct woption long_options[] =
    {
        { L"no-quoted", no_argument, 0, 'n' },
        { 0, 0, 0, 0 }
    };

    escape_flags_t flags = ESCAPE_ALL;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'n':
                flags |= ESCAPE_NO_QUOTED;
                break;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nesc = 0;
    wcstring storage;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        stdout_buffer += escape(arg, flags);
        stdout_buffer += L'\n';
        nesc++;
    }

    return (nesc > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_join(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"q";
    const struct woption long_options[] =
    {
        { L"quiet", no_argument, 0, 'q'},
        { 0, 0, 0, 0 }
    };

    bool quiet = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'q':
                quiet = true;
                break;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0)
    {
        string_error(STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nargs = 0;
    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        if (!quiet)
        {
            stdout_buffer += arg;
            stdout_buffer += sep;
        }
        nargs++;
    }
    if (nargs > 0 && !quiet)
    {
        stdout_buffer.resize(stdout_buffer.length() - wcslen(sep));
        stdout_buffer += L'\n';
    }

    return (nargs > 1) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_length(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"q";
    const struct woption long_options[] =
    {
        { L"quiet", no_argument, 0, 'q'},
        { 0, 0, 0, 0 }
    };

    bool quiet = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'q':
                quiet = true;
                break;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int nnonempty = 0;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        size_t n = wcslen(arg);
        if (n > 0)
        {
            nnonempty++;
        }
        if (!quiet)
        {
            stdout_buffer += to_string(int(n));
            stdout_buffer += L'\n';
        }
    }

    return (nnonempty > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

struct match_options_t
{
    bool all;
    bool ignore_case;
    bool index;
    bool quiet;

    match_options_t(): all(false), ignore_case(false), index(false), quiet(false) { }
};

class string_matcher_t
{
protected:
    match_options_t opts;
    int total_matched;

public:
    string_matcher_t(const match_options_t &opts_)
        : opts(opts_), total_matched(0)
    { }

    virtual ~string_matcher_t() { }
    virtual bool report_matches(const wchar_t *arg) = 0;
    int match_count() { return total_matched; }
};

class wildcard_matcher_t: public string_matcher_t
{
    wcstring wcpattern;

public:
    wildcard_matcher_t(const wchar_t * /*argv0*/, const wchar_t *pattern, const match_options_t &opts)
        : string_matcher_t(opts)
    {
        wcpattern = parse_util_unescape_wildcards(pattern);

        if (opts.ignore_case)
        {
            for (int i = 0; i < wcpattern.length(); i++)
            {
                wcpattern[i] = towlower(wcpattern[i]);
            }
        }
    }

    virtual ~wildcard_matcher_t() { }

    bool report_matches(const wchar_t *arg)
    {
        // Note: --all is a no-op for glob matching since the pattern is always
        // matched against the entire argument
        bool match;
        if (opts.ignore_case)
        {
            wcstring s = arg;
            for (int i = 0; i < s.length(); i++)
            {
                s[i] = towlower(s[i]);
            }
            match = wildcard_match(s, wcpattern, false);
        }
        else
        {
            match = wildcard_match(arg, wcpattern, false);
        }
        if (match)
        {
            total_matched++;
        }
        if (!opts.quiet)
        {
            if (match)
            {
                if (opts.index)
                {
                    stdout_buffer += L"1 ";
                    stdout_buffer += to_string(wcslen(arg));
                    stdout_buffer += L'\n';
                }
                else
                {
                    stdout_buffer += arg;
                    stdout_buffer += L'\n';
                }
            }
        }
        return true;
    }
};

static wcstring pcre2_strerror(int err_code)
{
    wchar_t buf[128];
    pcre2_get_error_message(err_code, (PCRE2_UCHAR *)buf, sizeof(buf) / sizeof(wchar_t));
    return buf;
}

struct compiled_regex_t
{
    pcre2_code *code;
    pcre2_match_data *match;

    compiled_regex_t(const wchar_t *argv0, const wchar_t *pattern, bool ignore_case)
        : code(0), match(0)
    {
        // Disable some sequences that can lead to security problems
        uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        code = pcre2_compile(
            PCRE2_SPTR(pattern),
            PCRE2_ZERO_TERMINATED,
            options | (ignore_case ? PCRE2_CASELESS : 0),
            &err_code,
            &err_offset,
            0);
        if (code == 0)
        {
            string_error(_(L"%ls: Regular expression compile error: %ls\n"),
                    argv0, pcre2_strerror(err_code).c_str());
            string_error(L"%ls: %ls\n", argv0, pattern);
            string_error(L"%ls: %*ls\n", argv0, err_offset, L"^");
            return;
        }

        match = pcre2_match_data_create_from_pattern(code, 0);
        if (match == 0)
        {
            DIE_MEM();
        }
    }

    ~compiled_regex_t()
    {
        if (match != 0)
        {
            pcre2_match_data_free(match);
        }
        if (code != 0)
        {
            pcre2_code_free(code);
        }
    }
};

class pcre2_matcher_t: public string_matcher_t
{
    const wchar_t *argv0;
    compiled_regex_t regex;

    int report_match(const wchar_t *arg, int pcre2_rc)
    {
        // Return values: -1 = error, 0 = no match, 1 = match
        if (pcre2_rc == PCRE2_ERROR_NOMATCH)
        {
            return 0;
        }
        if (pcre2_rc < 0)
        {
            string_error(_(L"%ls: Regular expression match error: %ls\n"),
                    argv0, pcre2_strerror(pcre2_rc).c_str());
            return -1;
        }
        if (pcre2_rc == 0)
        {
            // The output vector wasn't big enough. Should not happen.
            string_error(_(L"%ls: Regular expression internal error\n"), argv0);
            return -1;
        }
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);
        for (int j = 0; j < pcre2_rc; j++)
        {
            PCRE2_SIZE begin = ovector[2*j];
            PCRE2_SIZE end = ovector[2*j + 1];
            if (!opts.quiet)
            {
                if (begin != PCRE2_UNSET && end != PCRE2_UNSET)
                {
                    if (opts.index)
                    {
                        stdout_buffer += to_string(begin + 1);
                        stdout_buffer += ' ';
                        stdout_buffer += to_string(end - begin);
                    }
                    else if (end > begin) // may have end < begin if \K is used
                    {
                        stdout_buffer += wcstring(&arg[begin], end - begin);
                    }
                    stdout_buffer += L'\n';
                }
            }
        }
        return 1;
    }

public:
    pcre2_matcher_t(const wchar_t *argv0_, const wchar_t *pattern, const match_options_t &opts)
        : string_matcher_t(opts),
          argv0(argv0_),
          regex(argv0_, pattern, opts.ignore_case)
    { }

    virtual ~pcre2_matcher_t() { }

    bool report_matches(const wchar_t *arg)
    {
        // A return value of true means all is well (even if no matches were
        // found), false indicates an unrecoverable error.
        if (regex.code == 0)
        {
            // pcre2_compile() failed
            return false;
        }

        int matched = 0;

        // See pcre2demo.c for an explanation of this logic
        PCRE2_SIZE arglen = wcslen(arg);
        int rc = report_match(arg, pcre2_match(regex.code, PCRE2_SPTR(arg), arglen, 0, 0, regex.match, 0));
        if (rc < 0)
        {
            // pcre2 match error
            return false;
        }
        if (rc == 0)
        {
            // no match
            return true;
        }
        matched++;
        total_matched++;

        // Report any additional matches
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);
        while (opts.all || matched == 0)
        {
            uint32_t options = 0;
            PCRE2_SIZE offset = ovector[1]; // Start at end of previous match

            if (ovector[0] == ovector[1])
            {
                if (ovector[0] == arglen)
                {
                    break;
                }
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }

            rc = report_match(arg, pcre2_match(regex.code, PCRE2_SPTR(arg), arglen, offset, options, regex.match, 0));
            if (rc < 0)
            {
                return false;
            }
            if (rc == 0)
            {
                if (options == 0)
                {
                    // All matches found
                    break;
                }
                ovector[1] = offset + 1;
                continue;
            }
            matched++;
            total_matched++;
        }
        return true;
    }
};

static int string_match(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"ainqr";
    const struct woption long_options[] =
    {
        { L"all", no_argument, 0, 'a'},
        { L"ignore-case", no_argument, 0, 'i'},
        { L"index", no_argument, 0, 'n'},
        { L"quiet", no_argument, 0, 'q'},
        { L"regex", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    match_options_t opts;
    bool regex = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'a':
                opts.all = true;
                break;

            case 'i':
                opts.ignore_case = true;
                break;

            case 'n':
                opts.index = true;
                break;

            case 'q':
                opts.quiet = true;
                break;

            case 'r':
                regex = true;
                break;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *pattern;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0)
    {
        string_error(STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    string_matcher_t *matcher;
    if (regex)
    {
        matcher = new pcre2_matcher_t(argv[0], pattern, opts);
    }
    else
    {
        matcher = new wildcard_matcher_t(argv[0], pattern, opts);
    }

    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        if (!matcher->report_matches(arg))
        {
            delete matcher;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = matcher->match_count() > 0 ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
    delete matcher;
    return rc;
}

struct replace_options_t
{
    bool all;
    bool ignore_case;
    bool quiet;

    replace_options_t(): all(false), ignore_case(false), quiet(false) { }
};

class string_replacer_t
{
protected:
    const wchar_t *argv0;
    replace_options_t opts;
    int total_replaced;

public:
    string_replacer_t(const wchar_t *argv0_, const replace_options_t &opts_)
        : argv0(argv0_), opts(opts_), total_replaced(0)
    { }

    virtual ~string_replacer_t() {}
    virtual bool replace_matches(const wchar_t *arg) = 0;
    int replace_count() { return total_replaced; }
};

class literal_replacer_t: public string_replacer_t
{
    const wchar_t *pattern;
    const wchar_t *replacement;
    size_t patlen;

public:
    literal_replacer_t(const wchar_t *argv0, const wchar_t *pattern_, const wchar_t *replacement_,
                        const replace_options_t &opts)
        : string_replacer_t(argv0, opts),
          pattern(pattern_), replacement(replacement_), patlen(wcslen(pattern))
    { }

    virtual ~literal_replacer_t() { }

    bool replace_matches(const wchar_t *arg)
    {
        wcstring result;
        if (patlen == 0)
        {
            result = arg;
        }
        else
        {
            int replaced = 0;
            const wchar_t *cur = arg;
            while (*cur != L'\0')
            {
                if ((opts.all || replaced == 0) &&
                    (opts.ignore_case ? wcsncasecmp(cur, pattern, patlen) : wcsncmp(cur, pattern, patlen)) == 0)
                {
                    result += replacement;
                    cur += patlen;
                    replaced++;
                    total_replaced++;
                }
                else
                {
                    result += *cur;
                    cur++;
                }
            }
        }
        if (!opts.quiet)
        {
            stdout_buffer += result;
            stdout_buffer += L'\n';
        }
        return true;
    }
};

class regex_replacer_t: public string_replacer_t
{
    compiled_regex_t regex;
    wcstring replacement;

    wcstring interpret_escapes(const wchar_t *orig)
    {
        wcstring result;

        while (*orig != L'\0')
        {
            if (*orig == L'\\')
            {
                orig += read_unquoted_escape(orig, &result, true, false);
            }
            else
            {
                result += *orig;
                orig++;
            }
        }

        return result;
    }

public:
    regex_replacer_t(const wchar_t *argv0, const wchar_t *pattern, const wchar_t *replacement_,
                        const replace_options_t &opts)
        : string_replacer_t(argv0, opts),
          regex(argv0, pattern, opts.ignore_case),
          replacement(interpret_escapes(replacement_))
    { }

    virtual ~regex_replacer_t() { }

    bool replace_matches(const wchar_t *arg)
    {
        // A return value of true means all is well (even if no replacements
        // were performed), false indicates an unrecoverable error.
        if (regex.code == 0)
        {
            // pcre2_compile() failed
            return false;
        }

        uint32_t options = opts.all ? PCRE2_SUBSTITUTE_GLOBAL : 0;
        size_t arglen = wcslen(arg);
        PCRE2_SIZE outlen = (arglen == 0) ? 16 : 2 * arglen;
        wchar_t *output = (wchar_t *)malloc(sizeof(wchar_t) * outlen);
        if (output == 0)
        {
            DIE_MEM();
        }
        int pcre2_rc = 0;
        for (;;)
        {
            pcre2_rc = pcre2_substitute(
                            regex.code,
                            PCRE2_SPTR(arg),
                            arglen,
                            0,  // start offset
                            options,
                            regex.match,
                            0,  // match context
                            PCRE2_SPTR(replacement.c_str()),
                            PCRE2_ZERO_TERMINATED,
                            (PCRE2_UCHAR *)output,
                            &outlen);

            if (pcre2_rc == PCRE2_ERROR_NOMEMORY)
            {
                if (outlen < MAX_REPLACE_SIZE)
                {
                    outlen = std::min(2 * outlen, MAX_REPLACE_SIZE);
                    output = (wchar_t *)realloc(output, sizeof(wchar_t) * outlen);
                    if (output == 0)
                    {
                        DIE_MEM();
                    }
                    continue;
                }
                string_error(_(L"%ls: Replacement string too large\n"), argv0);
                free(output);
                return false;
            }
            break;
        }

        bool rc = true;
        if (pcre2_rc < 0)
        {
            string_error(_(L"%ls: Regular expression substitute error: %ls\n"),
                    argv0, pcre2_strerror(pcre2_rc).c_str());
            rc = false;
        }
        else
        {
            if (!opts.quiet)
            {
                stdout_buffer += output;
                stdout_buffer += L'\n';
            }
            total_replaced += pcre2_rc;
        }

        free(output);
        return rc;
    }
};

static int string_replace(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"aiqr";
    const struct woption long_options[] =
    {
        { L"all", no_argument, 0, 'a'},
        { L"ignore-case", no_argument, 0, 'i'},
        { L"quiet", no_argument, 0, 'q'},
        { L"regex", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    replace_options_t opts;
    bool regex = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'a':
                opts.all = true;
                break;

            case 'i':
                opts.ignore_case = true;
                break;

            case 'q':
                opts.quiet = true;
                break;

            case 'r':
                regex = true;
                break;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *pattern, *replacement;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0)
    {
        string_error(STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }
    if ((replacement = string_get_arg_argv(&i, argv)) == 0)
    {
        string_error(STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    string_replacer_t *replacer;
    if (regex)
    {
        replacer = new regex_replacer_t(argv[0], pattern, replacement, opts);
    }
    else
    {
        replacer = new literal_replacer_t(argv[0], pattern, replacement, opts);
    }

    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        if (!replacer->replace_matches(arg))
        {
            delete replacer;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = replacer->replace_count() > 0 ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
    delete replacer;
    return rc;
}

static int string_split(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":m:qr";
    const struct woption long_options[] =
    {
        { L"max", required_argument, 0, 'm'},
        { L"quiet", no_argument, 0, 'q'},
        { L"right", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    long max = LONG_MAX;
    bool quiet = false;
    bool right = false;
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'm':
            {
                errno = 0;
                wchar_t *endptr = 0;
                max = wcstol(w.woptarg, &endptr, 10);
                if (*endptr != L'\0' || errno != 0)
                {
                    string_error(BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                break;
            }

            case 'q':
                quiet = true;
                break;

            case 'r':
                right = true;
                break;

            case ':':
                string_error(STRING_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0)
    {
        string_error(STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    // we may want to use push_front or push_back, so do not use vector
    std::list<wcstring> splits;
    size_t seplen = wcslen(sep);
    int nsplit = 0;
    const wchar_t *arg;
    if (right)
    {
        wcstring storage;
        while ((arg = string_get_arg(&i, argv, &storage)) != 0)
        {
            int nargsplit = 0;
            if (seplen == 0)
            {
                // Split to individual characters
                const wchar_t *cur = arg + wcslen(arg) - 1;
                while (cur > arg && nargsplit < max)
                {
                    splits.push_front(wcstring(cur, 1));
                    cur--;
                    nargsplit++;
                    nsplit++;
                }
                splits.push_front(wcstring(arg, cur - arg + 1));
            }
            else
            {
                const wchar_t *end = arg + wcslen(arg);
                const wchar_t *cur = end - seplen;
                while (cur >= arg && nargsplit < max)
                {
                    if (wcsncmp(cur, sep, seplen) == 0)
                    {
                        splits.push_front(wcstring(cur + seplen, end - cur - seplen));
                        end = cur;
                        cur -= seplen;
                        nargsplit++;
                        nsplit++;
                    }
                    else
                    {
                        cur--;
                    }
                }
                splits.push_front(wcstring(arg, end - arg));
            }
        }
    }
    else
    {
        wcstring storage;
        while ((arg = string_get_arg(&i, argv, &storage)) != 0)
        {
            const wchar_t *cur = arg;
            int nargsplit = 0;
            if (seplen == 0)
            {
                // Split to individual characters
                const wchar_t *last = arg + wcslen(arg) - 1;
                while (cur < last && nargsplit < max)
                {
                    splits.push_back(wcstring(cur, 1));
                    cur++;
                    nargsplit++;
                    nsplit++;
                }
                splits.push_back(cur);
            }
            else
            {
                while (cur != 0)
                {
                    const wchar_t *ptr = (nargsplit < max) ? wcsstr(cur, sep) : 0;
                    if (ptr == 0)
                    {
                        splits.push_back(cur);
                        cur = 0;
                    }
                    else
                    {
                        splits.push_back(wcstring(cur, ptr - cur));
                        cur = ptr + seplen;
                        nargsplit++;
                        nsplit++;
                    }
                }
            }
        }
    }

    if (!quiet)
    {
        std::list<wcstring>::const_iterator si = splits.begin();
        while (si != splits.end())
        {
            stdout_buffer += *si;
            stdout_buffer += L'\n';
            si++;
        }
    }

    return (nsplit > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_sub(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":l:qs:";
    const struct woption long_options[] =
    {
        { L"length", required_argument, 0, 'l'},
        { L"quiet", no_argument, 0, 'q'},
        { L"start", required_argument, 0, 's'},
        { 0, 0, 0, 0 }
    };

    int start = 0;
    int length = -1;
    bool quiet = false;
    wgetopter_t w;
    wchar_t *endptr = 0;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'l':
                errno = 0;
                length = int(wcstol(w.woptarg, &endptr, 10));
                if (*endptr != L'\0' || errno != 0)
                {
                    string_error(BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                if (length < 0)
                {
                    string_error(_(L"%ls: Invalid length value '%d'\n"), argv[0], length);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case 'q':
                quiet = true;
                break;

            case 's':
                errno = 0;
                start = int(wcstol(w.woptarg, &endptr, 10));
                if (*endptr != L'\0' || errno != 0)
                {
                    string_error(BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                if (start == 0)
                {
                    string_error(_(L"%ls: Invalid start value '%d'\n"), argv[0], start);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case ':':
                string_error(STRING_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nsub = 0;
    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        wcstring::size_type pos = 0;
        wcstring::size_type count = wcstring::npos;
        wcstring s(arg);
        if (start > 0)
        {
            pos = start - 1;
        }
        else if (start < 0)
        {
            wcstring::size_type n = -start;
            pos = n > s.length() ? 0 : s.length() - n;
        }
        if (pos > s.length())
        {
            pos = s.length();
        }

        if (length >= 0)
        {
            count = length;
        }
        if (pos + count > s.length())
        {
            count = wcstring::npos;
        }

        if (!quiet)
        {
            stdout_buffer += s.substr(pos, count);
            stdout_buffer += L'\n';
        }
        nsub++;
    }

    return (nsub > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_trim(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L":c:lqr";
    const struct woption long_options[] =
    {
        { L"chars", required_argument, 0, 'c'},
        { L"left", no_argument, 0, 'l'},
        { L"quiet", no_argument, 0, 'q'},
        { L"right", no_argument, 0, 'r'},
        { 0, 0, 0, 0 }
    };

    int leftright = 0;
    bool quiet = false;
    wcstring chars = L" \f\n\r\t";
    wgetopter_t w;
    for (;;)
    {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1)
        {
            break;
        }
        switch (c)
        {
            case 0:
                break;

            case 'c':
                chars = w.woptarg;
                break;

            case 'l':
                leftright |= 1;
                break;

            case 'q':
                quiet = true;
                break;

            case 'r':
                leftright |= 2;
                break;

            case ':':
                string_error(STRING_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                string_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin() && argc > i)
    {
        string_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int ntrim = 0;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage)) != 0)
    {
        const wchar_t *begin = arg;
        const wchar_t *end = arg + wcslen(arg);
        if (!leftright || (leftright & 1))
        {
            while (begin != end && chars.find_first_of(begin, 0, 1) != wcstring::npos)
            {
                begin++;
                ntrim++;
            }
        }
        if (!leftright || (leftright & 2))
        {
            while (begin != end && chars.find_first_of(end - 1, 0, 1) != wcstring::npos)
            {
                end--;
                ntrim++;
            }
        }
        if (!quiet)
        {
            stdout_buffer += wcstring(begin, end - begin);
            stdout_buffer += L'\n';
        }
    }

    return (ntrim > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static const struct string_subcommand
{
    const wchar_t *name;
    int (*handler)(parser_t &, int argc, wchar_t **argv);
}
string_subcommands[] =
{
    { L"escape", &string_escape },
    { L"join", &string_join },
    { L"length", &string_length },
    { L"match", &string_match },
    { L"replace", &string_replace },
    { L"split", &string_split },
    { L"sub", &string_sub },
    { L"trim", &string_trim },
    { 0, 0 }
};

/**
   The string builtin, for manipulating strings.
*/
/*static*/ int builtin_string(parser_t &parser, wchar_t **argv)
{
    int argc = builtin_count_args(argv);
    if (argc <= 1)
    {
        string_error(STRING_ERR_MISSING, argv[0]);
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_ERROR;
    }

    if (wcscmp(argv[1], L"-h") == 0 || wcscmp(argv[1], L"--help") == 0)
    {
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_OK;
    }

    const string_subcommand *subcmd = &string_subcommands[0];
    while (subcmd->name != 0 && wcscmp(subcmd->name, argv[1]) != 0)
    {
        subcmd++;
    }
    if (subcmd->handler == 0)
    {
        string_error(_(L"%ls: Unknown subcommand '%ls'\n"), argv[0], argv[1]);
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_ERROR;
    }

    argc--;
    argv++;
    return subcmd->handler(parser, argc, argv);
}
