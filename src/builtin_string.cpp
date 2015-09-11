/** \file builtin_string.cpp
  Implementation of the string builtin.
*/

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif
#include "pcre2.h"

#define MAX_REPLACE_SIZE size_t(1048576)  // pcre2_substitute maximum output size in wchar_t

enum
{
    BUILTIN_STRING_OK = STATUS_BUILTIN_OK,
    BUILTIN_STRING_ERROR = STATUS_BUILTIN_ERROR
};

static void string_fatal_error(const wchar_t *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    wcstring errstr = vformat_string(fmt, va);
    va_end(va);

    if (!errstr.empty() && errstr.at(errstr.length() - 1) != L'\n')
    {
        errstr += L'\n';
    }

    stderr_buffer += errstr;
}

static const wchar_t *string_get_arg_stdin()
{
    static wcstring arg;

    arg.clear();

    bool eof = false;
    bool gotarg = false;

    for (;;)
    {
        wchar_t wch = L'\0';
        mbstate_t state = {};
        for (;;)
        {
            char ch = '\0';
            if (read_blocked(builtin_stdin, &ch, 1) <= 0)
            {
                eof = true;
                break;
            }
            else
            {
                size_t n = mbrtowc(&wch, &ch, 1, &state);
                if (n == size_t(-1))
                {
                    // Invalid multibyte sequence: start over
                    memset(&state, 0, sizeof(state));
                }
                else if (n == size_t(-2))
                {
                    // Incomplete sequence: continue reading
                }
                else
                {
                    // Got a complete char (could be L'\0')
                    break;
                }
            }
        }

        if (eof)
        {
            break;
        }

        if (wch == L'\n')
        {
            gotarg = true;
            break;
        }

        arg += wch;
    }

    return gotarg ? arg.c_str() : 0;
}

static const wchar_t *string_get_arg_argv(int *argidx, wchar_t **argv)
{
    return (argv && argv[*argidx]) ? argv[(*argidx)++] : 0;
}

static inline const wchar_t *string_get_arg(int *argidx, wchar_t **argv)
{
    if (isatty(builtin_stdin))
    {
        return string_get_arg_argv(argidx, argv);
    }
    else
    {
        return string_get_arg_stdin();
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
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nesc = 0;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        stdout_buffer += escape(arg, flags);
        stdout_buffer += L'\n';
        nesc++;
    }

    return (nesc > 0) ? 0 : 1;
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
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nargs = 0;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
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

    return (nargs > 1) ? 0 : 1;
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
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int nnonempty = 0;
    while ((arg = string_get_arg(&i, argv)) != 0)
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

    return (nnonempty > 0) ? 0 : 1;
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
    const wchar_t *argv0;
    int nmatch;

public:
    string_matcher_t(const wchar_t *argv0_, const match_options_t &opts_)
        : opts(opts_), argv0(argv0_), nmatch(0)
    { }

    virtual ~string_matcher_t() { }
    virtual bool report_matches(const wchar_t *arg) = 0;
    int match_count() { return nmatch; }
};

class wildcard_matcher_t: public string_matcher_t
{
    const wchar_t *pattern;

    bool arg_matches(const wchar_t *pat, const wchar_t *arg)
    {
        for (; *arg != L'\0'; arg++, pat++)
        {
            switch (*pat)
            {
                case L'?':
                    break;

                case L'*':
                    // skip redundant *
                    while (*pat == L'*')
                    {
                        pat++;
                    }

                    // * at end matches whatever follows
                    if (*pat == L'\0')
                    {
                        return true;
                    }

                    while (*arg != L'\0')
                    {
                        if (arg_matches(pat, arg++))
                        {
                            return true;
                        }
                    }
                    return false;

                case L'[':
                {
                    bool negate = false;
                    if (*++pat == L'^')
                    {
                        negate = true;
                        pat++;
                    }

                    bool match = false;
                    wchar_t argch = opts.ignore_case ? towlower(*arg) : *arg;
                    wchar_t patch, patch2;
                    while ((patch = *pat++) != L']')
                    {
                        if (patch == L'\0')
                        {
                            return false; // no closing ]
                        }
                        if (*pat == L'-' && (patch2 = *(pat + 1)) != L'\0' && patch2 != L']')
                        {
                            if (opts.ignore_case ? towlower(patch) <= argch && argch <= towlower(patch2)
                                                 : patch <= argch && argch <= patch2)
                            {
                                match = true;
                            }
                            pat += 2;
                        }
                        else if (patch == argch)
                        {
                            match = true;
                        }
                    }
                    if (match == negate)
                    {
                        return false;
                    }
                    pat--;
                    break;
                }

                case L'\\':
                    if (*(pat + 1) != L'\0')
                    {
                        pat++;
                    }
                    // fall through

                default:
                    if (opts.ignore_case ? towlower(*arg) != towlower(*pat) : *arg != *pat)
                    {
                        return false;
                    }
                    break;
            }
        }
        // arg is exhausted - it's a match only if pattern is as well
        while (*pat == L'*')
        {
            pat++;
        }
        return *pat == L'\0';
    }

public:
    wildcard_matcher_t(const wchar_t *argv0_, const match_options_t &opts_, const wchar_t *pattern_)
        : string_matcher_t(argv0_, opts_),
          pattern(pattern_)
    { }

