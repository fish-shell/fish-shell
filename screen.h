/** \file screen.h High level library for handling the terminal screen

	The screen library allows the interactive reader to write its
	output to screen efficiently by keeping an inetrnal representation
	of the current screen contents and trying to find a reasonably
	efficient way for transforming that to the desired screen content.

	The current implementation is less smart than ncurses allows
	and can not for example move blocks of text around to handle text
	insertion.
  */
#ifndef FISH_SCREEN_H
#define FISH_SCREEN_H

#include <vector>

/* A type that represents a color */
class rgb_color_t {
    enum {
        type_none,
        type_named,
        type_rgb,
        type_reset,
        type_ignore
    };
    unsigned char type;
    union {
        unsigned char name_idx; //0-10
        unsigned char rgb[3];
    } data;
    unsigned flags;
    
    /* Try parsing an explicit color name like "magenta" */
    bool try_parse_named(const wcstring &str) {
        bzero(&data, sizeof data);
        const struct {const wchar_t * name; unsigned char idx;} names[] = {
            {L"black", 0},
            {L"red", 1},
            {L"green", 2},
            {L"brown", 3},
            {L"yellow", 3},
            {L"blue", 4},
            {L"magenta", 5},
            {L"purple", 5},
            {L"cyan", 6},
            {L"white", 7},
            {L"normal", 8}            
        };
        size_t max = sizeof names / sizeof *names;
        for (size_t idx=0; idx < max; idx++) {
            if (0 == wcscasecmp(str.c_str(), names[idx].name)) {
                data.name_idx = names[idx].idx;
                return true;
            }
        }
        return false;
    }
    
    static int parse_hex_digit(wchar_t x) {
        switch (x) {
            case L'0': return 0x0;
            case L'1': return 0x1;
            case L'2': return 0x2;
            case L'3': return 0x3;
            case L'4': return 0x4;
            case L'5': return 0x5;
            case L'6': return 0x6;
            case L'7': return 0x7;
            case L'8': return 0x8;
            case L'9': return 0x9;
            case L'a':case L'A': return 0xA;
            case L'b':case L'B': return 0xB;
            case L'c':case L'C': return 0xC;
            case L'd':case L'D': return 0xD;
            case L'e':case L'E': return 0xE;
            case L'f':case L'F': return 0xF;
            default: return -1;
        }
    }
    
    bool try_parse_rgb(const wcstring &name) {
        bzero(&data, sizeof data);
        /* We support the following style of rgb formats (case insensitive):
            #FA3
            #F3A035
        */
        size_t i;
        if (name.size() == 4 && name.at(0) == L'#') {
            // type #FA3
            for (i=0; i < 3; i++) {
                int val = parse_hex_digit(name.at(i));
                if (val < 0) break;
                data.rgb[i] = val*16+val;
            }
            return i == 3;
        } else if (name.size() == 7 && name.at(0) == L'#') {
            // type #F3A035
            for (i=0; i < 6;) {
                int hi = parse_hex_digit(name.at(i++));
                int lo = parse_hex_digit(name.at(i++));
                if (lo < 0 || hi < 0) break;
                data.rgb[i] = hi*16+lo;
            }
            return i == 6;    
        } else {
            return false;
        }
    }
    
    static rgb_color_t named_color(unsigned char which)
    {
        rgb_color_t result(type_named);
        result.data.name_idx = which;
        return result;
    }
    
    public:
    
    explicit rgb_color_t(unsigned char t = type_none) : type(t), data(), flags() {}
    
    static rgb_color_t normal() { return named_color(8); }    
    static rgb_color_t white() { return named_color(7); }
    static rgb_color_t black() { return named_color(0); }
    static rgb_color_t reset() { return rgb_color_t(type_reset); }
    static rgb_color_t ignore() { return rgb_color_t(type_ignore); }
    
    rgb_color_t(unsigned char r, unsigned char g, unsigned char b)
    {
        type = type_rgb;
        data.rgb[0] = r;
        data.rgb[1] = g;
        data.rgb[2] = b;
    }
    
    static bool parse(const wcstring &str, rgb_color_t &color) {
        if (color.try_parse_named(str)) {
            color.type = type_named;
        } else if (color.try_parse_rgb(str)) {
            color.type = type_rgb;
        } else {
            bzero(color.data.rgb, sizeof color.data.rgb);
            color.type = type_none;
        }
        return color.type != type_none;
    }
    
