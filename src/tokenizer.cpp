// A specialized tokenizer for tokenizing the fish language. In the future, the tokenizer should be
// extended to support marks, tokenizing multiple strings and disposing of unused string segments.
#include "config.h"  // IWYU pragma: keep

#include "tokenizer.h"

#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <wctype.h>

#include <cwchar>
#include <string>
#include <type_traits>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "future_feature_flags.h"
#include "tokenizer.h"
#include "wutil.h"  // IWYU pragma: keep

// _(s) is already wgettext(s).c_str(), so let's not convert back to wcstring
const wchar_t *tokenizer_get_error_message(tokenizer_error_t err) {
    switch (err) {
        case tokenizer_error_t::none:
            return L"";
        case tokenizer_error_t::unterminated_quote:
            return _(L"Unexpected end of string, quotes are not balanced");
        case tokenizer_error_t::unterminated_subshell:
            return _(L"Unexpected end of string, expecting ')'");
        case tokenizer_error_t::unterminated_slice:
            return _(L"Unexpected end of string, square brackets do not match");
        case tokenizer_error_t::unterminated_escape:
            return _(L"Unexpected end of string, incomplete escape sequence");
        case tokenizer_error_t::invalid_redirect:
            return _(L"Invalid input/output redirection");
        case tokenizer_error_t::invalid_pipe:
            return _(L"Cannot use stdin (fd 0) as pipe output");
        case tokenizer_error_t::invalid_pipe_ampersand:
            return _(L"|& is not valid. In fish, use &| to pipe both stdout and stderr.");
        case tokenizer_error_t::closing_unopened_subshell:
            return _(L"Unexpected ')' for unopened parenthesis");
        case tokenizer_error_t::illegal_slice:
            return _(L"Unexpected '[' at this location");
        case tokenizer_error_t::closing_unopened_brace:
            return _(L"Unexpected '}' for unopened brace expansion");
        case tokenizer_error_t::unterminated_brace:
            return _(L"Unexpected end of string, incomplete parameter expansion");
        case tokenizer_error_t::expected_pclose_found_bclose:
            return _(L"Unexpected '}' found, expecting ')'");
        case tokenizer_error_t::expected_bclose_found_pclose:
            return _(L"Unexpected ')' found, expecting '}'");
    }
    assert(0 && "Unexpected tokenizer error");
    return nullptr;
}

// Whether carets redirect stderr.
static bool caret_redirs() { return !feature_test(features_t::stderr_nocaret); }

/// Return an error token and mark that we no longer have a next token.
tok_t tokenizer_t::call_error(tokenizer_error_t error_type, const wchar_t *token_start,
                              const wchar_t *error_loc, maybe_t<size_t> token_length) {
    assert(error_type != tokenizer_error_t::none && "tokenizer_error_t::none passed to call_error");
    assert(error_loc >= token_start && "Invalid error location");
    assert(this->token_cursor >= token_start && "Invalid buff location");

    // If continue_after_error is set and we have a real token length, then skip past it.
    // Otherwise give up.
    if (token_length.has_value() && continue_after_error) {
        assert(this->token_cursor < error_loc + *token_length && "Unable to continue past error");
        this->token_cursor = error_loc + *token_length;
    } else {
        this->has_next = false;
    }

    tok_t result{token_type_t::error};
    result.error = error_type;
    result.offset = token_start - this->start;
    // If we are passed a token_length, then use it; otherwise infer it from the buffer.
    result.length = token_length ? *token_length : this->token_cursor - token_start;
    result.error_offset_within_token = error_loc - token_start;
    return result;
}

tokenizer_t::tokenizer_t(const wchar_t *start, tok_flags_t flags)
    : token_cursor(start), start(start) {
    assert(start != nullptr && "Invalid start");

    this->accept_unfinished = static_cast<bool>(flags & TOK_ACCEPT_UNFINISHED);
    this->show_comments = static_cast<bool>(flags & TOK_SHOW_COMMENTS);
    this->show_blank_lines = static_cast<bool>(flags & TOK_SHOW_BLANK_LINES);
    this->continue_after_error = static_cast<bool>(flags & TOK_CONTINUE_AFTER_ERROR);
}

tok_t::tok_t(token_type_t type) : type(type) {}

