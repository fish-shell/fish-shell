#include "config.h"

#include "trace.h"

#include "common.h"
#include "flog.h"
#include "parser.h"

static const wcstring VAR_fish_trace = L"fish_trace";

bool trace_enabled(const parser_t &parser) {
    const auto &ld = parser.libdata();
    if (ld.suppress_fish_trace) return false;
    // TODO: this variable lookup is somewhat expensive, consider how to make this cheaper.
    return !parser.vars().get(VAR_fish_trace).missing_or_empty();
}

/// Trace an "argv": a list of arguments where the first is the command.
void trace_argv(const parser_t &parser, const wchar_t *command, const wcstring_list_t &argv) {
    // Format into a string to prevent interleaving with flog in other threads.
    // Add the + prefix.
    wcstring trace_text(parser.blocks().size(), '+');

    if (command && command[0]) {
        trace_text.push_back(L' ');
        trace_text.append(command);
    }
    for (const wcstring &arg : argv) {
        trace_text.push_back(L' ');
        trace_text.append(escape_string(arg, ESCAPE_ALL));
    }
    trace_text.push_back('\n');
    log_extra_to_flog_file(trace_text);
}

void trace_if_enabled(const parser_t &parser, const wchar_t *command, const wcstring_list_t &argv) {
    if (trace_enabled(parser)) trace_argv(parser, command, argv);
}
