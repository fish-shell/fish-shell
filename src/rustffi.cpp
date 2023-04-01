#include <memory>

#include "wutil.h"

extern "C" {
void ghotiffi$unique_ptr$wcstring$null(std::unique_ptr<wcstring> *ptr) noexcept {
    new (ptr) std::unique_ptr<wcstring>();
}
void ghotiffi$unique_ptr$wcstring$raw(std::unique_ptr<wcstring> *ptr, wcstring *raw) noexcept {
    new (ptr) std::unique_ptr<wcstring>(raw);
}
const wcstring *ghotiffi$unique_ptr$wcstring$get(const std::unique_ptr<wcstring> &ptr) noexcept {
    return ptr.get();
}
wcstring *ghotiffi$unique_ptr$wcstring$release(std::unique_ptr<wcstring> &ptr) noexcept {
    return ptr.release();
}
void ghotiffi$unique_ptr$wcstring$drop(std::unique_ptr<wcstring> *ptr) noexcept {
    ptr->~unique_ptr();
}
}  // extern "C"
