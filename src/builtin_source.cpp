// Implementation of the source builtin.
#include "config.h"  // IWYU pragma: keep

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include "builtin.h"
#include "builtin_source.h"
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
int builtin_source(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    ASSERT_IS_MAIN_THREAD();
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    help_only_cmd_opts_t opts;

    int optind;
    int retval = parse_help_only_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd, streams.out);
        return STATUS_CMD_OK;
    }

    int fd;
    struct stat buf;
    const wchar_t *fn, *fn_intern;

    if (argc == optind || wcscmp(argv[optind], L"-") == 0) {
        // Either a bare `source` which means to implicitly read from stdin or an explicit `-`.
        if (argc == optind && !streams.stdin_is_directly_redirected) {
            // Don't implicitly read from the terminal.
            return STATUS_CMD_ERROR;
        }
        fn = L"-";
        fn_intern = fn;
        fd = dup(streams.stdin_fd);
    } else {
        if ((fd = wopen_cloexec(argv[optind], O_RDONLY)) == -1) {
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      cmd, argv[optind]);
            builtin_wperror(cmd, streams);
            return STATUS_CMD_ERROR;
        }

        if (fstat(fd, &buf) == -1) {
            close(fd);
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      cmd, argv[optind]);
            builtin_wperror(L"source", streams);
            return STATUS_CMD_ERROR;
        }

        if (!S_ISREG(buf.st_mode)) {
            close(fd);
            streams.err.append_format(_(L"%ls: '%ls' is not a file\n"), cmd, argv[optind]);
            return STATUS_CMD_ERROR;
        }

        fn_intern = intern(argv[optind]);
    }

    const source_block_t *sb = parser.push_block<source_block_t>(fn_intern);
    reader_push_current_filename(fn_intern);

    // This is slightly subtle. If this is a bare `source` with no args then `argv + optind` already
    // points to the end of argv. Otherwise we want to skip the file name to get to the args if any.
    env_set_argv(argv + optind + (argc == optind ? 0 : 1));

    retval = reader_read(fd, streams.io_chain ? *streams.io_chain : io_chain_t());

    parser.pop_block(sb);

    if (retval != STATUS_CMD_OK) {
        streams.err.append_format(_(L"%ls: Error while reading file '%ls'\n"), cmd,
                                  fn_intern == intern_static(L"-") ? L"<stdin>" : fn_intern);
    } else {
        retval = proc_get_last_status();
    }

    // Do not close fd after calling reader_read. reader_read automatically closes it before calling
    // eval.
    reader_pop_current_filename();
    return retval;
}
