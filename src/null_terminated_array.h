// Support for null-terminated arrays like char**.
#ifndef FISH_NULL_TERMINATED_ARRAY_H
#define FISH_NULL_TERMINATED_ARRAY_H

#include "config.h"  // IWYU pragma: keep

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "cxx.h"

struct OwningNullTerminatedArrayRef;
#if INCLUDE_RUST_HEADERS
#include "null_terminated_array.rs.h"
#endif

/// This supports the null-terminated array of NUL-terminated strings consumed by exec.
/// Given a list of strings, construct a vector of pointers to those strings contents.
/// This is used for building null-terminated arrays of null-terminated strings.
/// *Important*: the vector stores pointers into the interior of the input strings, which may be
/// subject to the small-string optimization. This means that pointers will be left dangling if any
/// input string is deallocated *or moved*. This class should only be used in transient calls.
template <typename T>
class null_terminated_array_t : noncopyable_t, nonmovable_t {
   public:
    /// \return the list of pointers, appropriate for envp or argv.
    /// Note this returns a mutable array of const strings. The caller may rearrange the strings but
    /// not modify their contents.
    const T **get() {
        assert(!pointers_.empty() && pointers_.back() == nullptr && "Should have null terminator");
        return &pointers_[0];
    }

    // Construct from a list of strings (std::string or wcstring).
    // This holds pointers into the strings.
    explicit null_terminated_array_t(const std::vector<std::basic_string<T>> &strs) {
        pointers_.reserve(strs.size() + 1);
        for (const auto &s : strs) {
            pointers_.push_back(s.c_str());
        }
        pointers_.push_back(nullptr);
    }

   private:
    std::vector<const T *> pointers_{};
};

/// A container which exposes a null-terminated array of pointers to strings that it owns.
/// This is useful for persisted null-terminated arrays, e.g. the exported environment variable
/// list. This assumes char, since we don't need this for wchar_t.
/// Note this class is not movable or copyable as it embeds a null_terminated_array_t.
struct owning_null_terminated_array_t {
   public:
    // Access the null-terminated array of nul-terminated strings, appropriate for execv().
    const char **get();

    // Construct, taking ownership of a list of strings.
    explicit owning_null_terminated_array_t(std::vector<std::string> &&strings);

    // Construct from the FFI side.
    explicit owning_null_terminated_array_t(rust::Box<OwningNullTerminatedArrayRef> impl);

   private:
    const rust::Box<OwningNullTerminatedArrayRef> impl_;
};

/// Helper to convert a list of wcstring to a list of std::string.
std::vector<std::string> wide_string_list_to_narrow(const std::vector<wcstring> &strs);

/// \return the length of a null-terminated array of pointers to something.
template <typename T>
size_t null_terminated_array_length(const T *const *arr) {
    size_t idx = 0;
    while (arr[idx] != nullptr) {
        idx++;
    }
    return idx;
}

#endif  // FISH_NULL_TERMINATED_ARRAY_H
