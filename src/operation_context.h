#ifndef FISH_OPERATION_CONTEXT_H
#define FISH_OPERATION_CONTEXT_H

#include <stddef.h>

#if INCLUDE_RUST_HEADERS
#include "operation_context.rs.h"
#else
struct OperationContext;
#endif

using operation_context_t = OperationContext;

/// Default limits for expansion.
enum expansion_limit_t : size_t {
    /// The default maximum number of items from expansion.
    kExpansionLimitDefault = 512 * 1024,

    /// A smaller limit for background operations like syntax highlighting.
    kExpansionLimitBackground = 512,
};

#endif
