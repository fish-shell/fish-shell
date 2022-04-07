// Implementation of the path builtin.
#include "config.h"  // IWYU pragma: keep

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glob.h>
#include <string>
#include <utility>
#include <vector>

#include "../builtin.h"
#include "../common.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../io.h"
#include "../util.h"
#include "../wcstringutil.h"
#include "../wgetopt.h"
#include "../wutil.h"  // IWYU pragma: keep

// How many bytes we read() at once.
// We use PATH_MAX here so we always get at least one path,
// and so we can automatically detect NULL-separated input.
#define PATH_CHUNK_SIZE PATH_MAX

static void path_error(io_streams_t &streams, const wchar_t *fmt, ...) {
    streams.err.append(L"path ");
    va_list va;
    va_start(va, fmt);
    streams.err.append_formatv(fmt, va);
    va_end(va);
}

static void path_unknown_option(parser_t &parser, io_streams_t &streams, const wchar_t *subcmd,
                                  const wchar_t *opt) {
    path_error(streams, BUILTIN_ERR_UNKNOWN, subcmd, opt);
    builtin_print_error_trailer(parser, streams.err, L"path");
}

// We read from stdin if we are the second or later process in a pipeline.
static bool path_args_from_stdin(const io_streams_t &streams) {
    return streams.stdin_is_directly_redirected;
}

static const wchar_t *path_get_arg_argv(int *argidx, const wchar_t *const *argv) {
    return argv && argv[*argidx] ? argv[(*argidx)++] : nullptr;
}

// A helper type for extracting arguments from either argv or stdin.
namespace {
class arg_iterator_t {
    // The list of arguments passed to this builtin.
    const wchar_t *const *argv_;
    // If using argv, index of the next argument to return.
    int argidx_;
    // If not using argv, a string to store bytes that have been read but not yet returned.
    std::string buffer_;
    // Whether we have found a char to split on yet, when reading from stdin.
    // If explicitly passed, we will always split on NULL,
    // if not we will split on NULL if the first PATH_MAX chunk includes one,
    // or '\n' otherwise.
    bool have_split_;
    // The char we have decided to split on when reading from stdin.
    char split_{'\0'};
    // Backing storage for the next() string.
    wcstring storage_;
    const io_streams_t &streams_;

    /// Reads the next argument from stdin, returning true if an argument was produced and false if
    /// not. On true, the string is stored in storage_.
    bool get_arg_stdin() {
        assert(path_args_from_stdin(streams_) && "should not be reading from stdin");
        assert(streams_.stdin_fd >= 0 && "should have a valid fd");
        // Read in chunks from fd until buffer has a line (or the end if split_ is unset).
        size_t pos;
        while (!have_split_ || (pos = buffer_.find(split_)) == std::string::npos) {
            char buf[PATH_CHUNK_SIZE];
            long n = read_blocked(streams_.stdin_fd, buf, PATH_CHUNK_SIZE);
            if (n == 0) {
                // If we still have buffer contents, flush them,
                // in case there was no trailing sep.
                if (buffer_.empty()) return false;
                storage_ = str2wcstring(buffer_);
                buffer_.clear();
                return true;
            }
            if (n == -1) {
                // Some error happened. We can't do anything about it,
                // so ignore it.
                // (read_blocked already retries for EAGAIN and EINTR)
                storage_ = str2wcstring(buffer_);
                buffer_.clear();
                return false;
            }
            buffer_.append(buf, n);
            if (!have_split_) {
                if (buffer_.find('\0') != std::string::npos) {
                    split_ = '\0';
                } else {
                    split_ = '\n';
                }
                have_split_ = true;
            }
        }

        // Split the buffer on the sep and return the first part.
        storage_ = str2wcstring(buffer_, pos);
        buffer_.erase(0, pos + 1);
        return true;
    }

   public:
    arg_iterator_t(const wchar_t *const *argv, int argidx, const io_streams_t &streams, bool split_null)
        : argv_(argv), argidx_(argidx), have_split_(split_null), streams_(streams) {}

    const wcstring *nextstr() {
        if (path_args_from_stdin(streams_)) {
            return get_arg_stdin() ? &storage_ : nullptr;
        }
        if (auto arg = path_get_arg_argv(&argidx_, argv_)) {
            storage_ = arg;
            return &storage_;
        } else {
            return nullptr;
        }
    }
};
}  // namespace

