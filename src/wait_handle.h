// Support for handling pids that are no longer fish jobs.
// This includes pids that have been disowned ("forgotten") and background jobs which have finished,
// but may be wait'ed.
#ifndef FISH_WAIT_HANDLE_H
#define FISH_WAIT_HANDLE_H

#include "config.h"  // IWYU pragma: keep

#include <unistd.h>

#include <list>
#include <unordered_map>
#include <vector>

#include "common.h"

/// The bits of a job necessary to support 'wait' and '--on-process-exit'.
/// This may outlive the job.
struct wait_handle_t {
    /// Construct from a pid and base name.
    wait_handle_t(pid_t pid, wcstring name) : pid(pid), base_name(std::move(name)) {}

    /// The pid of this process.
    pid_t pid{};

    /// The internal job id of the job which contained this process.
    /// This is initially 0; it is set when the job is completed.
    internal_job_id_t internal_job_id{};

    /// The "base name" of this process.
    /// For example if the process is "/bin/sleep" then this will be 'sleep'.
    wcstring base_name{};

    /// The value appropriate for populating $status, if completed.
    int status{0};

    /// Set to true when the process is completed.
    bool completed{false};
};
using wait_handle_ref_t = std::shared_ptr<wait_handle_t>;

/// Support for storing a list of wait handles, with a max limit set at initialization.
/// Note this class is not safe for concurrent access.
class wait_handle_store_t : noncopyable_t {
   public:
    // Our wait handles are arranged in a linked list for its iterator invalidation semantics: we
    // may remove one without needing to update the map from pid -> handle.
    using wait_handle_list_t = std::list<wait_handle_ref_t>;

    /// Construct with a max limit on the number of handles we will remember.
    /// The default is 1024, which is zsh's default.
    explicit wait_handle_store_t(size_t limit = 1024);

    /// Add a wait handle to the store. This may remove the oldest handle, if our limit is exceeded.
    /// It may also remove any existing handle with that pid.
    /// For convenience, this does nothing if wh is null.
    void add(wait_handle_ref_t wh);

    /// \return the wait handle for a pid, or nullptr if there is none.
    /// This is a fast lookup.
    wait_handle_ref_t get_by_pid(pid_t pid) const;

    /// Remove a given wait handle, if present in this store.
    void remove(const wait_handle_ref_t &wh);

    /// Remove the wait handle for a pid, if present in this store.
    void remove_by_pid(pid_t pid);

    /// Get the list of all wait handles.
    const wait_handle_list_t &get_list() const { return handles_; }

    /// Convenience to return the size, for testing.
    size_t size() const { return handles_.size(); }

   private:
    using list_node_t = typename wait_handle_list_t::iterator;

    /// The list of all wait handles. New ones come on the front, the last one is oldest.
    wait_handle_list_t handles_{};

    /// Map from pid to the wait handle's position in the list.
    std::unordered_map<pid_t, list_node_t> handle_map_{};

    /// Max supported wait handles.
    const size_t limit_;
};

#endif
