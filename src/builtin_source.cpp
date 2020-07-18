// Implementation of the source builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_source.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cwchar>

#include "builtin.h"
#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "intern.h"
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "reader.h"
#include "wutil.h"  // IWYU pragma: keep

/// The  source builtin, sometimes called `.`. Evaluates the contents of a file in the current
/// context.
maybe_t<int> builtin_source(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    ASSERT_IS_MAIN_THREAD();
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    // If we open a file, this ensures we close it.
    autoclose_fd_t opened_fd;

    // The fd that we read from, either from opened_fd or stdin.
    int fd = -1;

    struct stat buf;
    const wchar_t *fn, *fn_intern;

    if (argc == optind || std::wcscmp(argv[optind], L"-") == 0) {
        // Either a bare `source` which means to implicitly read from stdin or an explicit `-`.
        if (argc == optind && isatty(streams.stdin_fd)) {
            // Don't implicitly read from the terminal.
            return STATUS_CMD_ERROR;
        }
        fn = L"-";
        fn_intern = fn;
        fd = streams.stdin_fd;
    } else {
        opened_fd = autoclose_fd_t(wopen_cloexec(argv[optind], O_RDONLY));
        if (!opened_fd.valid()) {
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      cmd, argv[optind]);
            builtin_wperror(cmd, streams);
            return STATUS_CMD_ERROR;
        }

        fd = opened_fd.fd();
        if (fstat(fd, &buf) == -1) {
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      cmd, argv[optind]);
            builtin_wperror(L"source", streams);
            return STATUS_CMD_ERROR;
        }

        if (!S_ISREG(buf.st_mode)) {
            streams.err.append_format(_(L"%ls: '%ls' is not a file\n"), cmd, argv[optind]);
            return STATUS_CMD_ERROR;
        }

        fn_intern = intern(argv[optind]);
    }
    assert(fd >= 0 && "Should have a valid fd");

    const block_t *sb = parser.push_block(block_t::source_block(fn_intern));
    auto &ld = parser.libdata();
    scoped_push<const wchar_t *> filename_push{&ld.current_filename, fn_intern};

    // This is slightly subtle. If this is a bare `source` with no args then `argv + optind` already
    // points to the end of argv. Otherwise we want to skip the file name to get to the args if any.
    wcstring_list_t argv_list =
        null_terminated_array_t<wchar_t>::to_list(argv + optind + (argc == optind ? 0 : 1));
    parser.vars().set_argv(std::move(argv_list));

    retval = reader_read(parser, fd, streams.io_chain ? *streams.io_chain : io_chain_t());

    parser.pop_block(sb);

    if (retval != STATUS_CMD_OK) {
        streams.err.append_format(_(L"%ls: Error while reading file '%ls'\n"), cmd,
                                  fn_intern == intern_static(L"-") ? L"<stdin>" : fn_intern);
    } else {
        retval = parser.get_last_status();
    }

    // Do not close fd after calling reader_read. reader_read automatically closes it before calling
    // eval.
    return retval;
}
