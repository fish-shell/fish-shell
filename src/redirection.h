#ifndef FISH_REDIRECTION_H
#define FISH_REDIRECTION_H

#include <vector>

#include "common.h"
#include "maybe.h"

/// This file supports specifying and applying redirections.

enum class redirection_mode_t {
    overwrite,  // normal redirection: > file.txt
    append,     // appending redirection: >> file.txt
    input,      // input redirection: < file.txt
    fd,         // fd redirection: 2>&1
    noclob      // noclobber redirection: >? file.txt
};

class io_chain_t;

/// A struct which represents a redirection specification from the user.
/// Here the file descriptors don't represent open files - it's purely textual.
struct redirection_spec_t {
    /// The redirected fd, or -1 on overflow.
    /// In the common case of a pipe, this is 1 (STDOUT_FILENO).
    /// For example, in the case of "3>&1" this will be 3.
    int fd{-1};

    /// The redirection mode.
    redirection_mode_t mode{redirection_mode_t::overwrite};

    /// The target of the redirection.
    /// For example in "3>&1", this will be "1".
    /// In "< file.txt" this will be "file.txt".
    wcstring target{};

    /// \return if this is a close-type redirection.
    bool is_close() const { return mode == redirection_mode_t::fd && target == L"-"; }

    /// Attempt to parse target as an fd. Return the fd, or none() if none.
    maybe_t<int> get_target_as_fd() const;

    /// \return the open flags for this redirection.
    int oflags() const;

    redirection_spec_t(int fd, redirection_mode_t mode, wcstring target)
        : fd(fd), mode(mode), target(std::move(target)) {}
};
using redirection_spec_list_t = std::vector<redirection_spec_t>;

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

    /// Append a dup2 action.
    void add_dup2(int src, int target) {
        assert(src >= 0 && target >= 0 && "Invalid fd in add_dup2");
        // Note: record these even if src and target is the same.
        // This is a note that we must clear the CLO_EXEC bit.
        actions_.push_back(action_t{src, target});
    }

    /// Append a close action.
    void add_close(int fd) {
        assert(fd >= 0 && "Invalid fd in add_close");
        actions_.push_back(action_t{fd, -1});
    }

    dup2_list_t() = default;

   public:
    ~dup2_list_t();

    /// Disable copying.
    dup2_list_t(const dup2_list_t &) = delete;
    void operator=(const dup2_list_t &) = delete;

    dup2_list_t(dup2_list_t &&) = default;
    dup2_list_t &operator=(dup2_list_t &&) = default;

    /// \return the list of dup2 actions.
    const std::vector<action_t> &get_actions() const { return actions_; }

    /// Produce a dup_fd_list_t from an io_chain. This may not be called before fork().
    /// The result contains the list of fd actions (dup2 and close), as well as the list
    /// of fds opened.
    static dup2_list_t resolve_chain(const io_chain_t &);

    /// \return the fd ultimately dup'd to a target fd, or -1 if the target is closed.
    /// For example, if target fd is 1, and we have a dup2 chain 5->3 and 3->1, then we will
    /// return 5. If the target is not referenced in the chain, returns target.
    int fd_for_target_fd(int target) const;
};

#endif
