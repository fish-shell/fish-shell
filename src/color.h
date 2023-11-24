// Color class.
#ifndef FISH_COLOR_H
#define FISH_COLOR_H

#include <cstdint>
#include <cstring>
#include <string>

#include "common.h"

// 24-bit color.
struct color24_t {
    uint8_t rgb[3];
};

/// A type that represents a color. We work hard to keep it at a size of 4 bytes and verify with
/// static_assert
class rgb_color_t {
    // Types
    enum { type_none, type_named, type_rgb, type_normal, type_reset };
    uint8_t type : 3;

    // Flags
    enum {
        flag_bold = 1 << 0,
        flag_underline = 1 << 1,
        flag_italics = 1 << 2,
        flag_dim = 1 << 3,
        flag_reverse = 1 << 4
    };
    uint8_t flags : 5;

    union {
        uint8_t name_idx;  // 0-10
        color24_t color;
    } data;

    /// Try parsing a special color name like "normal".
    bool try_parse_special(const wcstring &special);

    /// Try parsing an rgb color like "#F0A030".
    bool try_parse_rgb(const wcstring &name);

    /// Try parsing an explicit color name like "magenta".
    bool try_parse_named(const wcstring &str);

    /// Parsing entry point.
    void parse(const wcstring &str);

    /// Private constructor.
    explicit rgb_color_t(uint8_t t, uint8_t i = 0);

   public:
    /// Default constructor of type none.
    explicit rgb_color_t() : type(type_none), flags(), data() {}

    /// Parse a color from a string.
    explicit rgb_color_t(const wcstring &str);
    explicit rgb_color_t(const std::string &str);

    /// Returns white.
    static rgb_color_t white();

    /// Returns black.
    static rgb_color_t black();

    /// Returns the reset special color.
    static rgb_color_t reset();

    /// Returns the normal special color.
    static rgb_color_t normal();

    /// Returns the none special color.
    static rgb_color_t none();

    /// Returns whether the color is the normal special color.
    bool is_normal(void) const { return type == type_normal; }

    void set_is_named() { type = type_named; }
    void set_is_rgb() { type = type_rgb; }
    void set_is_normal() { type = type_normal; }
    void set_is_reset() { type = type_reset; }
    void set_name_idx(uint8_t idx) { data.name_idx = idx; }
    void set_color(uint8_t r, uint8_t g, uint8_t b) {
        data.color.rgb[0] = r;
        data.color.rgb[1] = g;
        data.color.rgb[2] = b;
    }

    /// Returns whether the color is the reset special color.
    bool is_reset(void) const { return type == type_reset; }

    /// Returns whether the color is the none special color.
    bool is_none(void) const { return type == type_none; }

    /// Returns whether the color is a named color (like "magenta").
    bool is_named(void) const { return type == type_named; }

    /// Returns whether the color is specified via RGB components.
    bool is_rgb(void) const { return type == type_rgb; }

    /// Returns whether the color is special, that is, not rgb or named.
    bool is_special(void) const { return type != type_named && type != type_rgb; }

    /// Returns the name index for the given color. Requires that the color be named or RGB.
    uint8_t to_name_index() const;

    /// Returns the term256 index for the given color. Requires that the color be RGB.
    uint8_t to_term256_index() const;

    /// Returns the 24 bit color for the given color. Requires that the color be RGB.
    color24_t to_color24() const;

    /// Returns whether the color is bold.
    bool is_bold() const { return static_cast<bool>(flags & flag_bold); }

    /// Set whether the color is bold.
    void set_bold(bool x) {
        if (x)
            flags |= flag_bold;
        else
            flags &= ~flag_bold;
    }

    /// Returns whether the color is underlined.
    bool is_underline() const { return static_cast<bool>(flags & flag_underline); }

    /// Set whether the color is underlined.
    void set_underline(bool x) {
        if (x)
            flags |= flag_underline;
        else
            flags &= ~flag_underline;
    }

    /// Returns whether the color is italics.
    bool is_italics() const { return static_cast<bool>(flags & flag_italics); }

    /// Set whether the color is italics.
    void set_italics(bool x) {
        if (x)
            flags |= flag_italics;
        else
            flags &= ~flag_italics;
    }

    /// Returns whether the color is dim.
    bool is_dim() const { return static_cast<bool>(flags & flag_dim); }

    /// Set whether the color is dim.
    void set_dim(bool x) {
        if (x)
            flags |= flag_dim;
        else
            flags &= ~flag_dim;
    }

    /// Returns whether the color is reverse.
    bool is_reverse() const { return static_cast<bool>(flags & flag_reverse); }

    /// Set whether the color is reverse.
    void set_reverse(bool x) {
        if (x)
            flags |= flag_reverse;
        else
            flags &= ~flag_reverse;
    }

    /// Compare two colors for equality.
    bool operator==(const rgb_color_t &other) const {
        return type == other.type && !std::memcmp(&data, &other.data, sizeof data);
    }

    /// Compare two colors for inequality.
    bool operator!=(const rgb_color_t &other) const { return !(*this == other); }

    /// Returns the names of all named colors.
    static std::vector<wcstring> named_color_names(void);
};

static_assert(sizeof(rgb_color_t) <= 4, "rgb_color_t is too big");

#endif
