#ifndef FISH_REDIRECTION_H
#define FISH_REDIRECTION_H

#include <vector>

#include "common.h"
#include "io.h"
#include "maybe.h"

/// This file supports "applying" redirections.

/// A class representing a sequence of basic redirections.
class dup2_list_t {
   public:
    /// A type that represents the action dup2(src, target).
    /// If target is negative, this represents close(src).
    /// Note none of the fds here are considered 'owned'.
    struct action_t {
        int src;
        int target;
    };

   private:
    /// The list of actions.
    std::vector<action_t> actions_;

    /// The list of fds that we opened, and are responsible for closing.
    std::vector<autoclose_fd_t> opened_fds_;

    /// Append a dup2 action.
    void add_dup2(int src, int target) {
        assert(src >= 0 && target >= 0 && "Invalid fd in add_dup2");
        if (src != target) {
            actions_.push_back(action_t{src, target});
        }
    }

    /// Append a close action.
    void add_close(int fd) {
        assert(fd >= 0 && "Invalid fd in add_close");
        actions_.push_back(action_t{fd, -1});
    }

    dup2_list_t() = default;

   public:
    ~dup2_list_t();

    /// Disable copying because we own our fds.
    dup2_list_t(const dup2_list_t &) = delete;
    void operator=(const dup2_list_t &) = delete;

    dup2_list_t(dup2_list_t &&) = default;
    dup2_list_t &operator=(dup2_list_t &&) = default;

    /// \return the list of dup2 actions.
    const std::vector<action_t> &get_actions() const { return actions_; }

    /// Produce a dup_fd_list_t from an io_chain. This may not be called before fork().
    /// The result contains the list of fd actions (dup2 and close), as well as the list
    /// of fds opened.
    static maybe_t<dup2_list_t> resolve_chain(const io_chain_t &);

    /// \return the fd ultimately dup'd to a target fd, or -1 if the target is closed.
    /// For example, if target fd is 1, and we have a dup2 chain 5->3 and 3->1, then we will
    /// return 5. If the target is not referenced in the chain, returns target.
    int fd_for_target_fd(int target) const;
};

#endif
