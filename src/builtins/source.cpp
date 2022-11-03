// SPDX-FileCopyrightText: © 2017 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Implementation of the source builtin.
#include "config.h"  // IWYU pragma: keep

#include "source.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cwchar>
#include <memory>
#include <string>
#include <utility>

#include "../builtin.h"
#include "../common.h"
#include "../env.h"
#include "../fallback.h"  // IWYU pragma: keep
#include "../fds.h"
#include "../io.h"
#include "../maybe.h"
#include "../null_terminated_array.h"
#include "../parser.h"
#include "../reader.h"
#include "../wutil.h"  // IWYU pragma: keep

/// The  source builtin, sometimes called `.`. Evaluates the contents of a file in the current
/// context.
maybe_t<int> builtin_source(parser_t &parser, io_streams_t &streams, const wchar_t **argv) {
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
    filename_ref_t func_filename{};

    if (argc == optind || std::wcscmp(argv[optind], L"-") == 0) {
        if (streams.stdin_fd < 0) {
            streams.err.append_format(_(L"%ls: stdin is closed\n"), cmd);
            return STATUS_CMD_ERROR;
        }
        // Either a bare `source` which means to implicitly read from stdin or an explicit `-`.
        if (argc == optind && isatty(streams.stdin_fd)) {
            // Don't implicitly read from the terminal.
            return STATUS_CMD_ERROR;
        }
        func_filename = std::make_shared<wcstring>(L"-");
        fd = streams.stdin_fd;
    } else {
        opened_fd = autoclose_fd_t(wopen_cloexec(argv[optind], O_RDONLY));
        if (!opened_fd.valid()) {
            wcstring esc = escape_string(argv[optind]);
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      cmd, esc.c_str());
            builtin_wperror(cmd, streams);
            return STATUS_CMD_ERROR;
        }

        fd = opened_fd.fd();
        if (fstat(fd, &buf) == -1) {
            wcstring esc = escape_string(argv[optind]);
            streams.err.append_format(_(L"%ls: Error encountered while sourcing file '%ls':\n"),
                                      cmd, esc.c_str());
            builtin_wperror(L"source", streams);
            return STATUS_CMD_ERROR;
        }

        if (!S_ISREG(buf.st_mode)) {
            wcstring esc = escape_string(argv[optind]);
            streams.err.append_format(_(L"%ls: '%ls' is not a file\n"), cmd, esc.c_str());
            return STATUS_CMD_ERROR;
        }

        func_filename = std::make_shared<wcstring>(argv[optind]);
    }
    assert(fd >= 0 && "Should have a valid fd");
    assert(func_filename && "Should have valid function filename");

    const block_t *sb = parser.push_block(block_t::source_block(func_filename));
    auto &ld = parser.libdata();
    scoped_push<filename_ref_t> filename_push{&ld.current_filename, func_filename};

    // Construct argv from our null-terminated list.
    // This is slightly subtle. If this is a bare `source` with no args then `argv + optind` already
    // points to the end of argv. Otherwise we want to skip the file name to get to the args if any.
    wcstring_list_t argv_list;
    const wchar_t *const *remaining_args = argv + optind + (argc == optind ? 0 : 1);
    for (size_t i = 0, len = null_terminated_array_length(remaining_args); i < len; i++) {
        argv_list.push_back(remaining_args[i]);
    }
    parser.vars().set_argv(std::move(argv_list));

    retval = reader_read(parser, fd, streams.io_chain ? *streams.io_chain : io_chain_t());

    parser.pop_block(sb);

    if (retval != STATUS_CMD_OK) {
        wcstring esc = escape_string(*func_filename);
        streams.err.append_format(_(L"%ls: Error while reading file '%ls'\n"), cmd,
                                  esc == L"-" ? L"<stdin>" : esc.c_str());
    } else {
        retval = parser.get_last_status();
    }

    // Do not close fd after calling reader_read. reader_read automatically closes it before calling
    // eval.
    return retval;
}
