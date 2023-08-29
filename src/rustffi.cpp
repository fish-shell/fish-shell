#include <memory>

#include "wutil.h"

extern "C" {
void fishffi$unique_ptr$wcstring$null(std::unique_ptr<wcstring> *ptr) noexcept {
    new (ptr) std::unique_ptr<wcstring>();
}
void fishffi$unique_ptr$wcstring$raw(std::unique_ptr<wcstring> *ptr, wcstring *raw) noexcept {
    new (ptr) std::unique_ptr<wcstring>(raw);
}
const wcstring *fishffi$unique_ptr$wcstring$get(const std::unique_ptr<wcstring> &ptr) noexcept {
    return ptr.get();
}
wcstring *fishffi$unique_ptr$wcstring$release(std::unique_ptr<wcstring> &ptr) noexcept {
    return ptr.release();
}
void fishffi$unique_ptr$wcstring$drop(std::unique_ptr<wcstring> *ptr) noexcept {
    ptr->~unique_ptr();
}
}  // extern "C"
