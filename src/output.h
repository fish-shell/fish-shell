// SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
// SPDX-FileCopyrightText: © 2009 fish-shell contributors
// SPDX-FileCopyrightText: © 2022 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Generic output functions.
//
// Constants for various character classifications. Each character of a command string can be
// classified as one of the following types.
#ifndef FISH_OUTPUT_H
#define FISH_OUTPUT_H

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "color.h"
#include "common.h"
#include "fallback.h"  // IWYU pragma: keep

class env_var_t;

class outputter_t {
    /// Storage for buffered contents.
    std::string contents_;

    /// Count of how many outstanding begin_buffering() calls there are.
    uint32_t buffer_count_{0};

    /// fd to output to.
    int fd_{-1};

    rgb_color_t last_color = rgb_color_t::normal();
    rgb_color_t last_color2 = rgb_color_t::normal();
    bool was_bold = false;
    bool was_underline = false;
    bool was_italics = false;
    bool was_dim = false;
    bool was_reverse = false;

    void reset_modes() {
        was_bold = false;
        was_underline = false;
        was_italics = false;
        was_dim = false;
        was_reverse = false;
    }

    /// Construct an outputter which outputs to a given fd.
    explicit outputter_t(int fd) : fd_(fd) {}

    /// Flush output, if we have a set fd and our buffering count is 0.
    void maybe_flush() {
        if (fd_ >= 0 && buffer_count_ == 0) flush_to(fd_);
    }

   public:
    /// Construct an outputter which outputs to its string.
    outputter_t() = default;

    /// Unconditionally write the color string to the output.
    bool write_color(rgb_color_t color, bool is_fg);

    /// Set the foreground and background color.
    void set_color(rgb_color_t fg, rgb_color_t bg);

    /// Write a wide character to the receiver.
    void writech(wchar_t ch) { writestr(&ch, 1); }

    /// Write a NUL-terminated wide character string to the receiver.
    void writestr(const wchar_t *str) { writestr(str, wcslen(str)); }

    /// Write a wide character string to the receiver.
    void writestr(const wcstring &str) { writestr(str.data(), str.size()); }

    /// Write the given terminfo string to the receiver, like tputs().
    int term_puts(const char *str, int affcnt);

    /// Write a wide string of the given length.
    void writestr(const wchar_t *str, size_t len);

    /// Write a narrow string of the given length.
    void writestr(const char *str, size_t len) {
        contents_.append(str, len);
        maybe_flush();
    }

    /// Write a narrow NUL-terminated string.
    void writestr(const char *str) { writestr(str, std::strlen(str)); }

    /// Write a narrow character.
    void push_back(char c) {
        contents_.push_back(c);
        maybe_flush();
    }

    /// \return the "output" contents.
    const std::string &contents() const { return contents_; }

    /// Output any buffered data to the given \p fd.
    void flush_to(int fd);

    /// Begins buffering. Output will not be automatically flushed until a corresponding
    /// end_buffering() call.
    void begin_buffering() {
        buffer_count_++;
        assert(buffer_count_ > 0 && "bufferCount_ overflow");
    }

    /// Balance a begin_buffering() call.
    void end_buffering() {
        assert(buffer_count_ > 0 && "bufferCount_ underflow");
        buffer_count_--;
        maybe_flush();
    }

    /// Accesses the singleton stdout outputter.
    /// This can only be used from the main thread.
    /// This outputter flushes its buffer after every write operation.
    static outputter_t &stdoutput();
};

void writembs_check(outputter_t &outp, const char *mbs, const char *mbs_name, bool critical,
                    const char *file, long line);
#define writembs(outp, mbs) writembs_check((outp), (mbs), #mbs, true, __FILE__, __LINE__)
#define writembs_nofail(outp, mbs) writembs_check((outp), (mbs), #mbs, false, __FILE__, __LINE__)

rgb_color_t parse_color(const env_var_t &var, bool is_background);

/// Sets what colors are supported.
enum { color_support_term256 = 1 << 0, color_support_term24bit = 1 << 1 };
using color_support_t = unsigned int;
color_support_t output_get_color_support();
void output_set_color_support(color_support_t val);

rgb_color_t best_color(const std::vector<rgb_color_t> &candidates, color_support_t support);

unsigned char index_for_color(rgb_color_t c);

#endif