enum {
    TYPE_BLOCK = 1 << 0,  /// A block device
    TYPE_DIR = 1 << 1,    /// A directory
    TYPE_FILE = 1 << 2,     /// A regular file
    TYPE_LINK = 1 << 3,  /// A link
    TYPE_CHAR = 1 << 4,  /// A character device
    TYPE_FIFO = 1 << 5,  /// A fifo
    TYPE_SOCK = 1 << 6,  /// A socket
};
typedef uint32_t path_type_flags_t;

enum {
    PERM_READ = 1 << 0,
    PERM_WRITE = 1 << 1,
    PERM_EXEC = 1 << 2,
    PERM_SUID = 1 << 3,
    PERM_SGID = 1 << 4,
    PERM_USER = 1 << 5,
    PERM_GROUP = 1 << 6,
};
typedef uint32_t path_perm_flags_t;

// This is used by the subcommands to communicate with the option parser which flags are
// valid and get the result of parsing the command for flags.
struct options_t {  //!OCLINT(too many fields)
    bool perm_valid = false;
    bool type_valid = false;
    bool invert_valid = false;
    bool what_valid = false;
    bool have_what = false;
    const wchar_t *what = nullptr;

    bool null_in = false;
    bool null_out = false;
    bool quiet = false;

    bool have_type = false;
    path_type_flags_t type = 0;

    bool have_perm = false;
    // Whether we need to check a special permission like suid.
    bool have_special_perm = false;
    path_perm_flags_t perm = 0;

    bool invert = false;

    const wchar_t *arg1 = nullptr;
};

static void path_out(io_streams_t &streams, const options_t &opts, const wcstring &str) {
    if (!opts.quiet) {
        if (!opts.null_out) {
            streams.out.append_with_separation(str,
                                               separation_type_t::explicitly);
        } else {
            streams.out.append(str);
            streams.out.push_back(L'\0');
        }
    }
}

static int handle_flag_q(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    UNUSED(argv);
    UNUSED(parser);
    UNUSED(streams);
    UNUSED(w);
    opts->quiet = true;
    return STATUS_CMD_OK;
}

static int handle_flag_z(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    UNUSED(argv);
    UNUSED(parser);
    UNUSED(streams);
    UNUSED(w);
    opts->null_in = true;
    return STATUS_CMD_OK;
}

static int handle_flag_Z(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    UNUSED(argv);
    UNUSED(parser);
    UNUSED(streams);
    UNUSED(w);
    opts->null_out = true;
    return STATUS_CMD_OK;
}

static int handle_flag_t(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->type_valid) {
        if (!opts->have_type) opts->type = 0;
        opts->have_type = true;
        wcstring_list_t types = split_string_tok(w.woptarg, L",");
        for (const auto &t : types) {
            if (t == L"file") {
                opts->type |= TYPE_FILE;
            } else if (t == L"dir") {
                opts->type |= TYPE_DIR;
            } else if (t == L"block") {
                opts->type |= TYPE_BLOCK;
            } else if (t == L"char") {
                opts->type |= TYPE_CHAR;
            } else if (t == L"fifo") {
                opts->type |= TYPE_FIFO;
            } else if (t == L"socket") {
                opts->type |= TYPE_SOCK;
            } else if (t == L"link") {
                opts->type |= TYPE_LINK;
            } else {
                path_error(streams, _(L"%ls: Invalid type '%ls'"), L"path", t.c_str());
                return STATUS_INVALID_ARGS;
            }
        }
        return STATUS_CMD_OK;
    }
    path_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}


