#ifndef FISH_TOKENIZER_H
#define FISH_TOKENIZER_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "maybe.h"
#include "parse_constants.h"
#include "redirection.h"

using tok_flags_t = unsigned int;

#define TOK_ACCEPT_UNFINISHED 1
#define TOK_SHOW_COMMENTS 2
#define TOK_SHOW_BLANK_LINES 4
#define TOK_CONTINUE_AFTER_ERROR 8

#if INCLUDE_RUST_HEADERS

#include "tokenizer.rs.h"
using token_type_t = TokenType;
using tokenizer_error_t = TokenizerError;
using tok_t = Tok;
using tokenizer_t = Tokenizer;
using pipe_or_redir_t = PipeOrRedir;
using move_word_state_machine_t = MoveWordStateMachine;
using move_word_style_t = MoveWordStyle;

#else

// Hacks to allow us to compile without Rust headers.
enum class tokenizer_error_t : uint8_t {
    none,
    unterminated_quote,
    unterminated_subshell,
    unterminated_slice,
    unterminated_escape,
    invalid_redirect,
    invalid_pipe,
    invalid_pipe_ampersand,
    closing_unopened_subshell,
    illegal_slice,
    closing_unopened_brace,
    unterminated_brace,
    expected_pclose_found_bclose,
    expected_bclose_found_pclose,
};

#endif

#endif
