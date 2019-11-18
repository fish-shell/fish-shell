/// Support for fish_trace.
#ifndef FISH_TRACE_H
#define FISH_TRACE_H

#include "config.h"  // IWYU pragma: keep

#include "common.h"

class parser_t;
class process_t;

/// Trace an "argv": a list of arguments. Each argument is escaped.
/// If \p command is not null, it is traced first (and not escaped)
void trace_argv(const parser_t &parser, const wchar_t *command, const wcstring_list_t &argv);

/// \return whether tracing is enabled.
bool trace_enabled(const parser_t &parser);

/// Convenience helper to trace a single string if tracing is enabled.
void trace_if_enabled(const parser_t &parser, const wchar_t *command,
                      const wcstring_list_t &argv = {});

#endif
