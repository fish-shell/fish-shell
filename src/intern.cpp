// Library for pooling common strings.
#include "config.h"  // IWYU pragma: keep

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>
#include <algorithm>
#include <memory>
#include <vector>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "intern.h"

/// Comparison function for intern'd strings.
class string_table_compare_t {
   public:
    bool operator()(const wchar_t *a, const wchar_t *b) const { return wcscmp(a, b) < 0; }
};

// A sorted vector ends up being a little more memory efficient than a std::set for the intern'd
// string table.
#define USE_SET 0
#if USE_SET
/// The table of intern'd strings.
typedef std::set<const wchar_t *, string_table_compare_t> string_table_t;
#else
/// The table of intern'd strings.
typedef std::vector<const wchar_t *> string_table_t;
#endif

static string_table_t string_table;

/// The lock to provide thread safety for intern'd strings.
static pthread_mutex_t intern_lock = PTHREAD_MUTEX_INITIALIZER;

static const wchar_t *intern_with_dup(const wchar_t *in, bool dup) {
    if (!in) return NULL;

    // debug( 0, L"intern %ls", in );
    scoped_lock lock(intern_lock);
    const wchar_t *result;

#if USE_SET
    string_table_t::const_iterator iter = string_table.find(in);
    if (iter != string_table.end()) {
        result = *iter;
    } else {
        result = dup ? wcsdup(in) : in;
        string_table.insert(result);
    }
#else
    string_table_t::iterator iter =
        std::lower_bound(string_table.begin(), string_table.end(), in, string_table_compare_t());
    if (iter != string_table.end() && wcscmp(*iter, in) == 0) {
        result = *iter;
    } else {
        result = dup ? wcsdup(in) : in;
        string_table.insert(iter, result);
    }
#endif
    return result;
}

const wchar_t *intern(const wchar_t *in) { return intern_with_dup(in, true); }

const wchar_t *intern_static(const wchar_t *in) { return intern_with_dup(in, false); }
