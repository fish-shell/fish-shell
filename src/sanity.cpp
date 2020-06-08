// Functions for performing sanity checks on the program state.
#include "config.h"  // IWYU pragma: keep

#include "sanity.h"

#include <unistd.h>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "flog.h"
#include "global_safety.h"
#include "history.h"
#include "kill.h"
#include "proc.h"
#include "reader.h"

/// Status from earlier sanity checks.
static relaxed_atomic_bool_t insane{false};

void sanity_lose() {
    FLOG(error, _(L"Errors detected, shutting down. Break on sanity_lose() to debug."));
    insane = true;
}

void validate_pointer(const void *ptr, const wchar_t *err, int null_ok) {
    // Test if the pointer data crosses a segment boundary.
    if ((0x00000003L & reinterpret_cast<intptr_t>(ptr)) != 0) {
        FLOGF(error, _(L"The pointer '%ls' is invalid"), err);
        sanity_lose();
    }

    if ((!null_ok) && (ptr == nullptr)) {
        FLOGF(error, _(L"The pointer '%ls' is null"), err);
        sanity_lose();
    }
}
