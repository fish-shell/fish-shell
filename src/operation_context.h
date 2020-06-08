#ifndef FISH_OPERATION_CONTEXT_H
#define FISH_OPERATION_CONTEXT_H

#include <memory>

#include "common.h"

class environment_t;
class parser_t;
class job_group_t;

/// A common helper which always returns false.
bool no_cancel();

/// A operation_context_t is a simple property bag which wraps up data needed for highlighting,
/// expansion, completion, and more.
/// It contains the following triple:
///   1. A parser. This is often null. If not null, it may be used to execute fish script. If null,
///   then this is a background operation and fish script must not be executed.
///   2. A variable set. This is never null. This may differ from the variables in the parser.
///   3. A cancellation checker. This is a function which you may call to detect that the operation
///   is no longer necessary and should be cancelled.
class operation_context_t {
   public:
    // The parser, if this is a foreground operation. If this is a background operation, this may be
    // nullptr.
    std::shared_ptr<parser_t> parser;

    // The set of variables. It is the creator's responsibility to ensure this lives as log as the
    // context itself.
    const environment_t &vars;

    /// The job group of the parental job.
    /// This is used only when expanding command substitutions. If this is set, any jobs created by
    /// the command substitions should use this tree.
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

    /// Construct from the full triple of a parser, vars, and cancel checker.
    operation_context_t(std::shared_ptr<parser_t> parser, const environment_t &vars,
                        cancel_checker_t cancel_checker);

    /// Construct from vars alone.
    explicit operation_context_t(const environment_t &vars)
        : operation_context_t(nullptr, vars, no_cancel) {}

    ~operation_context_t();
};

#endif
