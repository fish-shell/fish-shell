#include <algorithm>
#include <memory>

#include "cxx.h"
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
