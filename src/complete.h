/// Prototypes for functions related to tab-completion.
///
/// These functions are used for storing and retrieving tab-completion data, as well as for
/// performing tab-completion.
#ifndef FISH_COMPLETE_H
#define FISH_COMPLETE_H

#include "config.h"  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "common.h"
#include "expand.h"
#include "parser.h"
#include "wcstringutil.h"

struct completion_mode_t {
    /// If set, skip file completions.
    bool no_files{false};
    bool force_files{false};

    /// If set, require a parameter after completion.
    bool requires_param{false};
};

/// Character that separates the completion and description on programmable completions.
#define PROG_COMPLETE_SEP L'\t'

enum {
    /// Do not insert space afterwards if this is the only completion. (The default is to try insert
    /// a space).
    COMPLETE_NO_SPACE = 1 << 0,
    /// This is not the suffix of a token, but replaces it entirely.
    COMPLETE_REPLACES_TOKEN = 1 << 1,
    /// This completion may or may not want a space at the end - guess by checking the last
    /// character of the completion.
    COMPLETE_AUTO_SPACE = 1 << 2,
    /// This completion should be inserted as-is, without escaping.
    COMPLETE_DONT_ESCAPE = 1 << 3,
    /// If you do escape, don't escape tildes.
    COMPLETE_DONT_ESCAPE_TILDES = 1 << 4,
    /// Do not sort supplied completions
    COMPLETE_DONT_SORT = 1 << 5,
    /// This completion looks to have the same string as an existing argument.
    COMPLETE_DUPLICATES_ARGUMENT = 1 << 6,
    /// This completes not just a token but replaces the entire commandline.
    COMPLETE_REPLACES_COMMANDLINE = 1 << 7,
};
using complete_flags_t = uint8_t;

#if INCLUDE_RUST_HEADERS
#include "complete.rs.h"
#else
struct CompletionListFfi;
struct Completion;
struct CompletionRequestOptions;
#endif

using completion_t = Completion;
using completion_request_options_t = CompletionRequestOptions;
using completion_list_t = CompletionListFfi;

#endif
