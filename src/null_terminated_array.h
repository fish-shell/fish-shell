// Support for null-terminated arrays like char**.
#ifndef FISH_NULL_TERMINATED_ARRAY_H
#define FISH_NULL_TERMINATED_ARRAY_H

#include "config.h"  // IWYU pragma: keep

#include <string>
#include <vector>

#include "common.h"

wchar_t **make_null_terminated_array(const wcstring_list_t &lst);
char **make_null_terminated_array(const std::vector<std::string> &lst);

// Helper class for managing a null-terminated array of null-terminated strings (of some char type).
template <typename CharT>
class null_terminated_array_t {
    using string_list_t = std::vector<std::basic_string<CharT>>;

    CharT **array{nullptr};

    // No assignment or copying.
    void operator=(null_terminated_array_t rhs) = delete;
    null_terminated_array_t(const null_terminated_array_t &) = delete;

    size_t size() const {
        size_t len = 0;
        if (array != nullptr) {
            while (array[len] != nullptr) {
                len++;
            }
        }
        return len;
    }

    void free(void) {
        ::free(reinterpret_cast<void *>(array));
        array = nullptr;
    }

   public:
    null_terminated_array_t() = default;

    explicit null_terminated_array_t(const string_list_t &argv)
        : array(make_null_terminated_array(argv)) {}

    ~null_terminated_array_t() { this->free(); }

    null_terminated_array_t(null_terminated_array_t &&rhs) : array(rhs.array) {
        rhs.array = nullptr;
    }

    null_terminated_array_t operator=(null_terminated_array_t &&rhs) {
        free();
        array = rhs.array;
        rhs.array = nullptr;
    }

    void set(const string_list_t &argv) {
        this->free();
        this->array = make_null_terminated_array(argv);
    }

    /// Convert from a null terminated list to a vector of strings.
    static string_list_t to_list(const CharT *const *arr) {
        string_list_t result;
        for (auto cursor = arr; cursor && *cursor; cursor++) {
            result.push_back(*cursor);
        }
        return result;
    }

    /// Instance method.
    string_list_t to_list() const { return to_list(array); }

    const CharT *const *get() const { return array; }
    CharT **get() { return array; }

    void clear() { this->free(); }
};

// Helper function to convert from a null_terminated_array_t<wchar_t> to a
// null_terminated_array_t<char_t>.
void convert_wide_array_to_narrow(const null_terminated_array_t<wchar_t> &arr,
                                  null_terminated_array_t<char> *output);

#endif  // FISH_NULL_TERMINATED_ARRAY_H
