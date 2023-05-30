#include "config.h"  // IWYU pragma: keep

#include "kill.h"

wcstring kill_yank_rotate() {
    wcstring front;
    kill_yank_rotate(front);
    return front;
}

wcstring kill_yank() {
    wcstring front;
    kill_yank(front);
    return front;
}

std::vector<wcstring> kill_entries() {
    wcstring_list_ffi_t entries;
    kill_entries(entries);
    return std::move(entries.vals);
}
