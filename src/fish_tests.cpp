// Various bug and feature tests. Compiled and run by make test.
#include "config.h"  // IWYU pragma: keep

#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <functional>
#include <future>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fds.rs.h"
#include "parse_constants.rs.h"

#ifdef FISH_CI_SAN
#include <sanitizer/lsan_interface.h>
#endif

#include "abbrs.h"
#include "ast.h"
#include "color.h"
#include "common.h"
#include "complete.h"
#include "cxxgen.h"
#include "enum_set.h"
#include "env.h"
#include "env/env_ffi.rs.h"
#include "env_universal_common.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fd_monitor.rs.h"
#include "fd_readable_set.rs.h"
#include "fds.h"
#include "ffi_baggage.h"
#include "ffi_init.rs.h"
#include "ffi_tests.rs.h"
#include "function.h"
#include "future_feature_flags.h"
#include "global_safety.h"
#include "highlight.h"
#include "history.h"
#include "input_ffi.rs.h"
#include "io.h"
#include "iothread.h"
#include "kill.rs.h"
#include "lru.h"
#include "maybe.h"
#include "operation_context.h"
#include "pager.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "redirection.h"
#include "screen.h"
#include "signals.h"
#include "smoke.rs.h"
#include "termsize.h"
#include "threads.rs.h"
#include "tokenizer.h"
#include "util.h"
#include "wcstringutil.h"
#include "wgetopt.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

static const char *const *s_arguments;
static int s_test_run_count = 0;

// Indicate if we should test the given function. Either we test everything (all arguments) or we
// run only tests that have a prefix in s_arguments.
// If \p default_on is set, then allow no args to run this test by default.
static bool should_test_function(const char *func_name, bool default_on = true) {
    bool result = false;
    if (!s_arguments || !s_arguments[0]) {
        // No args, test if defaulted on.
        result = default_on;
    } else {
        for (size_t i = 0; s_arguments[i] != nullptr; i++) {
            if (!std::strcmp(func_name, s_arguments[i])) {
                result = true;
                break;
            }
        }
    }
    if (result) s_test_run_count++;
    return result;
}

/// The number of tests to run.
#define ESCAPE_TEST_COUNT 100000
/// The average length of strings to unescape.
#define ESCAPE_TEST_LENGTH 100

/// Number of encountered errors.
static int err_count = 0;

/// Print formatted output.
static void say(const wchar_t *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    std::vfwprintf(stdout, fmt, va);
    va_end(va);
    std::fwprintf(stdout, L"\n");
}

/// Print formatted error string.
static void err(const wchar_t *blah, ...) {
    va_list va;
    va_start(va, blah);
    err_count++;

    // Show errors in red.
    std::fputws(L"\x1B[31m", stdout);
    std::fwprintf(stdout, L"Error: ");
    std::vfwprintf(stdout, blah, va);
    va_end(va);

    // Return to normal color.
    std::fputws(L"\x1B[0m", stdout);
    std::fwprintf(stdout, L"\n");
}

static std::vector<std::string> pushed_dirs;

// Helper to return a string whose length greatly exceeds PATH_MAX.
wcstring get_overlong_path() {
    wcstring longpath;
    longpath.reserve(PATH_MAX * 2 + 10);
    while (longpath.size() <= PATH_MAX * 2) {
        longpath += L"/overlong";
    }
    return longpath;
}