    unsigned char name_index() const {
        assert(type == type_named);
        return data.name_idx;
    }
    
    bool is_bold() const { return flags & 1; }
    void set_bold(bool x) { if (x) flags |= 1; else flags &= ~1; }
    bool is_underline() const { return flags & 2; }
    void set_underline(bool x) { if (x) flags |= 2; else flags &= ~2; }
    
    bool is_reset(void) const { return type == type_reset; }
    bool is_ignore(void) const { return type == type_ignore; }
    bool is_named(void) const { return type == type_named; }
    
    bool operator==(const rgb_color_t &other) const {
        return type == other.type && ! memcmp(&data, &other.data, sizeof data);
    }
    
    bool operator!=(const rgb_color_t &other) const {
        return !(*this == other);
    }

};

struct line_entry_t
{
    wchar_t text;
    int color;
};

/**
   A class representing a single line of a screen.
*/
class line_t
{
    public:
    std::vector<struct line_entry_t> entries;
    
    void resize(size_t size) {
        entries.resize(size);
    }
    
    line_entry_t &entry(size_t idx) {
        return entries.at(idx);
    }
    
    line_entry_t &create_entry(size_t idx) {
        if (idx >= entries.size()) {
            entries.resize(idx + 1);
        }
        return entries.at(idx);
    }
    
    size_t entry_count(void) {
        return entries.size();
    }
};

/**
 A class representing screen contents.
*/
class screen_data_t
{
    std::vector<line_t> line_datas;
    
    public:
    
    int cursor[2];
    
    line_t &add_line(void) {
        line_datas.resize(line_datas.size() + 1);
        return line_datas.back();
    }
    
    void resize(size_t size) {
        line_datas.resize(size);
    }
    
    line_t &create_line(size_t idx) {
        if (idx >= line_datas.size()) {
            line_datas.resize(idx + 1);
        }
        return line_datas.at(idx);
    }
    
    line_t &line(size_t idx) {
        return line_datas.at(idx);
    }
    
    size_t line_count(void) {
        return line_datas.size();
    }
};

/**
   The struct representing the current and desired screen contents.
*/
typedef struct
{
	/**
	  The internal representation of the desired screen contents.
	*/
	screen_data_t desired;
	/**
	  The internal representation of the actual screen contents.
	*/
	screen_data_t actual;

	/**
	   A string containing the prompt which was last printed to
	   the screen.
	*/
	wcstring actual_prompt;

	/**
	  The actual width of the screen at the time of the last screen
	  write.
	*/
	int actual_width;	

	/**
	   This flag is set to true when there is reason to suspect that
	   the parts of the screen lines where the actual content is not
	   filled in may be non-empty. This means that a clr_eol command
	   has to be sent to the terminal at the end of each line.
	*/
	int need_clear;
	
	/**
	   These status buffers are used to check if any output has occurred
	   other than from fish's main loop, in which case we need to redraw.
	*/
	struct stat prev_buff_1, prev_buff_2, post_buff_1, post_buff_2;
}
	screen_t;

/**
   This is the main function for the screen putput library. It is used
   to define the desired contents of the screen. The screen command
   will use it's knowlege of the current contents of the screen in
   order to render the desired output using as few terminal commands
   as possible.
*/
void s_write( screen_t *s, 
			  const wchar_t *prompt, 
			  const wchar_t *commandline,
			  const int *colors, 
			  const int *indent,
			  int cursor_pos );

/** 
    This function resets the screen buffers internal knowledge about
    the contents of the screen. Use this function when some other
    function than s_write has written to the screen.

    \param s the screen to reset
    \param reset_cursor whether the line on which the curor has changed should be assumed to have changed. If \c reset_cursor is set to 0, the library will attempt to make sure that the screen area does not seem to move up or down on repaint. 

    If reset_cursor is incorreclt set to 0, this may result in screen
    contents being erased. If it is incorrectly set to one, it may
    result in one or more lines of garbage on screen on the next
    repaint. If this happens during a loop, such as an interactive
    resizing, there will be one line of garbage for every repaint,
    which will quicly fill the screen.
*/
void s_reset( screen_t *s, int reset_cursor );

#endif
