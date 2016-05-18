// Color class implementation.
#include "config.h"  // IWYU pragma: keep

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>  // IWYU pragma: keep

#include "color.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep

bool rgb_color_t::try_parse_special(const wcstring &special) {
    memset(&data, 0, sizeof data);
    const wchar_t *name = special.c_str();
    if (!wcscasecmp(name, L"normal")) {
        this->type = type_normal;
    } else if (!wcscasecmp(name, L"reset")) {
        this->type = type_reset;
    } else {
        this->type = type_none;
    }
    return this->type != type_none;
}

static int parse_hex_digit(wchar_t x) {
    switch (x) {
        case L'0': {
            return 0x0;
        }
        case L'1': {
            return 0x1;
        }
        case L'2': {
            return 0x2;
        }
        case L'3': {
            return 0x3;
        }
        case L'4': {
            return 0x4;
        }
        case L'5': {
            return 0x5;
        }
        case L'6': {
            return 0x6;
        }
        case L'7': {
            return 0x7;
        }
        case L'8': {
            return 0x8;
        }
        case L'9': {
            return 0x9;
        }
        case L'a':
        case L'A': {
            return 0xA;
        }
        case L'b':
        case L'B': {
            return 0xB;
        }
        case L'c':
        case L'C': {
            return 0xC;
        }
        case L'd':
        case L'D': {
            return 0xD;
        }
        case L'e':
        case L'E': {
            return 0xE;
        }
        case L'f':
        case L'F': {
            return 0xF;
        }
        default: { return -1; }
    }
}

static unsigned long squared_difference(long p1, long p2) {
    unsigned long diff = (unsigned long)labs(p1 - p2);
    return diff * diff;
}

static unsigned char convert_color(const unsigned char rgb[3], const uint32_t *colors,
                                   size_t color_count) {
    long r = rgb[0], g = rgb[1], b = rgb[2];
    unsigned long best_distance = (unsigned long)-1;
    unsigned char best_index = (unsigned char)-1;
    for (unsigned char idx = 0; idx < color_count; idx++) {
        uint32_t color = colors[idx];
        long test_r = (color >> 16) & 0xFF, test_g = (color >> 8) & 0xFF,
             test_b = (color >> 0) & 0xFF;
        unsigned long distance = squared_difference(r, test_r) + squared_difference(g, test_g) +
                                 squared_difference(b, test_b);
        if (distance <= best_distance) {
            best_index = idx;
            best_distance = distance;
        }
    }
    return best_index;
}

bool rgb_color_t::try_parse_rgb(const wcstring &name) {
    memset(&data, 0, sizeof data);
    // We support the following style of rgb formats (case insensitive):
    //  #FA3
    //  #F3A035
    //  FA3
    //  F3A035
    size_t digit_idx = 0, len = name.size();

    // Skip any leading #.
    if (len > 0 && name.at(0) == L'#') digit_idx++;

    bool success = false;
    size_t i;
    if (len - digit_idx == 3) {
        // Format: FA3
        for (i = 0; i < 3; i++) {
            int val = parse_hex_digit(name.at(digit_idx++));
            if (val < 0) break;
            data.color.rgb[i] = val * 16 + val;
        }
        success = (i == 3);
    } else if (len - digit_idx == 6) {
        // Format: F3A035
        for (i = 0; i < 3; i++) {
            int hi = parse_hex_digit(name.at(digit_idx++));
            int lo = parse_hex_digit(name.at(digit_idx++));
            if (lo < 0 || hi < 0) break;
            data.color.rgb[i] = hi * 16 + lo;
        }
        success = (i == 3);
    }
    if (success) {
        this->type = type_rgb;
    }
    return success;
}

struct named_color_t {
    const wchar_t *name;
    unsigned char idx;
    unsigned char rgb[3];
};

static const named_color_t named_colors[] = {
    {L"black", 0, {0, 0, 0}},
    {L"red", 1, {0xFF, 0, 0}},
    {L"green", 2, {0, 0xFF, 0}},
    {L"brown", 3, {0x72, 0x50, 0}},
    {L"yellow", 3, {0xFF, 0xFF, 0}},
    {L"blue", 4, {0, 0, 0xFF}},
    {L"magenta", 5, {0xFF, 0, 0xFF}},
    {L"purple", 5, {0xFF, 0, 0xFF}},
    {L"cyan", 6, {0, 0xFF, 0xFF}},
    {L"grey", 7, {0xE5, 0xE5, 0xE5}},
    {L"brgrey", 8, {0x55, 0x55, 0x55}},
    {L"brred", 9, {0xFF, 0x55, 0x55}},
    {L"brgreen", 10, {0x55, 0xFF, 0x55}},
    {L"brbrown", 11, {0xFF, 0xFF, 0x55}},
    {L"bryellow", 11, {0xFF, 0xFF, 0x55}},
    {L"brblue", 12, {0x55, 0x55, 0xFF}},
    {L"brmagenta", 13, {0xFF, 0x55, 0xFF}},
    {L"brpurple", 13, {0xFF, 0x55, 0xFF}},
    {L"brcyan", 14, {0x55, 0xFF, 0xFF}},
    {L"white", 15, {0xFF, 0xFF, 0xFF}},
};