// The odd formulation of these macros is to avoid "multiple unary operator" warnings from oclint
// were we to use the more natural "if (!(e)) err(..." form. We have to do this because the rules
// for the C preprocessor make it practically impossible to embed a comment in the body of a macro.
#define do_test(e)                                             \
    do {                                                       \
        if (e) {                                               \
            ;                                                  \
        } else {                                               \
            err(L"Test failed on line %lu: %s", __LINE__, #e); \
        }                                                      \
    } while (0)

#define do_test1(e, msg)                                           \
    do {                                                           \
        if (e) {                                                   \
            ;                                                      \
        } else {                                                   \
            err(L"Test failed on line %lu: %ls", __LINE__, (msg)); \
        }                                                          \
    } while (0)

// todo!("already ported, delete this");
/// Test that the fish functions for converting strings to numbers work.
static void test_str_to_num() {
    say(L"Testing str_to_num");
    const wchar_t *end;
    int i;
    long l;

    i = fish_wcstoi(L"");
    do_test1(errno == EINVAL && i == 0, L"converting empty string to int did not fail");
    i = fish_wcstoi(L" \n ");
    do_test1(errno == EINVAL && i == 0, L"converting whitespace string to int did not fail");
    i = fish_wcstoi(L"123");
    do_test1(errno == 0 && i == 123, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L"-123");
    do_test1(errno == 0 && i == -123, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L" 345  ");
    do_test1(errno == 0 && i == 345, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L" -345  ");
    do_test1(errno == 0 && i == -345, L"converting valid num to int did not succeed");
    i = fish_wcstoi(L"x345");
    do_test1(errno == EINVAL && i == 0, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L" x345");
    do_test1(errno == EINVAL && i == 0, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"456 x");
    do_test1(errno == -1 && i == 456, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"99999999999999999999999");
    do_test1(errno == ERANGE && i == INT_MAX, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"-99999999999999999999999");
    do_test1(errno == ERANGE && i == INT_MIN, L"converting invalid num to int did not fail");
    i = fish_wcstoi(L"567]", &end);
    do_test1(errno == -1 && i == 567 && *end == L']',
             L"converting valid num to int did not succeed");
    // This is subtle. "567" in base 8 is "375" in base 10. The final "8" is not converted.
    i = fish_wcstoi(L"5678", &end, 8);
    do_test1(errno == -1 && i == 375 && *end == L'8',
             L"converting invalid num to int did not fail");

    l = fish_wcstol(L"");
    do_test1(errno == EINVAL && l == 0, L"converting empty string to long did not fail");
    l = fish_wcstol(L" \t ");
    do_test1(errno == EINVAL && l == 0, L"converting whitespace string to long did not fail");
    l = fish_wcstol(L"123");
    do_test1(errno == 0 && l == 123, L"converting valid num to long did not succeed");
    l = fish_wcstol(L"-123");
    do_test1(errno == 0 && l == -123, L"converting valid num to long did not succeed");
    l = fish_wcstol(L" 345  ");
    do_test1(errno == 0 && l == 345, L"converting valid num to long did not succeed");
    l = fish_wcstol(L" -345  ");
    do_test1(errno == 0 && l == -345, L"converting valid num to long did not succeed");
    l = fish_wcstol(L"x345");
    do_test1(errno == EINVAL && l == 0, L"converting invalid num to long did not fail");
    l = fish_wcstol(L" x345");
    do_test1(errno == EINVAL && l == 0, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"456 x");
    do_test1(errno == -1 && l == 456, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"99999999999999999999999");
    do_test1(errno == ERANGE && l == LONG_MAX, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"-99999999999999999999999");
    do_test1(errno == ERANGE && l == LONG_MIN, L"converting invalid num to long did not fail");
    l = fish_wcstol(L"567]", &end);
    do_test1(errno == -1 && l == 567 && *end == L']',
             L"converting valid num to long did not succeed");
    // This is subtle. "567" in base 8 is "375" in base 10. The final "8" is not converted.
    l = fish_wcstol(L"5678", &end, 8);
    do_test1(errno == -1 && l == 375 && *end == L'8',
             L"converting invalid num to long did not fail");
}

enum class test_enum { alpha, beta, gamma, COUNT };

template <>
struct enum_info_t<test_enum> {
    static constexpr auto count = test_enum::COUNT;
};

// todo!("no need to port, delete this");
static void test_enum_set() {
    say(L"Testing enum set");
    enum_set_t<test_enum> es;
    do_test(es.none());
    do_test(!es.any());
    do_test(es.to_raw() == 0);
    do_test(es == enum_set_t<test_enum>::from_raw(0));
    do_test(es != enum_set_t<test_enum>::from_raw(1));

    es.set(test_enum::beta);
    do_test(es.get(test_enum::beta));
    do_test(!es.get(test_enum::alpha));
    do_test(es & test_enum::beta);
    do_test(!(es & test_enum::alpha));
    do_test(es.to_raw() == 2);
    do_test(es == enum_set_t<test_enum>::from_raw(2));
    do_test(es == enum_set_t<test_enum>{test_enum::beta});
    do_test(es != enum_set_t<test_enum>::from_raw(3));
    do_test(es.any());
    do_test(!es.none());

    do_test((enum_set_t<test_enum>{test_enum::beta} | test_enum::alpha).to_raw() == 3);
    do_test((enum_set_t<test_enum>{test_enum::beta} | enum_set_t<test_enum>{test_enum::alpha})
                .to_raw() == 3);
}

// todo!("no need to port, delete this");
static void test_enum_array() {
    say(L"Testing enum array");
    enum_array_t<std::string, test_enum> es{};
    do_test(es.size() == enum_count<test_enum>());
    es[test_enum::beta] = "abc";
    do_test(es[test_enum::beta] == "abc");
    es.at(test_enum::gamma) = "def";
    do_test(es.at(test_enum::gamma) == "def");
}

/// Helper to convert a narrow string to a sequence of hex digits.
static std::string str2hex(const std::string &input) {
    std::string output;
    for (char c : input) {
        char buff[16];
        size_t amt = snprintf(buff, sizeof buff, "0x%02X ", (int)(c & 0xFF));
        output.append(buff, amt);
    }
    return output;
}

// todo!("already ported, delete this");
/// Test wide/narrow conversion by creating random strings and verifying that the original string
/// comes back through double conversion.
static void test_convert() {
    say(L"Testing wide/narrow string conversion");
    for (int i = 0; i < ESCAPE_TEST_COUNT; i++) {
        std::string orig{};
        while (random() % ESCAPE_TEST_LENGTH) {
            char c = random();
            orig.push_back(c);
        }

        const wcstring w = str2wcstring(orig);
        std::string n = wcs2string(w);
        if (orig != n) {
            err(L"Line %d - %d: Conversion cycle of string:\n%4d chars: %s\n"
                L"produced different string:\n%4d chars: %s",
                __LINE__, i, orig.size(), str2hex(orig).c_str(), n.size(), str2hex(n).c_str());
        }
    }
}

// todo!("already ported, delete this");
/// Verify that ASCII narrow->wide conversions are correct.
static void test_convert_ascii() {
    std::string s(4096, '\0');
    for (size_t i = 0; i < s.size(); i++) {
        s[i] = (i % 10) + '0';
    }

    // Test a variety of alignments.
    for (size_t left = 0; left < 16; left++) {
        for (size_t right = 0; right < 16; right++) {
            const char *start = s.data() + left;
            size_t len = s.size() - left - right;
            wcstring wide = str2wcstring(start, len);
            std::string narrow = wcs2string(wide);
            do_test(narrow == std::string(start, len));
        }
    }

    // Put some non-ASCII bytes in and ensure it all still works.
    for (char &c : s) {
        char saved = c;
        c = 0xF7;
        do_test(wcs2string(str2wcstring(s)) == s);
        c = saved;
    }
}

// todo!("already ported, delete this");
/// fish uses the private-use range to encode bytes that could not be decoded using the user's
/// locale. If the input could be decoded, but decoded to private-use codepoints, then fish should
/// also use the direct encoding for those bytes. Verify that characters in the private use area are
/// correctly round-tripped. See #7723.
static void test_convert_private_use() {
    for (wchar_t wc = ENCODE_DIRECT_BASE; wc < ENCODE_DIRECT_END; wc++) {
        // Encode the char via the locale. Do not use fish functions which interpret these
        // specially.
        char converted[MB_LEN_MAX];
        mbstate_t state{};
        size_t len = std::wcrtomb(converted, wc, &state);
        if (len == static_cast<size_t>(-1)) {
            // Could not be encoded in this locale.
            continue;
        }
        std::string s(converted, len);

        // Ask fish to decode this via str2wcstring.
        // str2wcstring should notice that the decoded form collides with its private use and encode
        // it directly.
        wcstring ws = str2wcstring(s);

        // Each byte should be encoded directly, and round tripping should work.
        do_test(ws.size() == s.size());
        do_test(wcs2string(ws) == s);
    }
}

// todo!("already ported, delete this");
static void test_iothread() {
    say(L"Testing iothreads");
    std::atomic<int> shared_int{0};
    const int iterations = 64;
    std::promise<void> prom;
    for (int i = 0; i < iterations; i++) {
        iothread_perform([&] {
            int newv = 1 + shared_int.fetch_add(1, std::memory_order_relaxed);
            if (newv == iterations) {
                prom.set_value();
            }
        });
    }
    auto status = prom.get_future().wait_for(std::chrono::seconds(64));

    // Should have incremented it once per thread.
    do_test(status == std::future_status::ready);
    do_test(shared_int == iterations);
    if (shared_int != iterations) {
        say(L"Expected int to be %d, but instead it was %d", iterations, shared_int.load());
    }
}

static void test_const_strlen() {
    do_test(const_strlen("") == 0);
    do_test(const_strlen(L"") == 0);
    do_test(const_strlen("\0") == 0);
    do_test(const_strlen(L"\0") == 0);
    do_test(const_strlen("\0abc") == 0);
    do_test(const_strlen(L"\0abc") == 0);
    do_test(const_strlen("x") == 1);
    do_test(const_strlen(L"x") == 1);
    do_test(const_strlen("abc") == 3);
    do_test(const_strlen(L"abc") == 3);
    do_test(const_strlen("abc\0def") == 3);
    do_test(const_strlen(L"abc\0def") == 3);
    do_test(const_strlen("abcdef\0") == 6);
    do_test(const_strlen(L"abcdef\0") == 6);
    static_assert(const_strlen("hello") == 5, "const_strlen failure");
}

static void test_const_strcmp() {
    static_assert(const_strcmp("", "a") < 0, "const_strcmp failure");
    static_assert(const_strcmp("a", "a") == 0, "const_strcmp failure");
    static_assert(const_strcmp("a", "") > 0, "const_strcmp failure");
    static_assert(const_strcmp("aa", "a") > 0, "const_strcmp failure");
    static_assert(const_strcmp("a", "aa") < 0, "const_strcmp failure");
    static_assert(const_strcmp("b", "aa") > 0, "const_strcmp failure");
}

// todo!("already ported, delete this");
void test_dir_iter() {
    dir_iter_t baditer(L"/definitely/not/a/valid/directory/for/sure");
    do_test(!baditer.valid());
    do_test(baditer.error() == ENOENT || baditer.error() == EACCES);
    do_test(baditer.next() == nullptr);

    char t1[] = "/tmp/fish_test_dir_iter.XXXXXX";
    const std::string basepathn = mkdtemp(t1);
    const wcstring basepath = str2wcstring(basepathn);
    auto makepath = [&](const wcstring &s) { return wcs2zstring(basepath + L"/" + s); };

    const wcstring dirname = L"dir";
    const wcstring regname = L"reg";
    const wcstring reglinkname = L"reglink";    // link to regular file
    const wcstring dirlinkname = L"dirlink";    // link to directory
    const wcstring badlinkname = L"badlink";    // link to nowhere
    const wcstring selflinkname = L"selflink";  // link to self
    const wcstring fifoname = L"fifo";
    const std::vector<wcstring> names = {dirname,     regname,      reglinkname, dirlinkname,
                                         badlinkname, selflinkname, fifoname};

    const auto is_link_name = [&](const wcstring &name) -> bool {
        return contains({reglinkname, dirlinkname, badlinkname, selflinkname}, name);
    };

    // Make our different file types
    int ret = mkdir(makepath(dirname).c_str(), 0700);
    do_test(ret == 0);
    ret = open(makepath(regname).c_str(), O_CREAT | O_WRONLY, 0600);
    do_test(ret >= 0);
    close(ret);
    ret = symlink(makepath(regname).c_str(), makepath(reglinkname).c_str());
    do_test(ret == 0);
    ret = symlink(makepath(dirname).c_str(), makepath(dirlinkname).c_str());
    do_test(ret == 0);
    ret = symlink("/this/is/an/invalid/path", makepath(badlinkname).c_str());
    do_test(ret == 0);
    ret = symlink(makepath(selflinkname).c_str(), makepath(selflinkname).c_str());
    do_test(ret == 0);
    ret = mkfifo(makepath(fifoname).c_str(), 0600);
    do_test(ret == 0);

    dir_iter_t iter1(basepath);
    do_test(iter1.valid());
    do_test(iter1.error() == 0);
    size_t seen = 0;
    while (const auto *entry = iter1.next()) {
        seen += 1;
        do_test(entry->name != L"." && entry->name != L"..");
        do_test(contains(names, entry->name));
        maybe_t<dir_entry_type_t> expected{};
        if (entry->name == dirname) {
            expected = dir_entry_type_t::dir;
        } else if (entry->name == regname) {
            expected = dir_entry_type_t::reg;
        } else if (entry->name == reglinkname) {
            expected = dir_entry_type_t::reg;
        } else if (entry->name == dirlinkname) {
            expected = dir_entry_type_t::dir;
        } else if (entry->name == badlinkname) {
            expected = none();
        } else if (entry->name == selflinkname) {
            expected = dir_entry_type_t::lnk;
        } else if (entry->name == fifoname) {
            expected = dir_entry_type_t::fifo;
        } else {
            err(L"Unexpected file type");
            continue;
        }
        // Links should never have a fast type if we are resolving them, since we cannot resolve a
        // symlink from readdir.
        if (is_link_name(entry->name)) {
            do_test(entry->fast_type() == none());
        }
        // If we have a fast type, it should be correct.
        do_test(entry->fast_type() == none() || entry->fast_type() == expected);
        do_test(entry->check_type() == expected);
    }
    do_test(seen == names.size());

    // Clean up.
    for (const auto &name : names) {
        (void)unlink(makepath(name).c_str());
    }
    (void)rmdir(basepathn.c_str());
}

static void test_utility_functions() {
    say(L"Testing utility functions");
    test_const_strlen();
    test_const_strcmp();
}

class test_lru_t : public lru_cache_t<int> {
   public:
    static constexpr size_t test_capacity = 16;
    using value_type = std::pair<wcstring, int>;

    test_lru_t() : lru_cache_t<int>(test_capacity) {}

    std::vector<value_type> values() const {
        std::vector<value_type> result;
        for (auto p : *this) {
            result.push_back(p);
        }
        return result;
    }

    std::vector<int> ints() const {
        std::vector<int> result;
        for (auto p : *this) {
            result.push_back(p.second);
        }
        return result;
    }
};

// todo!("no need to port this, delete this");
static void test_lru() {
    say(L"Testing LRU cache");

    test_lru_t cache;
    std::vector<std::pair<wcstring, int>> expected_evicted;
    std::vector<std::pair<wcstring, int>> expected_values;
    int total_nodes = 20;
    for (int i = 0; i < total_nodes; i++) {
        do_test(cache.size() == size_t(std::min(i, 16)));
        do_test(cache.values() == expected_values);
        if (i < 4) expected_evicted.emplace_back(to_string(i), i);
        // Adding the node the first time should work, and subsequent times should fail.
        do_test(cache.insert(to_string(i), i));
        do_test(!cache.insert(to_string(i), i + 1));

        expected_values.emplace_back(to_string(i), i);
        while (expected_values.size() > test_lru_t::test_capacity) {
            expected_values.erase(expected_values.begin());
        }
        cache.check_sanity();
    }
    do_test(cache.values() == expected_values);
    cache.check_sanity();

    // Stable-sort ints in reverse order
    // This a/2 check ensures that some different ints compare the same
    // It also gives us a different order than we started with
    auto comparer = [](int a, int b) { return a / 2 > b / 2; };
    std::vector<int> ints = cache.ints();
    std::stable_sort(ints.begin(), ints.end(), comparer);

    cache.stable_sort(comparer);
    std::vector<int> new_ints = cache.ints();
    if (new_ints != ints) {
        auto commajoin = [](const std::vector<int> &vs) {
            wcstring ret;
            for (int v : vs) {
                append_format(ret, L"%d,", v);
            }
            if (!ret.empty()) ret.pop_back();
            return ret;
        };
        err(L"LRU stable sort failed. Expected %ls, got %ls\n", commajoin(new_ints).c_str(),
            commajoin(ints).c_str());
    }

    cache.evict_all_nodes();
    do_test(cache.size() == 0);
}

// todo!("already ported, delete this")
/// Testing colors.
static void test_colors() {
    say(L"Testing colors");
    do_test(rgb_color_t(L"#FF00A0").is_rgb());
    do_test(rgb_color_t(L"FF00A0").is_rgb());
    do_test(rgb_color_t(L"#F30").is_rgb());
    do_test(rgb_color_t(L"F30").is_rgb());
    do_test(rgb_color_t(L"f30").is_rgb());
    do_test(rgb_color_t(L"#FF30a5").is_rgb());
    do_test(rgb_color_t(L"3f30").is_none());
    do_test(rgb_color_t(L"##f30").is_none());
    do_test(rgb_color_t(L"magenta").is_named());
    do_test(rgb_color_t(L"MaGeNTa").is_named());
    do_test(rgb_color_t(L"mooganta").is_none());
}

// todo!("port this")
/// Helper for test_timezone_env_vars().
long return_timezone_hour(time_t tstamp, const wchar_t *timezone) {
    env_stack_t vars{parser_principal_parser()->deref().vars_boxed()};
    struct tm ltime;
    char ltime_str[3];
    char *str_ptr;
    size_t n;

    vars.set_one(L"TZ", ENV_EXPORT, timezone);

    const auto var = vars.get(L"TZ", ENV_DEFAULT);
    (void)var;

    localtime_r(&tstamp, &ltime);
    n = strftime(ltime_str, 3, "%H", &ltime);
    if (n != 2) {
        err(L"strftime() returned %d, expected 2", n);
        return 0;
    }
    return strtol(ltime_str, &str_ptr, 10);
}

// todo!("no need to port, delete this")
void test_maybe() {
    say(L"Testing maybe_t");
    // maybe_t<T> bool conversion is only enabled for non-bool-convertible T types
    do_test(!bool(maybe_t<wcstring>()));
    maybe_t<int> m(3);
    do_test(m.has_value());
    do_test(m.value() == 3);
    m.reset();
    do_test(!m.has_value());
    m = 123;
    do_test(m.has_value());
    do_test(m.has_value() && m.value() == 123);
    m = maybe_t<int>();
    do_test(!m.has_value());
    m = maybe_t<int>(64);
    do_test(m.has_value() && m.value() == 64);
    m = 123;
    do_test(m == maybe_t<int>(123));
    do_test(m != maybe_t<int>());
    do_test(maybe_t<int>() == none());
    do_test(!maybe_t<int>(none()).has_value());
    maybe_t<wcstring> n = none();
    do_test(!bool(n));

    maybe_t<std::string> m0 = none();
    maybe_t<std::string> m3("hi");
    maybe_t<std::string> m4 = m3;
    do_test(m4 && *m4 == "hi");
    maybe_t<std::string> m5 = m0;
    do_test(!m5);

    maybe_t<std::string> acquire_test("def");
    do_test(acquire_test);
    std::string res = acquire_test.acquire();
    do_test(!acquire_test);
    do_test(res == "def");

    // maybe_t<T> should be copyable iff T is copyable.
    using copyable = std::shared_ptr<int>;
    using noncopyable = std::unique_ptr<int>;
    do_test(std::is_copy_assignable<maybe_t<copyable>>::value == true);
    do_test(std::is_copy_constructible<maybe_t<copyable>>::value == true);
    do_test(std::is_copy_assignable<maybe_t<noncopyable>>::value == false);
    do_test(std::is_copy_constructible<maybe_t<noncopyable>>::value == false);

    // We can construct a maybe_t from noncopyable things.
    maybe_t<noncopyable> nmt{make_unique<int>(42)};
    do_test(**nmt == 42);

    maybe_t<std::string> c1{"abc"};
    maybe_t<std::string> c2 = c1;
    do_test(c1.value() == "abc");
    do_test(c2.value() == "abc");
    c2 = c1;
    do_test(c1.value() == "abc");
    do_test(c2.value() == "abc");

    do_test(c2.value_or("derp") == "abc");
    do_test(c2.value_or("derp") == "abc");
    c2.reset();
    do_test(c2.value_or("derp") == "derp");
}

// todo!("already ported, delete this")
void test_normalize_path() {
    say(L"Testing path normalization");
    do_test(normalize_path(L"") == L".");
    do_test(normalize_path(L"..") == L"..");
    do_test(normalize_path(L"./") == L".");
    do_test(normalize_path(L"./.") == L".");
    do_test(normalize_path(L"/") == L"/");
    do_test(normalize_path(L"//") == L"//");
    do_test(normalize_path(L"///") == L"/");
    do_test(normalize_path(L"////") == L"/");
    do_test(normalize_path(L"/.///") == L"/");
    do_test(normalize_path(L".//") == L".");
    do_test(normalize_path(L"/.//../") == L"/");
    do_test(normalize_path(L"////abc") == L"/abc");
    do_test(normalize_path(L"/abc") == L"/abc");
    do_test(normalize_path(L"/abc/") == L"/abc");
    do_test(normalize_path(L"/abc/..def/") == L"/abc/..def");
    do_test(normalize_path(L"//abc/../def/") == L"//def");
    do_test(normalize_path(L"abc/../abc/../abc/../abc") == L"abc");
    do_test(normalize_path(L"../../") == L"../..");
    do_test(normalize_path(L"foo/./bar") == L"foo/bar");
    do_test(normalize_path(L"foo/../") == L".");
    do_test(normalize_path(L"foo/../foo") == L"foo");
    do_test(normalize_path(L"foo/../foo/") == L"foo");
    do_test(normalize_path(L"foo/././bar/.././baz") == L"foo/baz");

    do_test(path_normalize_for_cd(L"/", L"..") == L"/..");
    do_test(path_normalize_for_cd(L"/abc/", L"..") == L"/");
    do_test(path_normalize_for_cd(L"/abc/def/", L"..") == L"/abc");
    do_test(path_normalize_for_cd(L"/abc/def/", L"../..") == L"/");
    do_test(path_normalize_for_cd(L"/abc///def/", L"../..") == L"/");
    do_test(path_normalize_for_cd(L"/abc///def/", L"../..") == L"/");
    do_test(path_normalize_for_cd(L"/abc///def///", L"../..") == L"/");
    do_test(path_normalize_for_cd(L"/abc///def///", L"..") == L"/abc");
    do_test(path_normalize_for_cd(L"/abc///def///", L"..") == L"/abc");
    do_test(path_normalize_for_cd(L"/abc/def/", L"./././/..") == L"/abc");
    do_test(path_normalize_for_cd(L"/abc/def/", L"../../../") == L"/../");
    do_test(path_normalize_for_cd(L"/abc/def/", L"../ghi/..") == L"/abc/ghi/..");
}

// todo!("already ported, delete this")
void test_dirname_basename() {
    say(L"Testing wdirname and wbasename");
    const struct testcase_t {
        const wchar_t *path;
        const wchar_t *dir;
        const wchar_t *base;
    } testcases[] = {
        {L"", L".", L"."},
        {L"foo//", L".", L"foo"},
        {L"foo//////", L".", L"foo"},
        {L"/////foo", L"/", L"foo"},
        {L"/////foo", L"/", L"foo"},
        {L"//foo/////bar", L"//foo", L"bar"},
        {L"foo/////bar", L"foo", L"bar"},

        // Examples given in XPG4.2.
        {L"/usr/lib", L"/usr", L"lib"},
        {L"usr", L".", L"usr"},
        {L"/", L"/", L"/"},
        {L".", L".", L"."},
        {L"..", L".", L".."},
    };
    for (const auto &tc : testcases) {
        wcstring dir = wdirname(tc.path);
        if (dir != tc.dir) {
            err(L"Wrong dirname for \"%ls\": expected \"%ls\", got \"%ls\"", tc.path, tc.dir,
                dir.c_str());
        }
        wcstring base = wbasename(tc.path);
        if (base != tc.base) {
            err(L"Wrong basename for \"%ls\": expected \"%ls\", got \"%ls\"", tc.path, tc.base,
                base.c_str());
        }
    }
    // Ensures strings which greatly exceed PATH_MAX still work (#7837).
    wcstring longpath = get_overlong_path();
    wcstring longpath_dir = longpath.substr(0, longpath.rfind(L'/'));
    do_test(wdirname(longpath) == longpath_dir);
    do_test(wbasename(longpath) == L"overlong");
}

void test_rust_smoke() {
    size_t x = rust::add(37, 5);
    do_test(x == 42);
}

void test_rust_ffi() { rust::run_ffi_tests(); }

// typedef void (test_entry_point_t)();
using test_entry_point_t = void (*)();
struct test_t {
    const char *group;
    std::function<void()> test;
    bool opt_in = false;

    test_t(const char *group, test_entry_point_t test, bool opt_in = false)
        : group(group), test(test), opt_in(opt_in) {}
};

struct test_comparator_t {
    // template<typename T=test_t>
    int operator()(const test_t &lhs, const test_t &rhs) { return strcmp(lhs.group, rhs.group); }
};

// This magic string is required for CMake to pick up the list of tests
#define TEST_GROUP(x) x
static const test_t s_tests[]{
    {TEST_GROUP("utility_functions"), test_utility_functions},
    {TEST_GROUP("dir_iter"), test_dir_iter},
    {TEST_GROUP("str_to_num"), test_str_to_num},
    {TEST_GROUP("enum"), test_enum_set},
    {TEST_GROUP("enum"), test_enum_array},
    {TEST_GROUP("convert"), test_convert},
    {TEST_GROUP("convert"), test_convert_private_use},
    {TEST_GROUP("convert_ascii"), test_convert_ascii},
    {TEST_GROUP("iothread"), test_iothread},
    {TEST_GROUP("lru"), test_lru},
    {TEST_GROUP("colors"), test_colors},
    {TEST_GROUP("maybe"), test_maybe},
    {TEST_GROUP("normalize"), test_normalize_path},
    {TEST_GROUP("dirname"), test_dirname_basename},
    {TEST_GROUP("rust_smoke"), test_rust_smoke},
    {TEST_GROUP("rust_ffi"), test_rust_ffi},
};

void list_tests() {
    std::set<std::string> groups;
    for (const auto &test : s_tests) {
        groups.insert(test.group);
    }

    for (const auto &group : groups) {
        std::fprintf(stdout, "%s\n", group.c_str());
    }
}

/// Main test.
int main(int argc, char **argv) {
    std::setlocale(LC_ALL, "");

    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        list_tests();
        return 0;
    }

    // Look for the file tests/test.fish. We expect to run in a directory containing that file.
    // If we don't find it, walk up the directory hierarchy until we do, or error.
    while (access("./tests/test.fish", F_OK) != 0) {
        char wd[PATH_MAX + 1] = {};
        if (!getcwd(wd, sizeof wd)) {
            perror("getcwd");
            exit(-1);
        }
        if (!std::strcmp(wd, "/")) {
            std::fwprintf(
                stderr, L"Unable to find 'tests' directory, which should contain file test.fish\n");
            exit(EXIT_FAILURE);
        }
        if (chdir(dirname(wd)) < 0) {
            perror("chdir");
        }
    }

    srandom((unsigned int)time(nullptr));
    configure_thread_assertions_for_testing();

    // Set the program name to this sentinel value
    // This will prevent some misleading stderr output during the tests
    program_name = TESTS_PROGRAM_NAME;
    s_arguments = argv + 1;

    struct utsname uname_info;
    uname(&uname_info);

    say(L"Testing low-level functionality");
    rust_init();
    proc_init();
    rust_env_init(true);
    misc_init();
    reader_init();

    // Set default signal handlers, so we can ctrl-C out of this.
    signal_reset_handlers();

    // Set PWD from getcwd - fixes #5599
    env_stack_principal().set_pwd_from_getcwd();

    for (const auto &test : s_tests) {
        if (should_test_function(test.group)) {
            test.test();
        }
    }

    say(L"Encountered %d errors in low-level tests", err_count);
    if (s_test_run_count == 0) say(L"*** No Tests Were Actually Run! ***");

    // If under ASAN, reclaim some resources before exiting.
    asan_before_exit();

    if (err_count != 0) {
        return 1;
    }
}
