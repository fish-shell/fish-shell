// Functions for performing sanity checks on the program state.
#include "config.h"  // IWYU pragma: keep

#include <unistd.h>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "history.h"
#include "kill.h"
#include "proc.h"
#include "reader.h"
#include "sanity.h"

/// Status from earlier sanity checks.
static int insane;

void sanity_lose() {
    debug(0, _(L"Errors detected, shutting down. Break on sanity_lose() to debug."));
    insane = 1;
}

int sanity_check() {
    if (!insane && shell_is_interactive()) history_sanity_check();
    if (!insane) reader_sanity_check();
    if (!insane) kill_sanity_check();
    if (!insane) proc_sanity_check();

    return insane;
}

void validate_pointer(const void *ptr, const wchar_t *err, int null_ok) {
    // Test if the pointer data crosses a segment boundary.
    if ((0x00000003l & (intptr_t)ptr) != 0) {
        debug(0, _(L"The pointer '%ls' is invalid"), err);
        sanity_lose();
    }

    if ((!null_ok) && (ptr == 0)) {
        debug(0, _(L"The pointer '%ls' is null"), err);
        sanity_lose();
    }
}
