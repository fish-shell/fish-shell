// Color class.
#ifndef FISH_COLOR_H
#define FISH_COLOR_H

#include <stdbool.h>
#include <string.h>
#include <string>

#include "common.h"

// 24-bit color.
struct color24_t {
    unsigned char rgb[3];
};

/// A type that represents a color. We work hard to keep it at a size of 4 bytes.
class rgb_color_t {
    // Types
    enum { type_none, type_named, type_rgb, type_normal, type_reset };
    unsigned char type : 4;

    // Flags
    enum { flag_bold = 1 << 0, flag_underline = 1 << 1 };
    unsigned char flags : 4;

    union {
        unsigned char name_idx;  // 0-10
        color24_t color;
    } data;

    /// Try parsing a special color name like "normal".
    bool try_parse_special(const wcstring &str);

    /// Try parsing an rgb color like "#F0A030".
    bool try_parse_rgb(const wcstring &str);

    /// Try parsing an explicit color name like "magenta".
    bool try_parse_named(const wcstring &str);

    /// Parsing entry point.
    void parse(const wcstring &str);

    /// Private constructor.
    explicit rgb_color_t(unsigned char t, unsigned char i = 0);

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

    /// Returns a description of the color.
    wcstring description() const;

    /// Returns the name index for the given color. Requires that the color be named or RGB.
    unsigned char to_name_index() const;

    /// Returns the term256 index for the given color. Requires that the color be RGB.
    unsigned char to_term256_index() const;

    /// Returns the 24 bit color for the given color. Requires that the color be RGB.
    color24_t to_color24() const;

    /// Returns whether the color is bold.
    bool is_bold() const { return !!(flags & flag_bold); }

    /// Set whether the color is bold.
    void set_bold(bool x) {
        if (x)
            flags |= flag_bold;
        else
            flags &= ~flag_bold;
    }

    /// Returns whether the color is underlined.
    bool is_underline() const { return !!(flags & flag_underline); }

    /// Set whether the color is underlined.
    void set_underline(bool x) {
        if (x)
            flags |= flag_underline;
        else
            flags &= ~flag_underline;
    }

    /// Compare two colors for equality.
    bool operator==(const rgb_color_t &other) const {
        return type == other.type && !memcmp(&data, &other.data, sizeof data);
    }

    /// Compare two colors for inequality.
    bool operator!=(const rgb_color_t &other) const { return !(*this == other); }

    /// Returns the names of all named colors.
    static wcstring_list_t named_color_names(void);
};

#endif
