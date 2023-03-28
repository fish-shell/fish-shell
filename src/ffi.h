#include <algorithm>
#include <memory>

#include "cxx.h"
#include "trace.rs.h"
#if INCLUDE_RUST_HEADERS
// For some unknown reason, the definition of rust::Box is in this particular header:
#include "parse_constants.rs.h"
#endif

template <typename T>
inline std::shared_ptr<T> box_to_shared_ptr(rust::Box<T> &&value) {
    T *ptr = value.into_raw();
    std::shared_ptr<T> shared(ptr, [](T *ptr) { rust::Box<T>::from_raw(ptr); });
    return shared;
}

inline static rust::Vec<wcharz_t> to_rust_string_vec(const wcstring_list_t &strings) {
    rust::Vec<wcharz_t> rust_strings;
    rust_strings.reserve(strings.size());
    for (const wcstring &string : strings) {
        rust_strings.emplace_back(string.c_str());
    }
    return rust_strings;
}

inline static void trace_if_enabled(const parser_t &parser, wcharz_t command,
                                    const wcstring_list_t &args = {}) {
    if (trace_enabled(parser)) {
        trace_argv(parser, command, to_rust_string_vec(args));
    }
}
