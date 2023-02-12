// Functions for handling event triggers.
#include "config.h"  // IWYU pragma: keep

#include "event.h"

#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <utility>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "io.h"
#include "maybe.h"
#include "parser.h"
#include "proc.h"
#include "signals.h"
#include "termsize.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

const wchar_t *const event_filter_names[] = {L"signal",       L"variable", L"exit",
                                             L"process-exit", L"job-exit", L"caller-exit",
                                             L"generic",      nullptr};

void event_fire_generic(parser_t &parser, const wcstring &name, const wcstring_list_t args) {
    std::vector<wcharz_t> ffi_args;
    for (const auto &arg : args) ffi_args.push_back(arg.c_str());
    event_fire_generic_ffi(parser, name, ffi_args);
}
