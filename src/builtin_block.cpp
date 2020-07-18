// Implementation of the bind builtin.
#include "config.h"  // IWYU pragma: keep

#include "builtin_block.h"

#include <cstddef>

#include "builtin.h"
#include "common.h"
#include "event.h"
#include "fallback.h"  // IWYU pragma: keep
#include "io.h"
#include "parser.h"
#include "wgetopt.h"
#include "wutil.h"  // IWYU pragma: keep

enum { UNSET, GLOBAL, LOCAL };
struct block_cmd_opts_t {
    int scope = UNSET;
    bool erase = false;
    bool print_help = false;
};

static int parse_cmd_opts(block_cmd_opts_t &opts, int *optind,  //!OCLINT(high ncss method)
                          int argc, wchar_t **argv, parser_t &parser, io_streams_t &streams) {
    wchar_t *cmd = argv[0];
    static const wchar_t *const short_options = L":eghl";
    static const struct woption long_options[] = {{L"erase", no_argument, nullptr, 'e'},
                                                  {L"local", no_argument, nullptr, 'l'},
                                                  {L"global", no_argument, nullptr, 'g'},
                                                  {L"help", no_argument, nullptr, 'h'},
                                                  {nullptr, 0, nullptr, 0}};

    int opt;
    wgetopter_t w;
    while ((opt = w.wgetopt_long(argc, argv, short_options, long_options, nullptr)) != -1) {
        switch (opt) {
            case 'h': {
                opts.print_help = true;
                break;
            }
            case 'g': {
                opts.scope = GLOBAL;
                break;
            }
            case 'l': {
                opts.scope = LOCAL;
                break;
            }
            case 'e': {
                opts.erase = true;
                break;
            }
            case ':': {
                builtin_missing_argument(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            case '?': {
                builtin_unknown_option(parser, streams, cmd, argv[w.woptind - 1]);
                return STATUS_INVALID_ARGS;
            }
            default: {
                DIE("unexpected retval from wgetopt_long");
            }
        }
    }

    *optind = w.woptind;
    return STATUS_CMD_OK;
}

/// The block builtin, used for temporarily blocking events.
maybe_t<int> builtin_block(parser_t &parser, io_streams_t &streams, wchar_t **argv) {
    const wchar_t *cmd = argv[0];
    int argc = builtin_count_args(argv);
    block_cmd_opts_t opts;

    int optind;
    int retval = parse_cmd_opts(opts, &optind, argc, argv, parser, streams);
    if (retval != STATUS_CMD_OK) return retval;

    if (opts.print_help) {
        builtin_print_help(parser, streams, cmd);
        return STATUS_CMD_OK;
    }

    if (opts.erase) {
        if (opts.scope != UNSET) {
            streams.err.append_format(_(L"%ls: Can not specify scope when removing block\n"), cmd);
            return STATUS_INVALID_ARGS;
        }

        if (parser.global_event_blocks.empty()) {
            streams.err.append_format(_(L"%ls: No blocks defined\n"), cmd);
            return STATUS_CMD_ERROR;
        }
        parser.global_event_blocks.pop_front();
        return STATUS_CMD_OK;
    }

    size_t block_idx = 0;
    block_t *block = parser.block_at_index(block_idx);

    event_blockage_t eb = {};

    switch (opts.scope) {
        case LOCAL: {
            // If this is the outermost block, then we're global
            if (block_idx + 1 >= parser.blocks().size()) {
                block = nullptr;
            }
            break;
        }
        case GLOBAL: {
            block = nullptr;
            break;
        }
        case UNSET: {
            while (block && !block->is_function_call()) {
                // Set it in function scope
                block = parser.block_at_index(++block_idx);
            }
            break;
        }
        default: {
            DIE("unexpected scope");
        }
    }
    if (block) {
        block->event_blocks.push_front(eb);
    } else {
        parser.global_event_blocks.push_front(eb);
    }

    return STATUS_CMD_OK;
}
