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

    // The output and errput of eval must go to our streams, not to the io_chain in our streams,
    // because they may contain a pipe which is intended to be consumed by a process which is not
    // yet launched (#6806). Make bufferfills to capture it.
    // TODO: we are sloppy and do not honor other redirections; eval'd code won't see for example a
    // redirection of fd 3.
    shared_ptr<io_bufferfill_t> stdout_fill =
        io_bufferfill_t::create(fd_set_t{}, parser.libdata().read_limit, STDOUT_FILENO);
    shared_ptr<io_bufferfill_t> stderr_fill =
        io_bufferfill_t::create(fd_set_t{}, parser.libdata().read_limit, STDERR_FILENO);
    if (!stdout_fill || !stderr_fill) {
        // This can happen if we are unable to create a pipe.
        return STATUS_CMD_ERROR;
    }

    const auto cached_exec_count = parser.libdata().exec_count;
    int status = STATUS_CMD_OK;
    if (parser.eval(new_cmd, io_chain_t{stdout_fill, stderr_fill}, block_type_t::top,
                    streams.parent_pgid) != eval_result_t::ok) {
        status = STATUS_CMD_ERROR;
    } else if (cached_exec_count == parser.libdata().exec_count) {
        // Issue #5692, in particular, to catch `eval ""`, `eval "begin; end;"`, etc.
        // where we have an argument but nothing is executed.
        status = STATUS_CMD_OK;
    } else {
        status = parser.get_last_status();
    }

    // Finish the bufferfills - exhaust and close our pipes.
    // Note it is important that we hold no other references to the bufferfills here - they need to
    // deallocate to close.
    std::shared_ptr<io_buffer_t> output = io_bufferfill_t::finish(std::move(stdout_fill));
    std::shared_ptr<io_buffer_t> errput = io_bufferfill_t::finish(std::move(stderr_fill));

    // Copy the output from the bufferfill back to the streams.
    streams.out.append_narrow_buffer(output->buffer());
    streams.err.append_narrow_buffer(errput->buffer());

    return status;
}