static int handle_flag_p(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->perm_valid) {
        if (!opts->have_perm) opts->perm = 0;
        opts->have_perm = true;
        wcstring_list_t perms = split_string_tok(w.woptarg, L",");
        for (const auto &p : perms) {
            if (p == L"read") {
                opts->perm |= PERM_READ;
            } else if (p == L"write") {
                opts->perm |= PERM_WRITE;
            } else if (p == L"exec") {
                opts->perm |= PERM_EXEC;
            } else if (p == L"suid") {
                opts->perm |= PERM_SUID;
                opts->have_special_perm = true;
            } else if (p == L"sgid") {
                opts->perm |= PERM_SGID;
                opts->have_special_perm = true;
            } else if (p == L"user") {
                opts->perm |= PERM_USER;
                opts->have_special_perm = true;
            } else if (p == L"group") {
                opts->perm |= PERM_GROUP;
                opts->have_special_perm = true;
            } else {
                path_error(streams, _(L"%ls: Invalid permission '%ls'"), L"path", p.c_str());
                return STATUS_INVALID_ARGS;
            }
        }
        return STATUS_CMD_OK;
    }
    path_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_perms(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                             const wgetopter_t &w, options_t *opts, path_perm_flags_t perm ) {
    if (opts->perm_valid) {
        if (!opts->have_perm) opts->perm = 0;
        opts->have_perm = true;
        opts->perm |= perm;
        return STATUS_CMD_OK;
    }
    path_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_r(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    return handle_flag_perms(argv, parser, streams, w, opts, PERM_READ);
}
static int handle_flag_w(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    return handle_flag_perms(argv, parser, streams, w, opts, PERM_WRITE);
}
static int handle_flag_x(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    return handle_flag_perms(argv, parser, streams, w, opts, PERM_EXEC);
}

static int handle_flag_types(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                             const wgetopter_t &w, options_t *opts, path_type_flags_t type) {
    if (opts->type_valid) {
        if (!opts->have_type) opts->type = 0;
        opts->have_type = true;
        opts->type |= type;
        return STATUS_CMD_OK;
    }
    path_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_f(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    return handle_flag_types(argv, parser, streams, w, opts, TYPE_FILE);
}
static int handle_flag_l(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    return handle_flag_types(argv, parser, streams, w, opts, TYPE_LINK);
}
static int handle_flag_d(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    return handle_flag_types(argv, parser, streams, w, opts, TYPE_DIR);
}

static int handle_flag_v(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    if (opts->invert_valid) {
        opts->invert = true;
        return STATUS_CMD_OK;
    }
    path_unknown_option(parser, streams, argv[0], argv[w.woptind - 1]);
    return STATUS_INVALID_ARGS;
}

static int handle_flag_what(const wchar_t **argv, parser_t &parser, io_streams_t &streams,
                         const wgetopter_t &w, options_t *opts) {
    UNUSED(argv);
    UNUSED(parser);
    UNUSED(streams);
    opts->have_what = true;
    opts->what = w.woptarg;
    return STATUS_CMD_OK;
}

/// This constructs the wgetopt() short options string based on which arguments are valid for the
/// subcommand. We have to do this because many short flags have multiple meanings and may or may
/// not require an argument depending on the meaning.
static wcstring construct_short_opts(options_t *opts) {  //!OCLINT(high npath complexity)
    // All commands accept -z, -Z and -q
    wcstring short_opts(L":zZq");
    if (opts->perm_valid) {
        short_opts.append(L"p:");
        short_opts.append(L"rwx");
    }
    if (opts->type_valid) {
        short_opts.append(L"t:");
        short_opts.append(L"fld");
    }
    if (opts->invert_valid) short_opts.append(L"v");
    return short_opts;
}

// Note that several long flags share the same short flag. That is okay. The caller is expected
// to indicate that a max of one of the long flags sharing a short flag is valid.
// Remember: adjust the completions in share/completions/ when options change
static const struct woption long_options[] = {
                                              {L"quiet", no_argument, nullptr, 'q'},
                                              {L"null-in", no_argument, nullptr, 'z'},
                                              {L"null-out", no_argument, nullptr, 'Z'},
                                              {L"perm", required_argument, nullptr, 'p'},
                                              {L"type", required_argument, nullptr, 't'},
                                              {L"invert", required_argument, nullptr, 't'},
                                              {L"what", required_argument, nullptr, 1},
                                              {}};

static const std::unordered_map<char, decltype(*handle_flag_q)> flag_to_function = {
    {'q', handle_flag_q}, {'v', handle_flag_v},
    {'z', handle_flag_z}, {'Z', handle_flag_Z},
    {'t', handle_flag_t}, {'p', handle_flag_p},
    {'r', handle_flag_r}, {'w', handle_flag_w},
    {'x', handle_flag_x}, {'f', handle_flag_f},
    {'l', handle_flag_l}, {'d', handle_flag_d},
    {1, handle_flag_what},
};

/// Parse the arguments for flags recognized by a specific string subcommand.
static int parse_opts(options_t *opts, int *optind, int n_req_args, int argc, const wchar_t **argv,
                      parser_t &parser, io_streams_t &streams) {
    const wchar_t *cmd = argv[0];
    wcstring short_opts = construct_short_opts(opts);
    const wchar_t *short_options = short_opts.c_str();
    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        auto fn = flag_to_function.find(opt);
        if (fn != flag_to_function.end()) {
            int retval = fn->second(argv, parser, streams, w, opts);
            if (retval != STATUS_CMD_OK) return retval;
        } else if (opt == ':') {
            streams.err.append(L"path ");
            builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1],
                                     false /* print_hints */);
            return STATUS_INVALID_ARGS;
        } else if (opt == '?') {
            path_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
            return STATUS_INVALID_ARGS;
        } else {
            DIE("unexpected retval from wgetopt_long");
        }
    }

    *optind = w.woptind;

    if (n_req_args) {
        assert(n_req_args == 1);
        opts->arg1 = path_get_arg_argv(optind, argv);
        if (!opts->arg1 && n_req_args == 1) {
            path_error(streams, BUILTIN_ERR_ARG_COUNT0, cmd);
            return STATUS_INVALID_ARGS;
        }
    }

    // At this point we should not have optional args and be reading args from stdin.
    if (path_args_from_stdin(streams) && argc > *optind) {
        path_error(streams, BUILTIN_ERR_TOO_MANY_ARGUMENTS, cmd);
        return STATUS_INVALID_ARGS;
    }

    return STATUS_CMD_OK;
}

