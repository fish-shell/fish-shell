/// ghoti logging
#include "config.h"  // IWYU pragma: keep

#include "flog.h"

#include <stdarg.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <memory>
#include <vector>

#include "common.h"
#include "global_safety.h"
#include "parse_util.h"
#include "wcstringutil.h"
#include "wildcard.h"

namespace flog_details {

// Note we are relying on the order of global initialization within this file.
// It is important that 'all' be initialized before 'g_categories', because g_categories wants to
// append to all in the ctor.
/// This is not modified after initialization.
static std::vector<category_t *> s_all_categories;

/// The fd underlying the flog output file.
/// This is separated out for flogf_async_safe.
static int s_flog_file_fd{STDERR_FILENO};

/// When a category is instantiated it adds itself to the 'all' list.
category_t::category_t(const wchar_t *name, const wchar_t *desc, bool enabled)
    : name(name), description(desc), enabled(enabled) {
    s_all_categories.push_back(this);
}

/// Instantiate all categories.
/// This is deliberately leaked to avoid pointless destructor registration.
category_list_t *const category_list_t::g_instance = new category_list_t();

logger_t::logger_t() : file_(stderr) {}

owning_lock<logger_t> g_logger;

void logger_t::log1(const wchar_t *s) { std::fputws(s, file_); }

void logger_t::log1(const char *s) {
    // Note glibc prohibits mixing narrow and wide I/O, so always use wide-printing functions.
    // See #5900.
    std::fwprintf(file_, L"%s", s);
}

void logger_t::log1(wchar_t c) { std::fputwc(c, file_); }

void logger_t::log1(char c) { std::fwprintf(file_, L"%c", c); }

void logger_t::log1(int64_t v) { std::fwprintf(file_, L"%lld", v); }

void logger_t::log1(uint64_t v) { std::fwprintf(file_, L"%llu", v); }

void logger_t::log_fmt(const category_t &cat, const wchar_t *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    log1(cat.name);
    log1(L": ");
    std::vfwprintf(file_, fmt, va);
    log1(L'\n');
    va_end(va);
}

void logger_t::log_fmt(const category_t &cat, const char *fmt, ...) {
    // glibc dislikes mixing wide and narrow output functions.
    // So construct a narrow string in-place and output that via wide functions.
    va_list va;
    va_start(va, fmt);
    int ret = vsnprintf(nullptr, 0, fmt, va);
    va_end(va);

    if (ret < 0) {
        perror("vsnprintf");
        return;
    }
    size_t len = static_cast<size_t>(ret) + 1;
    std::unique_ptr<char[]> buff(new char[len]);

    va_start(va, fmt);
    ret = vsnprintf(buff.get(), len, fmt, va);
    va_end(va);
    if (ret < 0) {
        perror("vsnprintf");
        return;
    }
    log_fmt(cat, L"%s", buff.get());
}

inline void async_safe_write_str(const char *s, size_t len = std::string::npos) {
    if (s_flog_file_fd < 0) return;
    if (len == std::string::npos) len = std::strlen(s);
    ignore_result(write(s_flog_file_fd, s, len));
}

// static
void logger_t::flogf_async_safe(const char *category, const char *fmt, const char *param1,
                                const char *param2, const char *param3, const char *param4,
                                const char *param5, const char *param6, const char *param7,
                                const char *param8, const char *param9, const char *param10,
                                const char *param11, const char *param12) {
    const char *const params[] = {param1, param2, param3, param4,  param5,  param6,
                                  param7, param8, param9, param10, param11, param12};
    const size_t max_params = sizeof params / sizeof *params;

    // Can't call fwprintf, that may allocate memory.
    // Just call write() over and over.
    async_safe_write_str(category);
    async_safe_write_str(": ");

    size_t param_idx = 0;
    const char *cursor = fmt;
    while (*cursor != '\0') {
        const char *end = std::strchr(cursor, '%');
        if (end == nullptr) end = cursor + std::strlen(cursor);
        if (end > cursor) async_safe_write_str(cursor, end - cursor);
        if (end[0] == '%' && end[1] == 's') {
            // Handle a format string.
            const char *param = (param_idx < max_params ? params[param_idx++] : nullptr);
            if (!param) param = "(null)";
            async_safe_write_str(param);
            cursor = end + 2;
        } else if (end[0] == '\0') {
            // Must be at the end of the string.
            cursor = end;
        } else {
            // Some other format specifier, just skip it.
            cursor = end + 1;
        }
    }

    // We always append a newline.
    async_safe_write_str("\n");
}

}  // namespace flog_details

using namespace flog_details;

/// For each category, if its name matches the wildcard, set its enabled to the given sense.
static void apply_one_wildcard(const wcstring &wc_esc, bool sense) {
    wcstring wc = parse_util_unescape_wildcards(wc_esc);
    bool match_found = false;
    for (category_t *cat : s_all_categories) {
        if (wildcard_match(cat->name, wc)) {
            cat->enabled = sense;
            match_found = true;
        }
    }
    if (!match_found) {
        fprintf(stderr, "Failed to match debug category: %ls\n", wc_esc.c_str());
    }
}

void activate_flog_categories_by_pattern(wcstring wc) {
    // Normalize underscores to dashes, allowing the user to be sloppy.
    std::replace(wc.begin(), wc.end(), L'_', L'-');
    for (const wcstring &s : split_string(wc, L',')) {
        if (string_prefixes_string(L"-", s)) {
            apply_one_wildcard(s.substr(1), false);
        } else {
            apply_one_wildcard(s, true);
        }
    }
}

void set_flog_output_file(FILE *f) {
    assert(f && "Null file");
    g_logger.acquire()->set_file(f);
    s_flog_file_fd = fileno(f);
}

void log_extra_to_flog_file(const wcstring &s) { g_logger.acquire()->log_extra(s.c_str()); }

int get_flog_file_fd() { return s_flog_file_fd; }

std::vector<const category_t *> get_flog_categories() {
    std::vector<const category_t *> result(s_all_categories.begin(), s_all_categories.end());
    std::sort(result.begin(), result.end(), [](const category_t *a, const category_t *b) {
        return wcscmp(a->name, b->name) < 0;
    });
    return result;
}