/// Tests if this character can be a part of a string. The redirect ^ is allowed unless it's the
/// first character. Hash (#) starts a comment if it's the first character in a token; otherwise it
/// is considered a string character. See issue #953.
static bool tok_is_string_character(wchar_t c, bool is_first) {
    switch (c) {
        case L'\0':
        case L' ':
        case L'\n':
        case L'|':
        case L'\t':
        case L';':
        case L'\r':
        case L'<':
        case L'>':
        case L'&': {
            // Unconditional separators.
            return false;
        }
        case L'^': {
            // Conditional separator.
            return !caret_redirs() || !is_first;
        }
        default: {
            return true;
        }
    }
}

/// Quick test to catch the most common 'non-magical' characters, makes read_string slightly faster
/// by adding a fast path for the most common characters. This is obviously not a suitable
/// replacement for iswalpha.
static inline int myal(wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'); }

namespace tok_modes {
enum {
    regular_text = 0,         // regular text
    subshell = 1 << 0,        // inside of subshell parentheses
    array_brackets = 1 << 1,  // inside of array brackets
    curly_braces = 1 << 2,
    char_escape = 1 << 3,
};
}  // namespace tok_modes
using tok_mode_t = uint32_t;

/// Read the next token as a string.
tok_t tokenizer_t::read_string() {
    tok_mode_t mode{tok_modes::regular_text};
    std::vector<int> paran_offsets;
    std::vector<int> brace_offsets;
    std::vector<char> expecting;
    int slice_offset = 0;
    const wchar_t *const buff_start = this->token_cursor;
    bool is_first = true;

    while (true) {
        wchar_t c = *this->token_cursor;
#if false
        wcstring msg = L"Handling 0x%x (%lc)";
        tok_mode mode_begin = mode;
#endif

        if (c == L'\0') {
            break;
        }

        // Make sure this character isn't being escaped before anything else
        if ((mode & tok_modes::char_escape) == tok_modes::char_escape) {
            mode &= ~(tok_modes::char_escape);
            // and do nothing more
        } else if (myal(c)) {
            // Early exit optimization in case the character is just a letter,
            // which has no special meaning to the tokenizer, i.e. the same mode continues.
        }

        // Now proceed with the evaluation of the token, first checking to see if the token
        // has been explicitly ignored (escaped).
        else if (c == L'\\') {
            mode |= tok_modes::char_escape;
        } else if (c == L'(') {
            paran_offsets.push_back(this->token_cursor - this->start);
            expecting.push_back(L')');
            mode |= tok_modes::subshell;
        } else if (c == L'{') {
            brace_offsets.push_back(this->token_cursor - this->start);
            expecting.push_back(L'}');
            mode |= tok_modes::curly_braces;
        } else if (c == L')') {
            if (!expecting.empty() && expecting.back() == L'}') {
                return this->call_error(tokenizer_error_t::expected_bclose_found_pclose,
                                        this->token_cursor, this->token_cursor, 1);
            }
            if (paran_offsets.empty()) {
                return this->call_error(tokenizer_error_t::closing_unopened_subshell,
                                        this->token_cursor, this->token_cursor, 1);
            }
            paran_offsets.pop_back();
            if (paran_offsets.empty()) {
                mode &= ~(tok_modes::subshell);
            }
            expecting.pop_back();
        } else if (c == L'}') {
            if (!expecting.empty() && expecting.back() == L')') {
                return this->call_error(tokenizer_error_t::expected_pclose_found_bclose,
                                        this->token_cursor, this->token_cursor, 1);
            }
            if (brace_offsets.empty()) {
                return this->call_error(tokenizer_error_t::closing_unopened_brace,
                                        this->token_cursor,
                                        this->token_cursor + wcslen(this->token_cursor));
            }
            brace_offsets.pop_back();
            if (brace_offsets.empty()) {
                mode &= ~(tok_modes::curly_braces);
            }
            expecting.pop_back();
        } else if (c == L'[') {
            if (this->token_cursor != buff_start) {
                mode |= tok_modes::array_brackets;
                slice_offset = this->token_cursor - this->start;
            } else {
                // This is actually allowed so the test operator `[` can be used as the head of a
                // command
            }
        }
        // Only exit bracket mode if we are in bracket mode.
        // Reason: `]` can be a parameter, e.g. last parameter to `[` test alias.
        // e.g. echo $argv[([ $x -eq $y ])] # must not end bracket mode on first bracket
        else if (c == L']' && ((mode & tok_modes::array_brackets) == tok_modes::array_brackets)) {
            mode &= ~(tok_modes::array_brackets);
        } else if (c == L'\'' || c == L'"') {
            const wchar_t *end = quote_end(this->token_cursor);
            if (end) {
                this->token_cursor = end;
            } else {
                const wchar_t *error_loc = this->token_cursor;
                this->token_cursor += std::wcslen(this->token_cursor);
                if ((!this->accept_unfinished)) {
                    return this->call_error(tokenizer_error_t::unterminated_quote, buff_start,
                                            error_loc);
                }
                break;
            }
        } else if (mode == tok_modes::regular_text && !tok_is_string_character(c, is_first)) {
            break;
        }

#if false
        if (mode != mode_begin) {
            msg.append(L": mode 0x%x -> 0x%x\n");
        } else {
            msg.push_back(L'\n');
        }
        FLOGF(error, msg.c_str(), c, c, int(mode_begin), int(mode));
#endif

        this->token_cursor++;
        is_first = false;
    }

    if ((!this->accept_unfinished) && (mode != tok_modes::regular_text)) {
        if (mode & tok_modes::char_escape) {
            return this->call_error(tokenizer_error_t::unterminated_escape, buff_start,
                                    this->token_cursor - 1, 1);
        } else if (mode & tok_modes::array_brackets) {
            return this->call_error(tokenizer_error_t::unterminated_slice, buff_start,
                                    this->start + slice_offset);
        } else if (mode & tok_modes::subshell) {
            assert(!paran_offsets.empty());
            size_t offset_of_open_paran = paran_offsets.back();

            return this->call_error(tokenizer_error_t::unterminated_subshell, buff_start,
                                    this->start + offset_of_open_paran);
        } else if (mode & tok_modes::curly_braces) {
            assert(!brace_offsets.empty());
            size_t offset_of_open_brace = brace_offsets.back();

            return this->call_error(tokenizer_error_t::unterminated_brace, buff_start,
                                    this->start + offset_of_open_brace);
        } else {
            DIE("Unknown non-regular-text mode");
        }
    }

    tok_t result(token_type_t::string);
    result.offset = buff_start - this->start;
    result.length = this->token_cursor - buff_start;
    return result;
}

// Parse an fd from the non-empty string [start, end), all of which are digits.
// Return the fd, or -1 on overflow.
static int parse_fd(const wchar_t *start, const wchar_t *end) {
    assert(start < end && "String cannot be empty");
    long long big_fd = 0;
    for (const wchar_t *cursor = start; cursor < end; ++cursor) {
        assert(L'0' <= *cursor && *cursor <= L'9' && "Not a digit");
        big_fd = big_fd * 10 + (*cursor - L'0');
        if (big_fd > INT_MAX) return -1;
    }
    assert(big_fd <= INT_MAX && "big_fd should be in range");
    return static_cast<int>(big_fd);
}

pipe_or_redir_t::pipe_or_redir_t() = default;

maybe_t<pipe_or_redir_t> pipe_or_redir_t::from_string(const wchar_t *buff) {
    pipe_or_redir_t result{};

    /* Examples of supported syntaxes.
       Note we are only responsible for parsing the redirection part, not 'cmd' or 'file'.

        cmd | cmd        normal pipe
        cmd &| cmd       normal pipe plus stderr-merge
        cmd >| cmd       pipe with explicit fd
        cmd 2>| cmd      pipe with explicit fd
        cmd < file       stdin redirection
        cmd > file       redirection
        cmd >> file      appending redirection
        cmd >? file      noclobber redirection
        cmd >>? file     appending noclobber redirection
        cmd 2> file      file redirection with explicit fd
        cmd >&2          fd redirection with no explicit src fd (stdout is used)
        cmd 1>&2         fd redirection with an explicit src fd
        cmd <&2          fd redirection with no explicit src fd (stdin is used)
        cmd 3<&0         fd redirection with an explicit src fd
        cmd &> file      redirection with stderr merge
        cmd ^ file       caret (stderr) redirection, perhaps disabled via feature flags
        cmd ^^ file      caret (stderr) redirection, perhaps disabled via feature flags
    */

    const wchar_t *cursor = buff;

    // Extract a range of leading fd.
    const wchar_t *fd_start = cursor;
    while (iswdigit(*cursor)) cursor++;
    const wchar_t *fd_end = cursor;
    bool has_fd = (fd_end > fd_start);

    // Try consuming a given character.
    // Return true if consumed. On success, advances cursor.
    auto try_consume = [&cursor](wchar_t c) -> bool {
        if (*cursor != c) return false;
        cursor++;
        return true;
    };

    // Like try_consume, but asserts on failure.
    auto consume = [&](wchar_t c) {
        assert(*cursor == c && "Failed to consume char");
        cursor++;
    };

    switch (*cursor) {
        case L'|': {
            if (has_fd) {
                // Like 123|
                return none();
            }
            consume(L'|');
            assert(*cursor != L'|' &&
                   "|| passed as redirection, this should have been handled as 'or' by the caller");
            result.fd = STDOUT_FILENO;
            result.is_pipe = true;
            break;
        }
        case L'>': {
            consume(L'>');
            if (try_consume(L'>')) result.mode = redirection_mode_t::append;
            if (try_consume(L'|')) {
                // Note we differ from bash here.
                // Consider `echo foo 2>| bar`
                // In fish, this is a *pipe*. Run bar as a command and attach foo's stderr to bar's
                // stdin, while leaving stdout as tty.
                // In bash, this is a *redirection* to bar as a file. It is like > but ignores
                // noclobber.
                result.is_pipe = true;
                result.fd = has_fd ? parse_fd(fd_start, fd_end)  // like 2>|
                                   : STDOUT_FILENO;              // like >|
            } else if (try_consume(L'&')) {
                // This is a redirection to an fd.
                // Note that we allow ">>&", but it's still just writing to the fd - "appending" to
                // it doesn't make sense.
                result.mode = redirection_mode_t::fd;
                result.fd = has_fd ? parse_fd(fd_start, fd_end)  // like 1>&2
                                   : STDOUT_FILENO;              // like >&2
            } else {
                // This is a redirection to a file.
                result.fd = has_fd ? parse_fd(fd_start, fd_end)  // like 1> file.txt
                                   : STDOUT_FILENO;              // like > file.txt
                if (result.mode != redirection_mode_t::append)
                    result.mode = redirection_mode_t::overwrite;
                // Note 'echo abc >>? file' is valid: it means append and noclobber.
                // But here "noclobber" means the file must not exist, so appending
                // can be ignored.
                if (try_consume(L'?')) result.mode = redirection_mode_t::noclob;
            }
            break;
        }
        case L'<': {
            consume(L'<');
            if (try_consume('&')) {
                result.mode = redirection_mode_t::fd;
            } else {
                result.mode = redirection_mode_t::input;
            }
            result.fd = has_fd ? parse_fd(fd_start, fd_end)  // like 1<&3 or 1< /tmp/file.txt
                               : STDIN_FILENO;               // like <&3 or < /tmp/file.txt
            break;
        }
        case L'^': {
            if (!caret_redirs()) {
                // ^ is not special if caret_redirs is disabled.
                return none();
            } else {
                if (has_fd) {
                    return none();
                }
                consume(L'^');
                result.fd = STDERR_FILENO;
                result.mode = redirection_mode_t::overwrite;
                if (try_consume(L'^')) {
                    result.mode = redirection_mode_t::append;
                } else if (try_consume(L'&')) {
                    // This is a redirection to an fd.
                    result.mode = redirection_mode_t::fd;
                }
                if (try_consume(L'?')) result.mode = redirection_mode_t::noclob;
                break;
            }
        }
        case L'&': {
            consume(L'&');
            if (try_consume(L'|')) {
                // &| is pipe with stderr merge.
                result.fd = STDOUT_FILENO;
                result.is_pipe = true;
                result.stderr_merge = true;
            } else if (try_consume(L'>')) {
                result.fd = STDOUT_FILENO;
                result.stderr_merge = true;
                result.mode = redirection_mode_t::overwrite;
                if (try_consume(L'>')) result.mode = redirection_mode_t::append;  // like &>>
                if (try_consume(L'?'))
                    result.mode = redirection_mode_t::noclob;  // like &>? or &>>?
            } else {
                return none();
            }
            break;
        }
        default: {
            // Not a redirection.
            return none();
        }
    }

    result.consumed = (cursor - buff);
    assert(result.consumed > 0 && "Should have consumed at least one character on success");
    return result;
}

int pipe_or_redir_t::oflags() const {
    switch (mode) {
        case redirection_mode_t::append: {
            return O_CREAT | O_APPEND | O_WRONLY;
        }
        case redirection_mode_t::overwrite: {
            return O_CREAT | O_WRONLY | O_TRUNC;
        }
        case redirection_mode_t::noclob: {
            return O_CREAT | O_EXCL | O_WRONLY;
        }
        case redirection_mode_t::input: {
            return O_RDONLY;
        }
        case redirection_mode_t::fd:
        default: {
            return -1;
        }
    }
}

/// Test if a character is whitespace. Differs from iswspace in that it does not consider a
/// newline to be whitespace.
static bool iswspace_not_nl(wchar_t c) {
    switch (c) {
        case L' ':
        case L'\t':
        case L'\r':
            return true;
        case L'\n':
            return false;
        default:
            return iswspace(c);
    }
}

maybe_t<tok_t> tokenizer_t::next() {
    if (!this->has_next) {
        return none();
    }

    // Consume non-newline whitespace. If we get an escaped newline, mark it and continue past
    // it.
    for (;;) {
        if (this->token_cursor[0] == L'\\' && this->token_cursor[1] == L'\n') {
            this->token_cursor += 2;
            this->continue_line_after_comment = true;
        } else if (iswspace_not_nl(this->token_cursor[0])) {
            this->token_cursor++;
        } else {
            break;
        }
    }

    while (*this->token_cursor == L'#') {
        // We have a comment, walk over the comment.
        const wchar_t *comment_start = this->token_cursor;
        while (this->token_cursor[0] != L'\n' && this->token_cursor[0] != L'\0')
            this->token_cursor++;
        size_t comment_len = this->token_cursor - comment_start;

        // If we are going to continue after the comment, skip any trailing newline.
        if (this->token_cursor[0] == L'\n' && this->continue_line_after_comment)
            this->token_cursor++;

        // Maybe return the comment.
        if (this->show_comments) {
            tok_t result(token_type_t::comment);
            result.offset = comment_start - this->start;
            result.length = comment_len;
            return result;
        }
        while (iswspace_not_nl(this->token_cursor[0])) this->token_cursor++;
    }

    // We made it past the comments and ate any trailing newlines we wanted to ignore.
    this->continue_line_after_comment = false;
    const size_t start_pos = this->token_cursor - this->start;

    maybe_t<tok_t> result{};
    switch (*this->token_cursor) {
        case L'\0': {
            this->has_next = false;
            return none();
        }
        case L'\r':  // carriage-return
        case L'\n':  // newline
        case L';': {
            result.emplace(token_type_t::end);
            result->offset = start_pos;
            result->length = 1;
            this->token_cursor++;
            // Hack: when we get a newline, swallow as many as we can. This compresses multiple
            // subsequent newlines into a single one.
            if (!this->show_blank_lines) {
                while (*this->token_cursor == L'\n' || *this->token_cursor == 13 /* CR */ ||
                       *this->token_cursor == ' ' || *this->token_cursor == '\t') {
                    this->token_cursor++;
                }
            }
            break;
        }
        case L'&': {
            if (this->token_cursor[1] == L'&') {
                // && is and.
                result.emplace(token_type_t::andand);
                result->offset = start_pos;
                result->length = 2;
                this->token_cursor += 2;
            } else if (this->token_cursor[1] == L'>' || this->token_cursor[1] == L'|') {
                // &> and &| redirect both stdout and stderr.
                auto redir = pipe_or_redir_t::from_string(this->token_cursor);
                assert(redir.has_value() &&
                       "Should always succeed to parse a &> or &| redirection");
                result.emplace(redir->token_type());
                result->offset = start_pos;
                result->length = redir->consumed;
                this->token_cursor += redir->consumed;
            } else {
                result.emplace(token_type_t::background);
                result->offset = start_pos;
                result->length = 1;
                this->token_cursor++;
            }
            break;
        }
        case L'|': {
            if (this->token_cursor[1] == L'|') {
                // || is or.
                result.emplace(token_type_t::oror);
                result->offset = start_pos;
                result->length = 2;
                this->token_cursor += 2;
            } else if (this->token_cursor[1] == L'&') {
                // |& is a bashism; in fish it's &|.
                return this->call_error(tokenizer_error_t::invalid_pipe_ampersand,
                                        this->token_cursor, this->token_cursor, 2);
            } else {
                auto pipe = pipe_or_redir_t::from_string(this->token_cursor);
                assert(pipe.has_value() && pipe->is_pipe &&
                       "Should always succeed to parse a | pipe");
                result.emplace(pipe->token_type());
                result->offset = start_pos;
                result->length = pipe->consumed;
                this->token_cursor += pipe->consumed;
            }
            break;
        }
        case L'>':
        case L'<': {
            // There's some duplication with the code in the default case below. The key
            // difference here is that we must never parse these as a string; a failed
            // redirection is an error!
            auto redir_or_pipe = pipe_or_redir_t::from_string(this->token_cursor);
            if (!redir_or_pipe || redir_or_pipe->fd < 0) {
                return this->call_error(tokenizer_error_t::invalid_redirect, this->token_cursor,
                                        this->token_cursor,
                                        redir_or_pipe ? redir_or_pipe->consumed : 0);
            }
            result.emplace(redir_or_pipe->token_type());
            result->offset = start_pos;
            result->length = redir_or_pipe->consumed;
            this->token_cursor += redir_or_pipe->consumed;
            break;
        }
        default: {
            // Maybe a redirection like '2>&1', maybe a pipe like 2>|, maybe just a string.
            const wchar_t *error_location = this->token_cursor;
            maybe_t<pipe_or_redir_t> redir_or_pipe{};
            if (iswdigit(*this->token_cursor) || (*this->token_cursor == L'^' && caret_redirs())) {
                redir_or_pipe = pipe_or_redir_t::from_string(this->token_cursor);
            }

            if (redir_or_pipe) {
                // It looks like a redirection or a pipe. But we don't support piping fd 0. Note
                // that fd 0 may be -1, indicating overflow; but we don't treat that as a
                // tokenizer error.
                if (redir_or_pipe->is_pipe && redir_or_pipe->fd == 0) {
                    return this->call_error(tokenizer_error_t::invalid_pipe, error_location,
                                            error_location, redir_or_pipe->consumed);
                }
                result.emplace(redir_or_pipe->token_type());
                result->offset = start_pos;
                result->length = redir_or_pipe->consumed;
                this->token_cursor += redir_or_pipe->consumed;
            } else {
                // Not a redirection or pipe, so just a string.
                result = this->read_string();
            }
            break;
        }
    }
    assert(result.has_value() && "Should have a token");
    return result;
}

wcstring tok_first(const wcstring &str) {
    tokenizer_t t(str.c_str(), 0);
    if (auto token = t.next()) {
        if (token->type == token_type_t::string) {
            return t.text_of(*token);
        }
    }
    return {};
}

wcstring tok_command(const wcstring &str) {
    tokenizer_t t(str.c_str(), 0);
    while (auto token = t.next()) {
        if (token->type != token_type_t::string) {
            return {};
        }
        wcstring text = t.text_of(*token);
        if (variable_assignment_equals_pos(text)) {
            continue;
        }
        return text;
    }
    return {};
}

bool move_word_state_machine_t::consume_char_punctuation(wchar_t c) {
    enum { s_always_one = 0, s_rest, s_whitespace_rest, s_whitespace, s_alphanumeric, s_end };

    bool consumed = false;
    while (state != s_end && !consumed) {
        switch (state) {
            case s_always_one: {
                // Always consume the first character.
                consumed = true;
                if (iswspace(c)) {
                    state = s_whitespace;
                } else if (iswalnum(c)) {
                    state = s_alphanumeric;
                } else {
                    // Don't allow switching type (ws->nonws) after non-whitespace and
                    // non-alphanumeric.
                    state = s_rest;
                }
                break;
            }
            case s_rest: {
                if (iswspace(c)) {
                    // Consume only trailing whitespace.
                    state = s_whitespace_rest;
                } else if (iswalnum(c)) {
                    // Consume only alnums.
                    state = s_alphanumeric;
                } else {
                    consumed = false;
                    state = s_end;
                }
                break;
            }
            case s_whitespace_rest:
            case s_whitespace: {
                // "whitespace" consumes whitespace and switches to alnums,
                // "whitespace_rest" only consumes whitespace.
                if (iswspace(c)) {
                    // Consumed whitespace.
                    consumed = true;
                } else {
                    state = state == s_whitespace ? s_alphanumeric : s_end;
                }
                break;
            }
            case s_alphanumeric: {
                if (iswalnum(c)) {
                    consumed = true;  // consumed alphanumeric
                } else {
                    state = s_end;
                }
                break;
            }
            case s_end:
            default: {
                break;
            }
        }
    }
    return consumed;
}

bool move_word_state_machine_t::is_path_component_character(wchar_t c) {
    // Always treat separators as first. All this does is ensure that we treat ^ as a string
    // character instead of as stderr redirection, which I hypothesize is usually what is
    // desired.
    return tok_is_string_character(c, true) && !std::wcschr(L"/={,}'\":@", c);
}

bool move_word_state_machine_t::consume_char_path_components(wchar_t c) {
    enum {
        s_initial_punctuation,
        s_whitespace,
        s_separator,
        s_slash,
        s_path_component_characters,
        s_end
    };

    // std::fwprintf(stdout, L"state %d, consume '%lc'\n", state, c);
    bool consumed = false;
    while (state != s_end && !consumed) {
        switch (state) {
            case s_initial_punctuation: {
                if (!is_path_component_character(c)) {
                    consumed = true;
                }
                state = s_whitespace;
                break;
            }
            case s_whitespace: {
                if (iswspace(c)) {
                    consumed = true;  // consumed whitespace
                } else if (c == L'/' || is_path_component_character(c)) {
                    state = s_slash;  // path component
                } else {
                    state = s_separator;  // path separator
                }
                break;
            }
            case s_separator: {
                if (!iswspace(c) && !is_path_component_character(c)) {
                    consumed = true;  // consumed separator
                } else {
                    state = s_end;
                }
                break;
            }
            case s_slash: {
                if (c == L'/') {
                    consumed = true;  // consumed slash
                } else {
                    state = s_path_component_characters;
                }
                break;
            }
            case s_path_component_characters: {
                if (is_path_component_character(c)) {
                    consumed = true;  // consumed string character except slash
                } else {
                    state = s_end;
                }
                break;
            }
            case s_end:
            default: {
                break;
            }
        }
    }
    return consumed;
}

bool move_word_state_machine_t::consume_char_whitespace(wchar_t c) {
    // Consume a "word" of printable characters plus any leading whitespace.
    enum { s_always_one = 0, s_blank, s_graph, s_end };

    bool consumed = false;
    while (state != s_end && !consumed) {
        switch (state) {
            case s_always_one: {
                consumed = true;  // always consume the first character
                // If it's not whitespace, only consume those from here.
                if (!iswspace(c)) {
                    state = s_graph;
                } else {
                    // If it's whitespace, keep consuming whitespace until the graphs.
                    state = s_blank;
                }
                break;
            }
            case s_blank: {
                if (iswspace(c)) {
                    consumed = true;  // consumed whitespace
                } else {
                    state = s_graph;
                }
                break;
            }
            case s_graph: {
                if (!iswspace(c)) {
                    consumed = true;  // consumed printable non-space
                } else {
                    state = s_end;
                }
                break;
            }
            case s_end:
            default: {
                break;
            }
        }
    }
    return consumed;
}

bool move_word_state_machine_t::consume_char(wchar_t c) {
    switch (style) {
        case move_word_style_punctuation: {
            return consume_char_punctuation(c);
        }
        case move_word_style_path_components: {
            return consume_char_path_components(c);
        }
        case move_word_style_whitespace: {
            return consume_char_whitespace(c);
        }
    }

    DIE("should not reach this statement");  // silence some compiler errors about not returning
}

move_word_state_machine_t::move_word_state_machine_t(move_word_style_t syl)
    : state(0), style(syl) {}

void move_word_state_machine_t::reset() { state = 0; }

// Return the location of the equals sign, or npos if the string does
// not look like a variable assignment like FOO=bar.  The detection
// works similar as in some POSIX shells: only letters and numbers qre
// allowed on the left hand side, no quotes or escaping.
maybe_t<size_t> variable_assignment_equals_pos(const wcstring &txt) {
    enum { init, has_some_variable_identifier } state = init;
    // TODO bracket indexing
    for (size_t i = 0; i < txt.size(); i++) {
        wchar_t c = txt[i];
        if (state == init) {
            if (!valid_var_name_char(c)) return {};
            state = has_some_variable_identifier;
        } else {
            if (c == '=') return {i};
            if (!valid_var_name_char(c)) return {};
        }
    }
    return {};
}
