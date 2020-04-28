// Functions for executing the eval builtin.
#include "config.h"  // IWYU pragma: keep

#include <cerrno>
#include <cstddef>

#include "builtin.h"
#include "common.h"
#include "exec.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

/// Implementation of eval builtin.
int builtin_eval(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    int argc = builtin_count_args(argv);
    if (argc <= 1) {
        return STATUS_CMD_OK;
    }

    wcstring new_cmd;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) new_cmd += L' ';
        new_cmd += argv[i];
    }

    // If stdout is piped, then its output must go to the streams, not to the io_chain in our
    // streams, because the pipe may be intended to be consumed by a process which
    // is not yet launched (#6806). If stdout is NOT redirected, it must see the tty (#6955). So
    // create a bufferfill for stdout if and only if stdout is piped.
    // Note do not do this if stdout is merely redirected (say, to a file); we don't want to
    // buffer in that case.
    shared_ptr<io_bufferfill_t> stdout_fill{};
    if (streams.out_is_piped) {
        stdout_fill =
            io_bufferfill_t::create(fd_set_t{}, parser.libdata().read_limit, STDOUT_FILENO);
        if (!stdout_fill) {
            // We were unable to create a pipe, probably fd exhaustion.
            return STATUS_CMD_ERROR;
        }
    }

    // Of course the same applies to stderr.
    shared_ptr<io_bufferfill_t> stderr_fill{};
    if (streams.err_is_piped) {
        stderr_fill =
            io_bufferfill_t::create(fd_set_t{}, parser.libdata().read_limit, STDERR_FILENO);
        if (!stderr_fill) {
            return STATUS_CMD_ERROR;
        }
    }

    // Construct the full io chain, perhaps with our bufferfills appended.
    io_chain_t ios = *streams.io_chain;
    if (stdout_fill) ios.push_back(stdout_fill);
    if (stderr_fill) ios.push_back(stderr_fill);

    const auto cached_exec_count = parser.libdata().exec_count;
    int status = STATUS_CMD_OK;
    if (parser.eval(new_cmd, ios, block_type_t::top, streams.parent_pgid) != eval_result_t::ok) {
        status = STATUS_CMD_ERROR;
    } else if (cached_exec_count == parser.libdata().exec_count) {
        // Issue #5692, in particular, to catch `eval ""`, `eval "begin; end;"`, etc.
        // where we have an argument but nothing is executed.
        status = STATUS_CMD_OK;
    } else {
        status = parser.get_last_status();
    }

    // Finish the bufferfills - exhaust and close our pipes.
    // Copy the output from the bufferfill back to the streams.
    // Note it is important that we hold no other references to the bufferfills here - they need to
    // deallocate to close.
    ios.clear();
    if (stdout_fill) {
        std::shared_ptr<io_buffer_t> output = io_bufferfill_t::finish(std::move(stdout_fill));
        streams.out.append_narrow_buffer(output->buffer());
    }
    if (stderr_fill) {
        std::shared_ptr<io_buffer_t> errput = io_bufferfill_t::finish(std::move(stderr_fill));
        streams.err.append_narrow_buffer(errput->buffer());
    }
    return status;
}