    virtual ~wildcard_matcher_t() { }

    bool report_matches(const wchar_t *arg)
    {
        if (opts.all || nmatch == 0)
        {
            bool match = arg_matches(pattern, arg);
            if (match)
            {
                nmatch++;
            }
            if (!opts.quiet)
            {
                if (match)
                {
                    if (opts.index)
                    {
                        stdout_buffer += L"1\n";
                    }
                    else
                    {
                        stdout_buffer += arg;
                        stdout_buffer += L'\n';
                    }
                }
            }
        }
        return true;
    }
};

class pcre2_matcher_t: public string_matcher_t
{
    pcre2_code *regex;
    pcre2_match_data *match;

    int report_match(const wchar_t *arg, int pcre2_rc)
    {
        // Return values: -1 = error, 0 = no match, 1 = match
        if (pcre2_rc == PCRE2_ERROR_NOMATCH)
        {
            return 0;
        }
        if (pcre2_rc < 0)
        {
            // see http://www.pcre.org/current/doc/html/pcre2api.html#SEC30
            string_fatal_error(_(L"%ls: Regular expression match error %d"), argv0, pcre2_rc);
            return -1;
        }
        if (pcre2_rc == 0)
        {
            // The output vector wasn't big enough. Should not happen.
            string_fatal_error(_(L"%ls: Regular expression internal error"), argv0);
            return -1;
        }
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match);
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
    pcre2_matcher_t(const wchar_t *argv0_, const match_options_t &opts_, const wchar_t *pattern)
        : string_matcher_t(argv0_, opts_),
          regex(0), match(0)
    {
        // Disable some sequences that can lead to security problems
        uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        regex = pcre2_compile(
            PCRE2_SPTR(pattern),
            PCRE2_ZERO_TERMINATED,
            options | (opts.ignore_case ? PCRE2_CASELESS : 0),
            &err_code,
            &err_offset,
            0);
        if (regex == 0)
        {
            string_fatal_error(_(L"%ls: Regular expression compilation failed at offset %d"),
                argv0, int(err_offset));
            return;
        }

        match = pcre2_match_data_create_from_pattern(regex, 0);
        if (match == 0)
        {
            DIE_MEM();
        }
    }

    virtual ~pcre2_matcher_t()
    {
        if (match != 0)
        {
            pcre2_match_data_free(match);
        }
        if (regex != 0)
        {
            pcre2_code_free(regex);
        }
    }

