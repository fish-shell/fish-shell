// Functions for executing the jobs builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#ifdef HAVE__PROC_SELF_STAT
#include <sys/time.h>
#endif

#include "builtin.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "proc.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

class parser_t;

/// The jobs builtin. Used for printing running jobs. Defined in builtin_jobs.c.
int builtin_eval(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);

    wcstring new_cmd;
    for (size_t i = 1; i < argc; ++i) {
        new_cmd += L' ';
        new_cmd += argv[i];
    }

    // debug(1, "new_cmd: %ls", new_cmd.c_str());

    auto status = proc_get_last_status();
    if (argc > 1) {
        if (parser.eval(new_cmd.c_str(), *streams.io_chain, block_type_t::TOP) != 0) {
            return STATUS_CMD_ERROR;
        }
        status = proc_get_last_status();
    } else {
        status = STATUS_CMD_OK;
    }

    return status;
}
