#include "config.h" // IWYU pragma: keep

#include "wait_handle.h"

#include <iterator>

wait_handle_store_t::wait_handle_store_t(size_t limit) : limit_(limit) {}

void wait_handle_store_t::add(wait_handle_ref_t wh) {
    if (!wh || wh->pid <= 0) return;
    pid_t pid = wh->pid;

    remove_by_pid(wh->pid);
    handles_.push_front(std::move(wh));
    handle_map_[pid] = std::begin(handles_);

    // Remove oldest until we reach our limit.
    while (handles_.size() > limit_) {
        handle_map_.erase(handles_.back()->pid);
        handles_.pop_back();
    }
}

void wait_handle_store_t::remove(const wait_handle_ref_t &wh) {
    // Note: this differs from remove_by_pid because we verify that the handle is the same.
    if (!wh) return;
    auto iter = handle_map_.find(wh->pid);
    if (iter != handle_map_.end() && *iter->second == wh) {
        // Note this may deallocate the wait handle, leaving it dangling.
        handles_.erase(iter->second);
        handle_map_.erase(iter);
    }
}

void wait_handle_store_t::remove_by_pid(pid_t pid) {
    auto iter = handle_map_.find(pid);
    if (iter != handle_map_.end()) {
        handles_.erase(iter->second);
        handle_map_.erase(iter);
    }
}

wait_handle_ref_t wait_handle_store_t::get_by_pid(pid_t pid) const {
    auto iter = handle_map_.find(pid);
    if (iter == handle_map_.end()) return nullptr;
    return *iter->second;
}