    bool report_matches(const wchar_t *arg)
    {
        // A return value of true means all is well (even if no matches were
        // found), false indicates an unrecoverable error.
        if (regex == 0)
        {
            // pcre2_compile() failed
            return false;
        }

        if (!opts.all && nmatch > 0)
        {
            return true;
        }

        // See pcre2demo.c for an explanation of this logic
        PCRE2_SIZE arglen = wcslen(arg);
        int rc = report_match(arg, pcre2_match(regex, PCRE2_SPTR(arg), arglen, 0, 0, match, 0));
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
        nmatch++;

        // Report any additional matches
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match);
        while (opts.all || nmatch == 0)
        {
            uint32_t options = 0;
            PCRE2_SIZE offset = ovector[1]; // Start at end of previous match
            PCRE2_SIZE old_offset = pcre2_get_startchar(match);
            if (offset <= old_offset)
            {
                offset = old_offset + 1;
            }

            if (ovector[0] == ovector[1])
            {
                if (ovector[0] == arglen)
                {
                    break;
                }
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }

            rc = report_match(arg, pcre2_match(regex, PCRE2_SPTR(arg), arglen, offset, options, match, 0));
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
            nmatch++;
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
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *pattern;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    string_matcher_t *matcher;
    if (regex)
    {
        matcher = new pcre2_matcher_t(argv[0], opts, pattern);
    }
    else
    {
        matcher = new wildcard_matcher_t(argv[0], opts, pattern);
    }

    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        if (!matcher->report_matches(arg))
        {
            delete matcher;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = matcher->match_count() > 0 ? 0 : 1;
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
    replace_options_t opts;
    const wchar_t *argv0;
    int nreplace;

public:
    string_replacer_t(const wchar_t *argv0_, const replace_options_t &opts_)
        : opts(opts_), argv0(argv0_), nreplace(0)
    { }

    virtual ~string_replacer_t() {}
    virtual bool replace_matches(const wchar_t *arg) = 0;
    int replace_count() { return nreplace; }
};

class literal_replacer_t: public string_replacer_t
{
    const wchar_t *pattern;
    const wchar_t *replacement;
    int patlen;

public:
    literal_replacer_t(const wchar_t *argv0_, const replace_options_t &opts_, const wchar_t *pattern_,
                        const wchar_t *replacement_)
        : string_replacer_t(argv0_, opts_),
          pattern(pattern_), replacement(replacement_), patlen(wcslen(pattern))
    { }

    virtual ~literal_replacer_t() { }

    bool replace_matches(const wchar_t *arg)
    {
        wcstring replaced;
        if (patlen == 0)
        {
            replaced = arg;
        }
        else
        {
            const wchar_t *cur = arg;
            while (*cur != L'\0')
            {
                if ((opts.all || nreplace == 0) &&
                    (opts.ignore_case ? wcsncasecmp(cur, pattern, patlen) : wcsncmp(cur, pattern, patlen)) == 0)
                {
                    replaced += replacement;
                    cur += patlen;
                    nreplace++;
                }
                else
                {
                    replaced += *cur;
                    cur++;
                }
            }
        }
        if (!opts.quiet)
        {
            stdout_buffer += replaced;
            stdout_buffer += L'\n';
        }
        return true;
    }
};

class regex_replacer_t: public string_replacer_t
{
    pcre2_code *regex;
    pcre2_match_data *match;
    const wchar_t *replacement;

public:
    regex_replacer_t(const wchar_t *argv0_, const replace_options_t &opts_, const wchar_t *pattern,
                      const wchar_t *replacement_)
        : string_replacer_t(argv0_, opts_),
          regex(0), match(0), replacement(replacement_)
    {
        // Disable some sequences that can lead to security problems
        uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        regex = pcre2_compile(
            PCRE2_SPTR(pattern),
            PCRE2_ZERO_TERMINATED,
            options | (opts.ignore_case ? PCRE2_CASELESS : 0),
            &err_code,
            &err_offset,
            0);
        if (regex == 0)
        {
            string_fatal_error(_(L"%ls: Regular expression compilation failed at offset %d"),
                argv0, int(err_offset));
            return;
        }

        match = pcre2_match_data_create_from_pattern(regex, 0);
        if (match == 0)
        {
            DIE_MEM();
        }
    }

    virtual ~regex_replacer_t()
    {
        if (match != 0)
        {
            pcre2_match_data_free(match);
        }
        if (regex != 0)
        {
            pcre2_code_free(regex);
        }
    }

    bool replace_matches(const wchar_t *arg)
    {
        // A return value of true means all is well (even if no replacements
        // were performed), false indicates an unrecoverable error.
        if (regex == 0)
        {
            // pcre2_compile() failed
            return false;
        }

        if (!opts.all && nreplace > 0)
        {
            if (!opts.quiet)
            {
                stdout_buffer += arg;
                stdout_buffer += L'\n';
            }
            return true;
        }

        uint32_t options = opts.all ? PCRE2_SUBSTITUTE_GLOBAL : 0;
        int arglen = wcslen(arg);
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
                            regex,
                            PCRE2_SPTR(arg),
                            arglen,
                            0,  // start offset
                            options,
                            match,
                            0,  // match context
                            PCRE2_SPTR(replacement),
                            PCRE2_ZERO_TERMINATED,
                            (PCRE2_UCHAR *)output,
                            &outlen);

            if (pcre2_rc == PCRE2_ERROR_NOMEMORY)
            {
                if (outlen < MAX_REPLACE_SIZE)
                {
                    outlen = std::max(2 * outlen, MAX_REPLACE_SIZE);
                    output = (wchar_t *)realloc(output, sizeof(wchar_t) * outlen);
                    if (output == 0)
                    {
                        DIE_MEM();
                    }
                    continue;
                }
                string_fatal_error(_(L"%ls: Replacement string too large"), argv0);
                free(output);
                return false;
            }
            break;
        }

        bool rc = true;
        if (pcre2_rc == PCRE2_ERROR_BADREPLACEMENT)
        {
            string_fatal_error(_(L"%ls: Invalid use of $ in replacement string"), argv0);
            rc = false;
        }
        else if (pcre2_rc < 0)
        {
            string_fatal_error(_(L"%ls: Regular expression match error %d"), argv0, pcre2_rc);
            rc = false;
        }
        else
        {
            if (!opts.quiet)
            {
                stdout_buffer += output;
                stdout_buffer += L'\n';
            }
            nreplace += pcre2_rc;
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
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *pattern, *replacement;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }
    if ((replacement = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    string_replacer_t *replacer;
    if (regex)
    {
        replacer = new regex_replacer_t(argv[0], opts, pattern, replacement);
    }
    else
    {
        replacer = new literal_replacer_t(argv[0], opts, pattern, replacement);
    }

    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
    {
        if (!replacer->replace_matches(arg))
        {
            delete replacer;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = replacer->replace_count() > 0 ? 0 : 1;
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

    int max = 0;
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
                max = int(wcstol(w.woptarg, 0, 10));
                break;

            case 'q':
                quiet = true;
                break;

            case 'r':
                right = true;
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0)
    {
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    std::list<wcstring> splits;
    int seplen = wcslen(sep);
    int nsplit = 0;
    const wchar_t *arg;
    if (right)
    {
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            if (seplen == 0)
            {
                // Split to individual characters
                const wchar_t *cur = arg + wcslen(arg) - 1;
                while (cur > arg && (max == 0 || nsplit < max))
                {
                    splits.push_front(wcstring(cur, 1));
                    cur--;
                    nsplit++;
                }
                splits.push_front(wcstring(arg, cur - arg + 1));
            }
            else
            {
                const wchar_t *end = arg + wcslen(arg);
                const wchar_t *cur = end - seplen;
                while (cur >= arg && (max == 0 || nsplit < max))
                {
                    if (wcsncmp(cur, sep, seplen) == 0)
                    {
                        splits.push_front(wcstring(cur + seplen, end - cur - seplen));
                        end = cur;
                        cur -= seplen;
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
        while ((arg = string_get_arg(&i, argv)) != 0)
        {
            const wchar_t *cur = arg;
            if (seplen == 0)
            {
                // Split to individual characters
                const wchar_t *last = arg + wcslen(arg) - 1;
                while (cur < last && (max == 0 || nsplit < max))
                {
                    splits.push_back(wcstring(cur, 1));
                    cur++;
                    nsplit++;
                }
                splits.push_back(cur);
            }
            else
            {
                while (cur != 0)
                {
                    const wchar_t *ptr = (max > 0 && nsplit >= max) ? 0 : wcsstr(cur, sep);
                    if (ptr == 0)
                    {
                        splits.push_back(cur);
                        cur = 0;
                    }
                    else
                    {
                        splits.push_back(wcstring(cur, ptr - cur));
                        cur = ptr + seplen;
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

    return (nsplit > 0) ? 0 : 1;
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
                length = int(wcstol(w.woptarg, 0, 10));
                if (length < 0)
                {
                    string_fatal_error(_(L"%ls: Invalid length value\n"), argv[0]);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case 'q':
                quiet = true;
                break;

            case 's':
                start = int(wcstol(w.woptarg, 0, 10));
                if (start == 0)
                {
                    string_fatal_error(_(L"%ls: Invalid start value\n"), argv[0]);
                    return BUILTIN_STRING_ERROR;
                }
                break;

            case ':':
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nsub = 0;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv)) != 0)
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

    return (nsub > 0) ? 0 : 1;
}

static int string_trim(parser_t &parser, int argc, wchar_t **argv)
{
    const wchar_t *short_options = L"c:lqr";
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
                string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;

            case '?':
                builtin_unknown_option(parser, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
        }
    }

    int i = w.woptind;
    if (!isatty(builtin_stdin) && argc > i)
    {
        string_fatal_error(BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int ntrim = 0;
    while ((arg = string_get_arg(&i, argv)) != 0)
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

    return (ntrim > 0) ? 0 : 1;
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
        string_fatal_error(BUILTIN_ERR_MISSING, argv[0]);
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
        string_fatal_error(_(L"%ls: Unknown subcommand '%ls'"), argv[0], argv[1]);
        builtin_print_help(parser, L"string", stderr_buffer);
        return BUILTIN_STRING_ERROR;
    }

    argc--;
    argv++;
    return subcmd->handler(parser, argc, argv);
}
