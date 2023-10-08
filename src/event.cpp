// event.h and event.cpp only contain cpp-side ffi compat code to make event.rs.h a drop-in
// replacement. There is no logic still in here that needs to be ported to rust.

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

void event_fire_generic(const parser_t &parser, const wcstring &name,
                        const std::vector<wcstring> &args) {
    wcstring_list_ffi_t ffi_args;
    for (const auto &arg : args) ffi_args.push(arg);
    event_fire_generic_ffi(parser, name, ffi_args);
}
