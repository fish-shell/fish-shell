// fish logging
#include "config.h"

#include "flog.h"

#include <vector>

#include "common.h"
#include "enum_set.h"
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

void activate_flog_categories_by_pattern(const wcstring &inwc) {
    // Normalize underscores to dashes, allowing the user to be sloppy.
    wcstring wc = inwc;
    std::replace(wc.begin(), wc.end(), L'_', L'-');
    for (const wcstring &s : split_string(wc, L',')) {
        if (string_prefixes_string(L"-", s)) {
            apply_one_wildcard(s.substr(1), false);
        } else {
            apply_one_wildcard(s, true);
        }
    }
}

void set_flog_output_file(FILE *f) { g_logger.acquire()->set_file(f); }

void log_extra_to_flog_file(const wcstring &s) { g_logger.acquire()->log_extra(s.c_str()); }

std::vector<const category_t *> get_flog_categories() {
    std::vector<const category_t *> result(s_all_categories.begin(), s_all_categories.end());
    std::sort(result.begin(), result.end(), [](const category_t *a, const category_t *b) {
        return wcscmp(a->name, b->name) < 0;
    });
    return result;
}
