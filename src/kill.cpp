// The killring.
//
// Works like the killring in emacs and readline. The killring is cut and paste with a memory of
// previous cuts.
#include "config.h"  // IWYU pragma: keep

#include "kill.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep

/** Kill ring */
static owning_lock<std::list<wcstring>> s_kill_list;

void kill_add(wcstring str) {
    if (!str.empty()) {
        s_kill_list.acquire()->push_front(std::move(str));
    }
}

void kill_replace(const wcstring &old, const wcstring &newv) {
    auto kill_list = s_kill_list.acquire();
    // Remove old.
    auto iter = std::find(kill_list->begin(), kill_list->end(), old);
    if (iter != kill_list->end()) kill_list->erase(iter);

    // Add new.
    if (!newv.empty()) {
        kill_list->push_front(newv);
    }
}

wcstring kill_yank_rotate() {
    auto kill_list = s_kill_list.acquire();
    // Move the first element to the end.
    if (kill_list->empty()) {
        return {};
    }
    kill_list->splice(kill_list->end(), *kill_list, kill_list->begin());
    return kill_list->front();
}

wcstring kill_yank() {
    auto kill_list = s_kill_list.acquire();
    if (kill_list->empty()) {
        return {};
    }
    return kill_list->front();
}

std::vector<wcstring> kill_entries() {
    auto kill_list = s_kill_list.acquire();
    return std::vector<wcstring>{kill_list->begin(), kill_list->end()};
}

wcstring_list_ffi_t kill_entries_ffi() { return kill_entries(); }
