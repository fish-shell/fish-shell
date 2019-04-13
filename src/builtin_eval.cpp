// Functions for executing the eval builtin.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <stddef.h>
#ifdef HAVE__PROC_SELF_STAT
#include <sys/time.h>
#endif

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

    wcstring new_cmd;
    for (size_t i = 1; i < argc; ++i) {
        if (i > 1) new_cmd += L' ';
        new_cmd += argv[i];
    }

    size_t cached_exec_count = exec_get_exec_count();
    int status = STATUS_CMD_OK;
    if (argc > 1) {
        if (parser.eval(std::move(new_cmd), *streams.io_chain, block_type_t::TOP) != 0) {
            // This indicates a parse error; nothing actually got executed.
            status = STATUS_CMD_ERROR;
        } else if (cached_exec_count == exec_get_exec_count()) {
            // Issue #5692, in particular, to catch `eval ""`, `eval "begin; end;"`, etc.
            // where we have an argument but nothing is executed.
            status = STATUS_CMD_OK;
        } else {
            status = proc_get_last_status();
        }
    }

    return status;
}