wcstring_list_t rgb_color_t::named_color_names(void) {
    size_t count = sizeof named_colors / sizeof *named_colors;
    wcstring_list_t result;
    result.reserve(1 + count);
    for (size_t i = 0; i < count; i++) {
        result.push_back(named_colors[i].name);
    }
    // "normal" isn't really a color and does not have a color palette index or
    // RGB value. Therefore, it does not appear in the named_colors table.
    // However, it is a legitimate color name for the "set_color" command so
    // include it in the publicly known list of colors. This is primarily so it
    // appears in the output of "set_color --print-colors".
    result.push_back(L"normal");
    return result;
}

bool rgb_color_t::try_parse_named(const wcstring &str) {
    memset(&data, 0, sizeof data);
    size_t max = sizeof named_colors / sizeof *named_colors;
    for (size_t idx = 0; idx < max; idx++) {
        if (0 == wcscasecmp(str.c_str(), named_colors[idx].name)) {
            data.name_idx = named_colors[idx].idx;
            this->type = type_named;
            return true;
        }
    }
    return false;
}

static const wchar_t *name_for_color_idx(unsigned char idx) {
    size_t max = sizeof named_colors / sizeof *named_colors;
    for (size_t i = 0; i < max; i++) {
        if (named_colors[i].idx == idx) {
            return named_colors[i].name;
        }
    }
    return L"unknown";
}

rgb_color_t::rgb_color_t(unsigned char t, unsigned char i) : type(t), flags(), data() {
    data.name_idx = i;
}

rgb_color_t rgb_color_t::normal() { return rgb_color_t(type_normal); }

rgb_color_t rgb_color_t::reset() { return rgb_color_t(type_reset); }

rgb_color_t rgb_color_t::none() { return rgb_color_t(type_none); }

rgb_color_t rgb_color_t::white() { return rgb_color_t(type_named, 7); }

rgb_color_t rgb_color_t::black() { return rgb_color_t(type_named, 0); }

static unsigned char term8_color_for_rgb(const unsigned char rgb[3]) {
    const uint32_t kColors[] = {
        0x000000,  // Black
        0xFF0000,  // Red
        0x00FF00,  // Green
        0xFFFF00,  // Yellow
        0x0000FF,  // Blue
        0xFF00FF,  // Magenta
        0x00FFFF,  // Cyan
        0xFFFFFF,  // White
    };
    return convert_color(rgb, kColors, sizeof kColors / sizeof *kColors);
}

static unsigned char term256_color_for_rgb(const unsigned char rgb[3]) {
    const uint32_t kColors[240] = {
        0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f, 0x005f87,
        0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af, 0x0087d7, 0x0087ff,
        0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff, 0x00d700, 0x00d75f, 0x00d787,
        0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f, 0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff,
        0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af, 0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87,
        0x5f5faf, 0x5f5fd7, 0x5f5fff, 0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff,
        0x5faf00, 0x5faf5f, 0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787,
        0x5fd7af, 0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
        0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f, 0x875f87,
        0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af, 0x8787d7, 0x8787ff,
        0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff, 0x87d700, 0x87d75f, 0x87d787,
        0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f, 0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff,
        0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af, 0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87,
        0xaf5faf, 0xaf5fd7, 0xaf5fff, 0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff,
        0xafaf00, 0xafaf5f, 0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787,
        0xafd7af, 0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,
        0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f, 0xd75f87,
        0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af, 0xd787d7, 0xd787ff,
        0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff, 0xd7d700, 0xd7d75f, 0xd7d787,
        0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f, 0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff,
        0xff0000, 0xff005f, 0xff0087, 0xff00af, 0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87,
        0xff5faf, 0xff5fd7, 0xff5fff, 0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff,
        0xffaf00, 0xffaf5f, 0xffaf87, 0xffafaf, 0xffafd7, 0xffafff, 0xffd700, 0xffd75f, 0xffd787,
        0xffd7af, 0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
        0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e, 0x585858,
        0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e, 0xa8a8a8, 0xb2b2b2,
        0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee};
    return 16 + convert_color(rgb, kColors, sizeof kColors / sizeof *kColors);
}

unsigned char rgb_color_t::to_term256_index() const {
    assert(type == type_rgb);
    return term256_color_for_rgb(data.color.rgb);
}

color24_t rgb_color_t::to_color24() const {
    assert(type == type_rgb);
    return data.color;
}

unsigned char rgb_color_t::to_name_index() const {
    assert(type == type_named || type == type_rgb);
    if (type == type_named) return data.name_idx;
    if (type == type_rgb) return term8_color_for_rgb(data.color.rgb);
    return (unsigned char)-1;  // this is an error
}

void rgb_color_t::parse(const wcstring &str) {
    bool success = false;
    if (!success) success = try_parse_special(str);
    if (!success) success = try_parse_named(str);
    if (!success) success = try_parse_rgb(str);
    if (!success) {
        memset(&this->data, 0, sizeof this->data);
        this->type = type_none;
    }
}

rgb_color_t::rgb_color_t(const wcstring &str) : type(), flags() { this->parse(str); }

rgb_color_t::rgb_color_t(const std::string &str) : type(), flags() {
    this->parse(str2wcstring(str));
}

wcstring rgb_color_t::description() const {
    switch (type) {
        case type_none: {
            return L"none";
        }
        case type_named: {
            return format_string(L"named(%d: %ls)", (int)data.name_idx,
                                 name_for_color_idx(data.name_idx));
        }
        case type_rgb: {
            return format_string(L"rgb(0x%02x%02x%02x)", data.color.rgb[0], data.color.rgb[1],
                                 data.color.rgb[2]);
        }
        case type_reset: {
            return L"reset";
        }
        case type_normal: {
            return L"normal";
        }
        default: {
            abort();
            return L"";
        }
    }
}