static int path_transform(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv,
                            wcstring (*func)(wcstring)) {
    options_t opts;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    arg_iterator_t aiter(argv, optind, streams, opts.null_in);
    while (const wcstring *arg = aiter.nextstr()) {
        // Empty paths make no sense, but e.g. wbasename returns true for them.
        if (arg->empty()) continue;
        wcstring transformed = func(*arg);
        if (transformed != *arg) {
            n_transformed++;
            // Return okay if path wasn't already in this form
            // TODO: Is that correct?
            if (opts.quiet) return STATUS_CMD_OK;
        }
        path_out(streams, opts, transformed);
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}


static int path_basename(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return path_transform(parser, streams, argc, argv, wbasename);
}

static int path_dirname(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return path_transform(parser, streams, argc, argv, wdirname);
}

// Not a constref because this must have the same type as wdirname.
// cppcheck-suppress passedByValue
static wcstring normalize_helper(wcstring path) {
    wcstring np = normalize_path(path, false);
    if (!np.empty() && np[0] == L'-') {
        np = L"./" + np;
    }
    return np;
}

static bool filter_path(options_t opts, const wcstring &path) {
    // TODO: Add moar stuff:
    // fifos, sockets, size greater than zero, setuid, ...
    // Nothing to check, file existence is checked elsewhere.
    if (!opts.have_type && !opts.have_perm) return true;

    if (opts.have_type) {
        bool type_ok = false;
        struct stat buf;
        if (opts.type & TYPE_LINK) {
            type_ok = !lwstat(path, &buf) && S_ISLNK(buf.st_mode);
        }

        auto ret = !wstat(path, &buf);
        if (!ret) {
            // Does not exist
            return false;
        }
        if (!type_ok && opts.type & TYPE_FILE && S_ISREG(buf.st_mode)) {
            type_ok = true;
        }
        if (!type_ok && opts.type & TYPE_DIR && S_ISDIR(buf.st_mode)) {
            type_ok = true;
        }
        if (!type_ok && opts.type & TYPE_BLOCK && S_ISBLK(buf.st_mode)) {
            type_ok = true;
        }
        if (!type_ok && opts.type & TYPE_CHAR && S_ISCHR(buf.st_mode)) {
            type_ok = true;
        }
        if (!type_ok && opts.type & TYPE_FIFO && S_ISFIFO(buf.st_mode)) {
            type_ok = true;
        }
        if (!type_ok && opts.type & TYPE_SOCK && S_ISSOCK(buf.st_mode)) {
            type_ok = true;
        }
        if (!type_ok) return false;
    }
    if (opts.have_perm) {
        int amode = 0;
        if (opts.perm & PERM_READ) amode |= R_OK;
        if (opts.perm & PERM_WRITE) amode |= W_OK;
        if (opts.perm & PERM_EXEC) amode |= X_OK;
        // access returns 0 on success,
        // -1 on failure. Yes, C can't even keep its bools straight.
        if (waccess(path, amode)) return false;

        // Permissions that require special handling
        if (opts.have_special_perm) {
            struct stat buf;
            auto ret = !wstat(path, &buf);
            if (!ret) {
                // Does not exist, WTF?
                return false;
            }

            if (opts.perm & PERM_SUID && !(S_ISUID & buf.st_mode)) return false;
            if (opts.perm & PERM_SGID && !(S_ISGID & buf.st_mode)) return false;
            if (opts.perm & PERM_USER && !(geteuid() == buf.st_uid)) return false;
            if (opts.perm & PERM_GROUP && !(getegid() == buf.st_gid)) return false;
        }
    }

    // No filters failed.
    return true;
}

static int path_normalize(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return path_transform(parser, streams, argc, argv, normalize_helper);
}

static maybe_t<size_t> find_extension (const wcstring &path) {
        // The extension belongs to the basename,
        // if there is a "." before the last component it doesn't matter.
        // e.g. ~/.config/fish/conf.d/foo
        // does not have an extension! The ".d" here is not a file extension for "foo".
        // And "~/.config" doesn't have an extension either - the ".config" is the filename.
        wcstring filename = wbasename(path);

        // "." and ".." aren't really *files* and therefore don't have an extension.
        if (filename == L"." || filename == L"..") return none();

        // If we don't have a "." or the "." is the first in the filename,
        // we do not have an extension
        size_t pos = filename.find_last_of(L'.');
        if (pos == wcstring::npos || pos == 0) {
            return none();
        }

        // Convert pos back to what it would be in the original path.
        return pos + path.size() - filename.size();
}

static int path_extension(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    arg_iterator_t aiter(argv, optind, streams, opts.null_in);
    while (const wcstring *arg = aiter.nextstr()) {
        auto pos = find_extension(*arg);

        if (!pos) continue;

        wcstring ext = arg->substr(*pos);
        if (opts.quiet && !ext.empty()) {
            return STATUS_CMD_OK;
        }
        path_out(streams, opts, ext);
        n_transformed++;
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int path_change_extension(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    int optind;
    int retval = parse_opts(&opts, &optind, 1, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    arg_iterator_t aiter(argv, optind, streams, opts.null_in);
    while (const wcstring *arg = aiter.nextstr()) {
        auto pos = find_extension(*arg);

        wcstring ext;
        if (!pos) {
            ext = *arg;
        } else {
            ext = arg->substr(0, *pos);
        }

        // Only add on the extension "." if we have something.
        // That way specifying an empty extension strips it.
        if (*opts.arg1) {
            if (opts.arg1[0] != L'.') {
                ext.push_back(L'.');
            }
            ext.append(opts.arg1);
        }
        path_out(streams, opts, ext);
        n_transformed++;
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int path_resolve(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    int n_transformed = 0;
    arg_iterator_t aiter(argv, optind, streams, opts.null_in);
    while (const wcstring *arg = aiter.nextstr()) {
        auto real = wrealpath(*arg);

        if (!real) {
            // The path doesn't exist, so we go up until we find
            // something that does.
            wcstring next = *arg;
            // First add $PWD if we're relative
            if (!next.empty() && next[0] != L'/') {
                next = wgetcwd() + L"/" + next;
            }
            auto rest = wbasename(next);
            while(!next.empty() && next != L"/") {
                next = wdirname(next);
                real = wrealpath(next);
                if (real) {
                    real->push_back(L'/');
                    real->append(rest);
                    real = normalize_path(*real, false);
                    break;
                }
                rest = wbasename(next) + L'/' + rest;
            }
            if (!real) {
                continue;
            }
        }

        // Normalize the path so "../" components are eliminated even after
        // nonexistent or non-directory components.
        // Otherwise `path resolve foo/../` will be `$PWD/foo/../` if foo is a file.
        real = normalize_path(*real, false);

        // Return 0 if we found a realpath.
        if (opts.quiet) {
            return STATUS_CMD_OK;
        }
        path_out(streams, opts, *real);
        n_transformed++;
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int path_sort(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    options_t opts;
    opts.invert_valid = true;
    opts.what_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    auto func = +[] (const wcstring &x) {
        return wbasename(x.c_str());
    };
    if (opts.have_what) {
        if (std::wcscmp(opts.what, L"basename") == 0) {
            // Do nothing, this is the default
        } else if (std::wcscmp(opts.what, L"dirname") == 0) {
            func = +[] (const wcstring &x) {
                return wdirname(x.c_str());
            };
        } else if (std::wcscmp(opts.what, L"path") == 0) {
            // Act as if --what hadn't been given.
            opts.have_what = false;
        } else {
            path_error(streams, _(L"%ls: Invalid sort key '%ls'\n"), argv[0], opts.what);
            return STATUS_INVALID_ARGS;
        }
    }

    wcstring_list_t list;
    arg_iterator_t aiter(argv, optind, streams, opts.null_in);
    while (const wcstring *arg = aiter.nextstr()) {
        list.push_back(*arg);
    }

    if (opts.have_what) {
        // Keep a map to avoid repeated func calls and to keep things alive.
        std::map<wcstring, wcstring> funced;
        for (const auto &arg : list) {
            funced[arg] = func(arg);
        }

        std::sort(list.begin(), list.end(),
                  [&](const wcstring &a, const wcstring &b) {
                      return (wcsfilecmp_glob(funced[a].c_str(), funced[b].c_str()) < 0) != opts.invert;
                  });
    } else {
        // Without --what, we just sort by the entire path,
        // so we have no need to transform and such.
        std::sort(list.begin(), list.end(),
                  [&](const wcstring &a, const wcstring &b) {
                      return (wcsfilecmp_glob(a.c_str(), b.c_str()) < 0) != opts.invert;
                  });
    }

    for (const auto &entry : list) {
        path_out(streams, opts, entry);
    }

    /* TODO: Return true only if already sorted? */
    return STATUS_CMD_OK;
}

// All strings are taken to be filenames, and if they match the type/perms/etc (and exist!)
// they are passed along.
static int path_filter(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv, bool is_is) {
    options_t opts;
    opts.type_valid = true;
    opts.perm_valid = true;
    opts.invert_valid = true;
    int optind;
    int retval = parse_opts(&opts, &optind, 0, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;
    // If we have been invoked as "path is", which is "path filter -q".
    if (is_is) opts.quiet = true;

    int n_transformed = 0;
    arg_iterator_t aiter(argv, optind, streams, opts.null_in);
    while (const wcstring *arg = aiter.nextstr()) {
        if ((!opts.invert || (!opts.have_perm && !opts.have_type)) && filter_path(opts, *arg)) {
            // If we don't have filters, check if it exists.
            // (for match this is done by the glob already)
            if (!opts.have_type && !opts.have_perm) {
                bool ok = !waccess(*arg, F_OK);
                if (ok == opts.invert) continue;
            }

            path_out(streams, opts, *arg);
            n_transformed++;
            if (opts.quiet) return STATUS_CMD_OK;
        }
    }

    return n_transformed > 0 ? STATUS_CMD_OK : STATUS_CMD_ERROR;
}

static int path_filter(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return path_filter(parser, streams, argc, argv, false /* is_is */);
}

static int path_is(parser_t &parser, io_streams_t &streams, int argc, const wchar_t **argv) {
    return path_filter(parser, streams, argc, argv, true /* is_is */);
}

// Keep sorted alphabetically
static constexpr const struct path_subcommand {
    const wchar_t *name;
    int (*handler)(parser_t &, io_streams_t &, int argc,  //!OCLINT(unused param)
                   const wchar_t **argv);                 //!OCLINT(unused param)
} path_subcommands[] = {
    // TODO: Which operations do we want?
    {L"basename", &path_basename},
    {L"change-extension", &path_change_extension},
    {L"dirname", &path_dirname},
    {L"extension", &path_extension},
    {L"filter", &path_filter},
    {L"is", &path_is},
    {L"normalize", &path_normalize},
    {L"resolve", &path_resolve},
    {L"sort", &path_sort},
};
ASSERT_SORTED_BY_NAME(path_subcommands);

/// The path builtin, for handling paths.
maybe_t<int> builtin_path(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    if (argc <= 1) {
        streams.err.append_format(BUILTIN_ERR_MISSING_SUBCMD, cmd);
        builtin_print_error_trailer(parser, streams.err, L"path");
        return STATUS_INVALID_ARGS;
    }

    if (std::wcscmp(argv[1], L"-h") == 0 || std::wcscmp(argv[1], L"--help") == 0) {
        builtin_print_help(parser, streams, L"path");
        return STATUS_CMD_OK;
    }

    const wchar_t *subcmd_name = argv[1];
    const auto *subcmd = get_by_sorted_name(subcmd_name, path_subcommands);
    if (!subcmd) {
        streams.err.append_format(BUILTIN_ERR_INVALID_SUBCMD, cmd, subcmd_name);
        builtin_print_error_trailer(parser, streams.err, L"path");
        return STATUS_INVALID_ARGS;
    }

    if (argc >= 3 && (std::wcscmp(argv[2], L"-h") == 0 || std::wcscmp(argv[2], L"--help") == 0)) {
        wcstring path_dash_subcommand = wcstring(argv[0]) + L"-" + subcmd_name;
        builtin_print_help(parser, streams, path_dash_subcommand.c_str());
        return STATUS_CMD_OK;
    }
    argc--;
    argv++;
    return subcmd->handler(parser, streams, argc, argv);
}
