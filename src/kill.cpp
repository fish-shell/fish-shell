// The killring.
//
// Works like the killring in emacs and readline. The killring is cut and paste with a memory of
// previous cuts.
#include "config.h"  // IWYU pragma: keep

#include <stdbool.h>
#include <stddef.h>
#include <algorithm>
#include <list>
#include <memory>
#include <string>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "kill.h"
#include "path.h"

/** Kill ring */
typedef std::list<wcstring> kill_list_t;
static kill_list_t kill_list;

void kill_add(const wcstring &str) {
    ASSERT_IS_MAIN_THREAD();
    if (str.empty()) return;
    kill_list.push_front(str);
}

/// Remove first match for specified string from circular list.
static void kill_remove(const wcstring &s) {
    ASSERT_IS_MAIN_THREAD();
    kill_list_t::iterator iter = std::find(kill_list.begin(), kill_list.end(), s);
    if (iter != kill_list.end()) kill_list.erase(iter);
}

void kill_replace(const wcstring &old, const wcstring &newv) {
    kill_remove(old);
    kill_add(newv);
}

const wchar_t *kill_yank_rotate() {
    ASSERT_IS_MAIN_THREAD();
    // Move the first element to the end.
    if (kill_list.empty()) {
        return NULL;
    }
    kill_list.splice(kill_list.end(), kill_list, kill_list.begin());
    return kill_list.front().c_str();
}

const wchar_t *kill_yank() {
    if (kill_list.empty()) {
        return L"";
    }
    return kill_list.front().c_str();
}

void kill_sanity_check() {}

void kill_init() {}

void kill_destroy() {}
