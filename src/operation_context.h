#ifndef FISH_OPERATION_CONTEXT_H
#define FISH_OPERATION_CONTEXT_H

#if INCLUDE_RUST_HEADERS
#include "operation_context.rs.h"
#else
struct OperationContext;
#endif

using operation_context_t = OperationContext;

#if 0
#include "parser.h"

struct job_group_t;

/// A common helper which always returns false.
bool no_cancel();

/// Default limits for expansion.
enum expansion_limit_t : size_t {
    /// The default maximum number of items from expansion.
    kExpansionLimitDefault = 512 * 1024,

    /// A smaller limit for background operations like syntax highlighting.
    kExpansionLimitBackground = 512,
};

/// A operation_context_t is a simple property bag which wraps up data needed for highlighting,
/// expansion, completion, and more.
class operation_context_t {
   public:
    // The parser, if this is a foreground operation. If this is a background operation, this may be
    // nullptr.
    std::shared_ptr<parser_t> parser;

    // The set of variables. It is the creator's responsibility to ensure this lives as log as the
    // context itself.
    const environment_t &vars;

    // The limit in the number of expansions which should be produced.
    const size_t expansion_limit;

    /// The job group of the parental job.
    /// This is used only when expanding command substitutions. If this is set, any jobs created by
    /// the command substitutions should use this tree.
    std::shared_ptr<job_group_t> job_group{};

    // A function which may be used to poll for cancellation.
    cancel_checker_t cancel_checker;

    // Invoke the cancel checker. \return if we should cancel.
    bool check_cancel() const { return cancel_checker(); }

    // \return an "empty" context which contains no variables, no parser, and never cancels.
    static operation_context_t empty();

    // \return an operation context that contains only global variables, no parser, and never
    // cancels.
    static operation_context_t globals();

    /// Construct from a full set of properties.
    operation_context_t(std::shared_ptr<parser_t> parser, const environment_t &vars,
                        cancel_checker_t cancel_checker,
                        size_t expansion_limit = kExpansionLimitDefault);

    /// Construct from vars alone.
    explicit operation_context_t(const environment_t &vars,
                                 size_t expansion_limit = kExpansionLimitDefault)
        : operation_context_t(nullptr, vars, no_cancel, expansion_limit) {}

    ~operation_context_t();
};

#endif
#endif
