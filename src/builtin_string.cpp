// Implementation of the string builtin.
#include "config.h"

#define PCRE2_CODE_UNIT_WIDTH WCHAR_T_BITS
#ifdef _WIN32
#define PCRE2_STATIC
#endif
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <wchar.h>
#include <wctype.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parse_util.h"
#include "pcre2.h"
#include "wgetopt.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

#define STRING_ERR_MISSING _(L"%ls: Expected argument\n")

enum { BUILTIN_STRING_OK = 0, BUILTIN_STRING_NONE = 1, BUILTIN_STRING_ERROR = 2 };

static void string_error(io_streams_t &streams, const wchar_t *fmt, ...) {
    streams.err.append(L"string ");
    va_list va;
    va_start(va, fmt);
    streams.err.append_formatv(fmt, va);
    va_end(va);
}

static void string_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *subcmd,
                                  const wchar_t *opt) {
    string_error(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_help(parser, streams, L"string", streams.err);
}

// We read from stdin if we are the second or later process in a pipeline.
static bool string_args_from_stdin(const io_streams_t &streams) {
    return streams.stdin_is_directly_redirected;
}

static const wchar_t *string_get_arg_stdin(wcstring *storage, const io_streams_t &streams) {
    std::string arg;
    for (;;) {
        char ch = '\0';
        long rc = read_blocked(streams.stdin_fd, &ch, 1);

        if (rc < 0) {  // failure
            return 0;
        }

        if (rc == 0) {  // EOF
            if (arg.empty()) {
                return 0;
            } else {
                break;
            }
        }

        if (ch == '\n') {
            break;
        }

        arg += ch;
    }

    *storage = str2wcstring(arg);
    return storage->c_str();
}

static const wchar_t *string_get_arg_argv(int *argidx, wchar_t **argv) {
    return (argv && argv[*argidx]) ? argv[(*argidx)++] : 0;
}

static const wchar_t *string_get_arg(int *argidx, wchar_t **argv, wcstring *storage,
                                     const io_streams_t &streams) {
    if (string_args_from_stdin(streams)) {
        return string_get_arg_stdin(storage, streams);
    } else {
        return string_get_arg_argv(argidx, argv);
    }
}

static int string_escape(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L"n";
    const struct woption long_options[] = {{L"no-quoted", no_argument, 0, 'n'}, {0, 0, 0, 0}};

    escape_flags_t flags = ESCAPE_ALL;
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'n': {
                flags |= ESCAPE_NO_QUOTED;
                break;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nesc = 0;
    wcstring storage;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        streams.out.append(escape(arg, flags));
        streams.out.append(L'\n');
        nesc++;
    }

    return (nesc > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_join(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L"q";
    const struct woption long_options[] = {{L"quiet", no_argument, 0, 'q'}, {0, 0, 0, 0}};

    bool quiet = false;
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'q': {
                quiet = true;
                break;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == 0) {
        string_error(streams, STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nargs = 0;
    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        if (!quiet) {
            if (nargs > 0) {
                streams.out.append(sep);
            }
            streams.out.append(arg);
        }
        nargs++;
    }
    if (nargs > 0 && !quiet) {
        streams.out.push_back(L'\n');
    }

    return (nargs > 1) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_length(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L"q";
    const struct woption long_options[] = {{L"quiet", no_argument, 0, 'q'}, {0, 0, 0, 0}};

    bool quiet = false;
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'q': {
                quiet = true;
                break;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    const wchar_t *arg;
    int nnonempty = 0;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        size_t n = wcslen(arg);
        if (n > 0) {
            nnonempty++;
        }
        if (!quiet) {
            streams.out.append(to_string(n));
            streams.out.append(L'\n');
        }
    }

    return (nnonempty > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

struct match_options_t {
    bool all;
    bool ignore_case;
    bool index;
    bool invert_match;
    bool quiet;

    match_options_t()
        : all(false), ignore_case(false), index(false), invert_match(false), quiet(false) {}
};

class string_matcher_t {
   protected:
    match_options_t opts;
    io_streams_t &streams;
    int total_matched;

   public:
    string_matcher_t(const match_options_t &opts_, io_streams_t &streams_)
        : opts(opts_), streams(streams_), total_matched(0) {}

    virtual ~string_matcher_t() {}
    virtual bool report_matches(const wchar_t *arg) = 0;
    int match_count() { return total_matched; }
};

class wildcard_matcher_t : public string_matcher_t {
   private:
    wcstring wcpattern;

   public:
    wildcard_matcher_t(const wchar_t * /*argv0*/, const wchar_t *pattern,
                       const match_options_t &opts, io_streams_t &streams)
        : string_matcher_t(opts, streams), wcpattern(parse_util_unescape_wildcards(pattern)) {
        if (opts.ignore_case) {
            for (size_t i = 0; i < wcpattern.length(); i++) {
                wcpattern[i] = towlower(wcpattern[i]);
            }
        }
    }

    virtual ~wildcard_matcher_t() {}

    bool report_matches(const wchar_t *arg) {
        // Note: --all is a no-op for glob matching since the pattern is always matched against the
        // entire argument.
        bool match;

        if (opts.ignore_case) {
            wcstring s = arg;
            for (size_t i = 0; i < s.length(); i++) {
                s[i] = towlower(s[i]);
            }
            match = wildcard_match(s, wcpattern, false);
        } else {
            match = wildcard_match(arg, wcpattern, false);
        }
        if (match ^ opts.invert_match) {
            total_matched++;

            if (!opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"1 %lu\n", wcslen(arg));
                } else {
                    streams.out.append(arg);
                    streams.out.append(L'\n');
                }
            }
        }
        return true;
    }
};

static wcstring pcre2_strerror(int err_code) {
    wchar_t buf[128];
    pcre2_get_error_message(err_code, (PCRE2_UCHAR *)buf, sizeof(buf) / sizeof(wchar_t));
    return buf;
}

struct compiled_regex_t {
    pcre2_code *code;
    pcre2_match_data *match;

    compiled_regex_t(const wchar_t *argv0, const wchar_t *pattern, bool ignore_case,
                     io_streams_t &streams)
        : code(0), match(0) {
        // Disable some sequences that can lead to security problems.
        uint32_t options = PCRE2_NEVER_UTF;
#if PCRE2_CODE_UNIT_WIDTH < 32
        options |= PCRE2_NEVER_BACKSLASH_C;
#endif

        int err_code = 0;
        PCRE2_SIZE err_offset = 0;

        code =
            pcre2_compile(PCRE2_SPTR(pattern), PCRE2_ZERO_TERMINATED,
                          options | (ignore_case ? PCRE2_CASELESS : 0), &err_code, &err_offset, 0);
        if (code == 0) {
            string_error(streams, _(L"%ls: Regular expression compile error: %ls\n"), argv0,
                         pcre2_strerror(err_code).c_str());
            string_error(streams, L"%ls: %ls\n", argv0, pattern);
            string_error(streams, L"%ls: %*ls\n", argv0, err_offset, L"^");
            return;
        }

        match = pcre2_match_data_create_from_pattern(code, 0);
        if (match == 0) {
            DIE_MEM();
        }
    }

    ~compiled_regex_t() {
        if (match != 0) {
            pcre2_match_data_free(match);
        }
        if (code != 0) {
            pcre2_code_free(code);
        }
    }
};

class pcre2_matcher_t : public string_matcher_t {
    const wchar_t *argv0;
    compiled_regex_t regex;

    int report_match(const wchar_t *arg, int pcre2_rc) {
        // Return values: -1 = error, 0 = no match, 1 = match.
        if (pcre2_rc == PCRE2_ERROR_NOMATCH) {
            if (opts.invert_match && !opts.quiet) {
                streams.out.append(arg);
                streams.out.push_back(L'\n');
            }

            return opts.invert_match ? 1 : 0;
        } else if (pcre2_rc < 0) {
            string_error(streams, _(L"%ls: Regular expression match error: %ls\n"), argv0,
                         pcre2_strerror(pcre2_rc).c_str());
            return -1;
        } else if (pcre2_rc == 0) {
            // The output vector wasn't big enough. Should not happen.
            string_error(streams, _(L"%ls: Regular expression internal error\n"), argv0);
            return -1;
        }

        else if (opts.invert_match)
            return 0;

        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);

        for (int j = 0; j < pcre2_rc; j++) {
            PCRE2_SIZE begin = ovector[2 * j];
            PCRE2_SIZE end = ovector[2 * j + 1];

            if (begin != PCRE2_UNSET && end != PCRE2_UNSET && !opts.quiet) {
                if (opts.index) {
                    streams.out.append_format(L"%lu %lu", (unsigned long)(begin + 1),
                                              (unsigned long)(end - begin));
                } else if (end > begin)  // may have end < begin if \K is used
                {
                    streams.out.append(wcstring(&arg[begin], end - begin));
                }
                streams.out.push_back(L'\n');
            }
        }

        return opts.invert_match ? 0 : 1;
    }

   public:
    pcre2_matcher_t(const wchar_t *argv0_, const wchar_t *pattern, const match_options_t &opts,
                    io_streams_t &streams)
        : string_matcher_t(opts, streams),
          argv0(argv0_),
          regex(argv0_, pattern, opts.ignore_case, streams) {}

    virtual ~pcre2_matcher_t() {}

    bool report_matches(const wchar_t *arg) {
        // A return value of true means all is well (even if no matches were found), false indicates
        // an unrecoverable error.
        if (regex.code == 0) {
            // pcre2_compile() failed.
            return false;
        }

        int matched = 0;

        // See pcre2demo.c for an explanation of this logic.
        PCRE2_SIZE arglen = wcslen(arg);
        int rc = report_match(
            arg, pcre2_match(regex.code, PCRE2_SPTR(arg), arglen, 0, 0, regex.match, 0));
        if (rc < 0) {  // pcre2 match error.
            return false;
        } else if (rc == 0) {  // no match
            return true;
        }
        matched++;
        total_matched++;

        if (opts.invert_match) {
            return true;
        }

        // Report any additional matches.
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(regex.match);
        while (opts.all || matched == 0) {
            uint32_t options = 0;
            PCRE2_SIZE offset = ovector[1];  // start at end of previous match

            if (ovector[0] == ovector[1]) {
                if (ovector[0] == arglen) {
                    break;
                }
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
            }

            rc = report_match(arg, pcre2_match(regex.code, PCRE2_SPTR(arg), arglen, offset, options,
                                               regex.match, 0));
            if (rc < 0) {
                return false;
            }
            if (rc == 0) {
                if (options == 0) {  // all matches found
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

static int string_match(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L"ainvqr";
    const struct woption long_options[] = {{L"all", no_argument, 0, 'a'},
                                           {L"ignore-case", no_argument, 0, 'i'},
                                           {L"index", no_argument, 0, 'n'},
                                           {L"invert", no_argument, 0, 'v'},
                                           {L"quiet", no_argument, 0, 'q'},
                                           {L"regex", no_argument, 0, 'r'},
                                           {0, 0, 0, 0}};

    match_options_t opts;
    bool regex = false;
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'a': {
                opts.all = true;
                break;
            }
            case 'i': {
                opts.ignore_case = true;
                break;
            }
            case 'n': {
                opts.index = true;
                break;
            }
            case 'v': {
                opts.invert_match = true;
                break;
            }
            case 'q': {
                opts.quiet = true;
                break;
            }
            case 'r': {
                regex = true;
                break;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    const wchar_t *pattern;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0) {
        string_error(streams, STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    string_matcher_t *matcher;
    if (regex) {
        matcher = new pcre2_matcher_t(argv[0], pattern, opts, streams);
    } else {
        matcher = new wildcard_matcher_t(argv[0], pattern, opts, streams);
    }

    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        if (!matcher->report_matches(arg)) {
            delete matcher;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = matcher->match_count() > 0 ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
    delete matcher;
    return rc;
}

struct replace_options_t {
    bool all;
    bool ignore_case;
    bool quiet;

    replace_options_t() : all(false), ignore_case(false), quiet(false) {}
};

class string_replacer_t {
   protected:
    const wchar_t *argv0;
    replace_options_t opts;
    int total_replaced;
    io_streams_t &streams;

   public:
    string_replacer_t(const wchar_t *argv0_, const replace_options_t &opts_, io_streams_t &streams_)
        : argv0(argv0_), opts(opts_), total_replaced(0), streams(streams_) {}

    virtual ~string_replacer_t() {}
    virtual bool replace_matches(const wchar_t *arg) = 0;
    int replace_count() { return total_replaced; }
};

class literal_replacer_t : public string_replacer_t {
    const wchar_t *pattern;
    const wchar_t *replacement;
    size_t patlen;

   public:
    literal_replacer_t(const wchar_t *argv0, const wchar_t *pattern_, const wchar_t *replacement_,
                       const replace_options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams),
          pattern(pattern_),
          replacement(replacement_),
          patlen(wcslen(pattern)) {}

    virtual ~literal_replacer_t() {}

    bool replace_matches(const wchar_t *arg) {
        wcstring result;
        if (patlen == 0) {
            result = arg;
        } else {
            int replaced = 0;
            const wchar_t *cur = arg;
            while (*cur != L'\0') {
                if ((opts.all || replaced == 0) &&
                    (opts.ignore_case ? wcsncasecmp(cur, pattern, patlen)
                                      : wcsncmp(cur, pattern, patlen)) == 0) {
                    result += replacement;
                    cur += patlen;
                    replaced++;
                    total_replaced++;
                } else {
                    result += *cur;
                    cur++;
                }
            }
        }
        if (!opts.quiet) {
            streams.out.append(result);
            streams.out.append(L'\n');
        }
        return true;
    }
};

class regex_replacer_t : public string_replacer_t {
    compiled_regex_t regex;
    wcstring replacement;

    static wcstring interpret_escapes(const wchar_t *orig) {
        wcstring result;

        while (*orig != L'\0') {
            if (*orig == L'\\') {
                orig += read_unquoted_escape(orig, &result, true, false);
            } else {
                result += *orig;
                orig++;
            }
        }

        return result;
    }

   public:
    regex_replacer_t(const wchar_t *argv0, const wchar_t *pattern, const wchar_t *replacement_,
                     const replace_options_t &opts, io_streams_t &streams)
        : string_replacer_t(argv0, opts, streams),
          regex(argv0, pattern, opts.ignore_case, streams),
          replacement(interpret_escapes(replacement_)) {}

    bool replace_matches(const wchar_t *arg) {
        // A return value of true means all is well (even if no replacements were performed), false
        // indicates an unrecoverable error.
        if (regex.code == 0) {
            // pcre2_compile() failed
            return false;
        }

        uint32_t options = PCRE2_SUBSTITUTE_OVERFLOW_LENGTH | PCRE2_SUBSTITUTE_EXTENDED |
                           (opts.all ? PCRE2_SUBSTITUTE_GLOBAL : 0);
        size_t arglen = wcslen(arg);
        PCRE2_SIZE bufsize = (arglen == 0) ? 16 : 2 * arglen;
        wchar_t *output = (wchar_t *)malloc(sizeof(wchar_t) * bufsize);
        int pcre2_rc = 0;
        for (;;) {
            if (output == NULL) {
                DIE_MEM();
            }
            PCRE2_SIZE outlen = bufsize;
            pcre2_rc = pcre2_substitute(regex.code, PCRE2_SPTR(arg), arglen,
                                        0,  // start offset
                                        options, regex.match,
                                        0,  // match context
                                        PCRE2_SPTR(replacement.c_str()), PCRE2_ZERO_TERMINATED,
                                        (PCRE2_UCHAR *)output, &outlen);

            if (pcre2_rc == PCRE2_ERROR_NOMEMORY && bufsize < outlen) {
                bufsize = outlen;
                // cppcheck-suppress memleakOnRealloc
                output = (wchar_t *)realloc(output, sizeof(wchar_t) * bufsize);
                continue;
            }
            break;
        }

        bool rc = true;
        if (pcre2_rc < 0) {
            string_error(streams, _(L"%ls: Regular expression substitute error: %ls\n"), argv0,
                         pcre2_strerror(pcre2_rc).c_str());
            rc = false;
        } else {
            if (!opts.quiet) {
                streams.out.append(output);
                streams.out.append(L'\n');
            }
            total_replaced += pcre2_rc;
        }

        free(output);
        return rc;
    }
};

static int string_replace(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L"aiqr";
    const struct woption long_options[] = {{L"all", no_argument, 0, 'a'},
                                           {L"ignore-case", no_argument, 0, 'i'},
                                           {L"quiet", no_argument, 0, 'q'},
                                           {L"regex", no_argument, 0, 'r'},
                                           {0, 0, 0, 0}};

    replace_options_t opts;
    bool regex = false;
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'a': {
                opts.all = true;
                break;
            }
            case 'i': {
                opts.ignore_case = true;
                break;
            }
            case 'q': {
                opts.quiet = true;
                break;
            }
            case 'r': {
                regex = true;
                break;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    const wchar_t *pattern, *replacement;
    if ((pattern = string_get_arg_argv(&i, argv)) == 0) {
        string_error(streams, STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }
    if ((replacement = string_get_arg_argv(&i, argv)) == 0) {
        string_error(streams, STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    string_replacer_t *replacer;
    if (regex) {
        replacer = new regex_replacer_t(argv[0], pattern, replacement, opts, streams);
    } else {
        replacer = new literal_replacer_t(argv[0], pattern, replacement, opts, streams);
    }

    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        if (!replacer->replace_matches(arg)) {
            delete replacer;
            return BUILTIN_STRING_ERROR;
        }
    }

    int rc = replacer->replace_count() > 0 ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
    delete replacer;
    return rc;
}

/// Given iterators into a string (forward or reverse), splits the haystack iterators
/// about the needle sequence, up to max times. Inserts splits into the output array.
/// If the iterators are forward, this does the normal thing.
/// If the iterators are backward, this returns reversed strings, in reversed order!
/// If the needle is empty, split on individual elements (characters).
template <typename ITER>
void split_about(ITER haystack_start, ITER haystack_end, ITER needle_start, ITER needle_end,
                 wcstring_list_t *output, long max) {
    long remaining = max;
    ITER haystack_cursor = haystack_start;
    while (remaining > 0 && haystack_cursor != haystack_end) {
        ITER split_point;
        if (needle_start == needle_end) {  // empty needle, we split on individual elements
            split_point = haystack_cursor + 1;
        } else {
            split_point = std::search(haystack_cursor, haystack_end, needle_start, needle_end);
        }
        if (split_point == haystack_end) {  // not found
            break;
        }
        output->push_back(wcstring(haystack_cursor, split_point));
        remaining--;
        // Need to skip over the needle for the next search note that the needle may be empty.
        haystack_cursor = split_point + std::distance(needle_start, needle_end);
    }
    // Trailing component, possibly empty.
    output->push_back(wcstring(haystack_cursor, haystack_end));
}

static int string_split(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L":m:qr";
    const struct woption long_options[] = {{L"max", required_argument, 0, 'm'},
                                           {L"quiet", no_argument, 0, 'q'},
                                           {L"right", no_argument, 0, 'r'},
                                           {0, 0, 0, 0}};

    long max = LONG_MAX;
    bool quiet = false;
    bool right = false;
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'm': {
                errno = 0;
                wchar_t *endptr = 0;
                max = wcstol(w.woptarg, &endptr, 10);
                if (*endptr != L'\0' || errno != 0) {
                    string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                break;
            }
            case 'q': {
                quiet = true;
                break;
            }
            case 'r': {
                right = true;
                break;
            }
            case ':': {
                string_error(streams, STRING_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    const wchar_t *sep;
    if ((sep = string_get_arg_argv(&i, argv)) == NULL) {
        string_error(streams, STRING_ERR_MISSING, argv[0]);
        return BUILTIN_STRING_ERROR;
    }
    const wchar_t *sep_end = sep + wcslen(sep);

    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    wcstring_list_t splits;
    size_t arg_count = 0;
    wcstring storage;
    const wchar_t *arg;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        const wchar_t *arg_end = arg + wcslen(arg);
        if (right) {
            typedef std::reverse_iterator<const wchar_t *> reverser;
            split_about(reverser(arg_end), reverser(arg), reverser(sep_end), reverser(sep), &splits,
                        max);
        } else {
            split_about(arg, arg_end, sep, sep_end, &splits, max);
        }
        arg_count++;
    }

    // If we are from the right, split_about gave us reversed strings, in reversed order!
    if (right) {
        for (size_t j = 0; j < splits.size(); j++) {
            std::reverse(splits[j].begin(), splits[j].end());
        }
        std::reverse(splits.begin(), splits.end());
    }

    if (!quiet) {
        for (wcstring_list_t::const_iterator si = splits.begin(); si != splits.end(); ++si) {
            streams.out.append(*si);
            streams.out.append(L'\n');
        }
    }

    // We split something if we have more split values than args.
    return (splits.size() > arg_count) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_sub(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L":l:qs:";
    const struct woption long_options[] = {{L"length", required_argument, 0, 'l'},
                                           {L"quiet", no_argument, 0, 'q'},
                                           {L"start", required_argument, 0, 's'},
                                           {0, 0, 0, 0}};

    long start = 0;
    long length = -1;
    bool quiet = false;
    wgetopter_t w;
    wchar_t *endptr = NULL;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'l': {
                errno = 0;
                length = wcstol(w.woptarg, &endptr, 10);
                if (*endptr != L'\0' || (errno != 0 && errno != ERANGE)) {
                    string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                if (length < 0 || errno == ERANGE) {
                    string_error(streams, _(L"%ls: Invalid length value '%ls'\n"), argv[0],
                                 w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                break;
            }
            case 'q': {
                quiet = true;
                break;
            }
            case 's': {
                errno = 0;
                start = wcstol(w.woptarg, &endptr, 10);
                if (*endptr != L'\0' || (errno != 0 && errno != ERANGE)) {
                    string_error(streams, BUILTIN_ERR_NOT_NUMBER, argv[0], w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                if (start == 0 || start == LONG_MIN || errno == ERANGE) {
                    string_error(streams, _(L"%ls: Invalid start value '%ls'\n"), argv[0],
                                 w.woptarg);
                    return BUILTIN_STRING_ERROR;
                }
                break;
            }
            case ':': {
                string_error(streams, STRING_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    int nsub = 0;
    const wchar_t *arg;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != NULL) {
        typedef wcstring::size_type size_type;
        size_type pos = 0;
        size_type count = wcstring::npos;
        wcstring s(arg);
        if (start > 0) {
            pos = static_cast<size_type>(start - 1);
        } else if (start < 0) {
            assert(start != LONG_MIN);  // checked above
            size_type n = static_cast<size_type>(-start);
            pos = n > s.length() ? 0 : s.length() - n;
        }
        if (pos > s.length()) {
            pos = s.length();
        }

        if (length >= 0) {
            count = static_cast<size_type>(length);
        }

        // Note that std::string permits count to extend past end of string.
        if (!quiet) {
            streams.out.append(s.substr(pos, count));
            streams.out.append(L'\n');
        }
        nsub++;
    }

    return (nsub > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static int string_trim(parser_t &parser, io_streams_t &streams, int argc, wchar_t **argv) {
    const wchar_t *short_options = L":c:lqr";
    const struct woption long_options[] = {{L"chars", required_argument, 0, 'c'},
                                           {L"left", no_argument, 0, 'l'},
                                           {L"quiet", no_argument, 0, 'q'},
                                           {L"right", no_argument, 0, 'r'},
                                           {0, 0, 0, 0}};

    bool do_left = 0, do_right = 0;
    bool quiet = false;
    wcstring chars_to_trim = L" \f\n\r\t";
    wgetopter_t w;
    for (;;) {
        int c = w.wgetopt_long(argc, argv, short_options, long_options, 0);

        if (c == -1) {
            break;
        }
        switch (c) {
            case 0: {
                break;
            }
            case 'c': {
                chars_to_trim = w.woptarg;
                break;
            }
            case 'l': {
                do_left = true;
                break;
            }
            case 'q': {
                quiet = true;
                break;
            }
            case 'r': {
                do_right = true;
                break;
            }
            case ':': {
                string_error(streams, STRING_ERR_MISSING, argv[0]);
                return BUILTIN_STRING_ERROR;
            }
            case '?': {
                string_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
                return BUILTIN_STRING_ERROR;
            }
        }
    }

    int i = w.woptind;
    if (string_args_from_stdin(streams) && argc > i) {
        string_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, argv[0]);
        return BUILTIN_STRING_ERROR;
    }

    // If neither left or right is specified, we do both.
    if (!do_left && !do_right) {
        do_left = true;
        do_right = true;
    }

    const wchar_t *arg;
    size_t ntrim = 0;

    wcstring argstr;
    wcstring storage;
    while ((arg = string_get_arg(&i, argv, &storage, streams)) != 0) {
        argstr = arg;
        // Begin and end are respectively the first character to keep on the left, and first
        // character to trim on the right. The length is thus end - start.
        size_t begin = 0, end = argstr.size();
        if (do_right) {
            size_t last_to_keep = argstr.find_last_not_of(chars_to_trim);
            end = (last_to_keep == wcstring::npos) ? 0 : last_to_keep + 1;
        }
        if (do_left) {
            size_t first_to_keep = argstr.find_first_not_of(chars_to_trim);
            begin = (first_to_keep == wcstring::npos ? end : first_to_keep);
        }
        assert(begin <= end && end <= argstr.size());
        ntrim += argstr.size() - (end - begin);
        if (!quiet) {
            streams.out.append(wcstring(argstr, begin, end - begin));
            streams.out.append(L'\n');
        }
    }

    return (ntrim > 0) ? BUILTIN_STRING_OK : BUILTIN_STRING_NONE;
}

static const struct string_subcommand {
    const wchar_t *name;
    int (*handler)(parser_t &, io_streams_t &, int argc, wchar_t **argv);
}

string_subcommands[] = {
    {L"escape", &string_escape}, {L"join", &string_join},       {L"length", &string_length},
    {L"match", &string_match},   {L"replace", &string_replace}, {L"split", &string_split},
    {L"sub", &string_sub},       {L"trim", &string_trim},       {0, 0}};

/// The string builtin, for manipulating strings.
int builtin_string(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);
    if (argc <= 1) {
        streams.err.append_format(_(L"string: Expected subcommand\n"));
        builtin_print_help(parser, streams, L"string", streams.err);
        return BUILTIN_STRING_ERROR;
    }

    if (wcscmp(argv[1], L"-h") == 0 || wcscmp(argv[1], L"--help") == 0) {
        builtin_print_help(parser, streams, L"string", streams.err);
        return BUILTIN_STRING_OK;
    }

    const string_subcommand *subcmd = &string_subcommands[0];
    while (subcmd->name != 0 && wcscmp(subcmd->name, argv[1]) != 0) {
        subcmd++;
    }
    if (subcmd->handler == 0) {
        streams.err.append_format(_(L"string: Unknown subcommand '%ls'\n"), argv[1]);
        builtin_print_help(parser, streams, L"string", streams.err);
        return BUILTIN_STRING_ERROR;
    }

    argc--;
    argv++;
    return subcmd->handler(parser, streams, argc, argv);
}
