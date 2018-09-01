// Library for pooling common strings.
#include "config.h"  // IWYU pragma: keep

#include <stddef.h>
#include <wchar.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "intern.h"

bool string_less_than_string(const wchar_t *a, const wchar_t *b) { return wcscmp(a, b) < 0; }

/// The table of intern'd strings.
owning_lock<std::vector<const wchar_t *>> string_table;

static const wchar_t *intern_with_dup(const wchar_t *in, bool dup) {
    if (!in) return NULL;

    debug(5, L"intern %ls", in);
    auto table = string_table.acquire();

    const wchar_t *result;
    auto iter = std::lower_bound(table->begin(), table->end(), in, string_less_than_string);
    if (iter != table->end() && wcscmp(*iter, in) == 0) {
        result = *iter;
    } else {
        result = dup ? wcsdup(in) : in;
        table->insert(iter, result);
    }
    return result;
}

const wchar_t *intern(const wchar_t *in) { return intern_with_dup(in, true); }

const wchar_t *intern_static(const wchar_t *in) { return intern_with_dup(in, false); }
