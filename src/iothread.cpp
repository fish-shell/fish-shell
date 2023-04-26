#include "config.h"  // IWYU pragma: keep

#include "iothread.h"

extern "C" const void *iothread_trampoline(const void *c) {
    iothread_callback_t *callback = (iothread_callback_t *)c;
    auto *result = (callback->callback)(callback->param);
    delete callback;
    return result;
}

extern "C" const void *iothread_trampoline2(const void *c, const void *p) {
    iothread_callback_t *callback = (iothread_callback_t *)c;
    auto *result = (callback->callback)(p);
    delete callback;
    return result;
}
