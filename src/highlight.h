// Prototypes for functions for syntax highlighting.
#ifndef FISH_HIGHLIGHT_H
#define FISH_HIGHLIGHT_H

#include <stddef.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "color.h"
#include "cxx.h"
#include "env.h"
#include "flog.h"
#include "history.h"
#include "maybe.h"
#include "operation_context.h"
#include "parser.h"

struct Highlighter;

/// Describes the role of a span of text.
enum class highlight_role_t : uint8_t {
    normal = 0,  // normal text
    error,       // error
    command,     // command
    keyword,
    statement_terminator,  // process separator
    param,                 // command parameter (argument)
    option,                // argument starting with "-", up to a "--"
    comment,               // comment
    search_match,          // search match
    operat,                // operator
    escape,                // escape sequences
    quote,                 // quoted string
    redirection,           // redirection
    autosuggestion,        // autosuggestion
    selection,

    // Pager support.
    // NOTE: pager.cpp relies on these being in this order.
    pager_progress,
    pager_background,
    pager_prefix,
    pager_completion,
    pager_description,
    pager_secondary_background,
    pager_secondary_prefix,
    pager_secondary_completion,
    pager_secondary_description,
    pager_selected_background,
    pager_selected_prefix,
    pager_selected_completion,
    pager_selected_description,
};

/// Simply value type describing how a character should be highlighted..
struct highlight_spec_t {
    highlight_role_t foreground{highlight_role_t::normal};
    highlight_role_t background{highlight_role_t::normal};
    bool valid_path{false};
    bool force_underline{false};

    highlight_spec_t() = default;

    /* implicit */ highlight_spec_t(highlight_role_t fg,
                                    highlight_role_t bg = highlight_role_t::normal)
        : foreground(fg), background(bg) {}

    bool operator==(const highlight_spec_t &rhs) const {
        return foreground == rhs.foreground && background == rhs.background &&
               valid_path == rhs.valid_path && force_underline == rhs.force_underline;
    }

    bool operator!=(const highlight_spec_t &rhs) const { return !(*this == rhs); }

    static highlight_spec_t make_background(highlight_role_t bg_role) {
        return highlight_spec_t{highlight_role_t::normal, bg_role};
    }
};

namespace std {
template <>
struct hash<highlight_spec_t> {
    std::size_t operator()(const highlight_spec_t &v) const {
        const size_t vals[4] = {static_cast<uint32_t>(v.foreground),
                                static_cast<uint32_t>(v.background), v.valid_path,
                                v.force_underline};
        return (vals[0] << 0) + (vals[1] << 6) + (vals[2] << 12) + (vals[3] << 18);
    }
};
}  // namespace std

/// Given a string and list of colors of the same size, return the string with ANSI escape sequences
/// representing the colors.
std::string colorize(const wcstring &text, const std::vector<highlight_spec_t> &colors,
                     const environment_t &vars);

/// Perform syntax highlighting for the shell commands in buff. The result is stored in the color
/// array as a color_code from the HIGHLIGHT_ enum for each character in buff.
///
/// \param buffstr The buffer on which to perform syntax highlighting
/// \param color The array in which to store the color codes. The first 8 bits are used for fg
/// color, the next 8 bits for bg color.
/// \param ctx The variables and cancellation check for this operation.
/// \param io_ok If set, allow IO which may block. This means that e.g. invalid commands may be
/// detected.
/// \param cursor The position of the cursor in the commandline.
void highlight_shell(const wcstring &buffstr, std::vector<highlight_spec_t> &color,
                     const operation_context_t &ctx, bool io_ok = false,
                     maybe_t<size_t> cursor = {});

/// Wrapper around colorize(highlight_shell)
wcstring colorize_shell(const wcstring &text, void *parser);

/// highlight_color_resolver_t resolves highlight specs (like "a command") to actual RGB colors.
/// It maintains a cache with no invalidation mechanism. The lifetime of these should typically be
/// one screen redraw.
struct highlight_color_resolver_t {
    /// \return an RGB color for a given highlight spec.
    rgb_color_t resolve_spec(const highlight_spec_t &highlight, bool is_background,
                             const environment_t &vars);

   private:
    std::unordered_map<highlight_spec_t, rgb_color_t> fg_cache_;
    std::unordered_map<highlight_spec_t, rgb_color_t> bg_cache_;
    rgb_color_t resolve_spec_uncached(const highlight_spec_t &highlight, bool is_background,
                                      const environment_t &vars) const;
};

/// Given an item \p item from the history which is a proposed autosuggestion, return whether the
/// autosuggestion is valid. It may not be valid if e.g. it is attempting to cd into a directory
/// which does not exist.
bool autosuggest_validate_from_history(const history_item_t &item,
                                       const wcstring &working_directory,
                                       const operation_context_t &ctx);

// Tests whether the specified string cpath is the prefix of anything we could cd to. directories is
// a list of possible parent directories (typically either the working directory, or the cdpath).
// This does I/O!
//
// This is used only internally to this file, and is exposed only for testing.
enum {
    // The path must be to a directory.
    PATH_REQUIRE_DIR = 1 << 0,
    // Expand any leading tilde in the path.
    PATH_EXPAND_TILDE = 1 << 1,
    // Normalize directories before resolving, as "cd".
    PATH_FOR_CD = 1 << 2,
};
typedef unsigned int path_flags_t;
bool is_potential_path(const wcstring &potential_path_fragment, bool at_cursor,
                       const std::vector<wcstring> &directories, const operation_context_t &ctx,
                       path_flags_t flags);

/// Syntax highlighter helper.
class highlighter_t {
    // The string we're highlighting. Note this is a reference member variable (to avoid copying)!
    // We must not outlive this!
    const wcstring &buff;
    // The position of the cursor within the string.
    const maybe_t<size_t> cursor;
    // The operation context. Again, a reference member variable!
    const operation_context_t &ctx;
    // Whether it's OK to do I/O.
    const bool io_ok;
    // Working directory.
    const wcstring working_directory;
    // The ast we produced.
    rust::Box<Ast> ast;
    rust::Box<Highlighter> highlighter;
    // The resulting colors.
    using color_array_t = std::vector<highlight_spec_t>;
    color_array_t color_array;
    // A stack of variables that the current commandline probably defines.  We mark redirections
    // as valid if they use one of these variables, to avoid marking valid targets as error.
    std::vector<wcstring> pending_variables;

    // Flags we use for AST parsing.
    static constexpr parse_tree_flags_t ast_flags =
        parse_flag_continue_after_error | parse_flag_include_comments |
        parse_flag_accept_incomplete_tokens | parse_flag_leave_unterminated |
        parse_flag_show_extra_semis;

    bool io_still_ok() const;

#if INCLUDE_RUST_HEADERS
    // Declaring methods with forward-declared opaque Rust types like "ast::node_t" will cause
    // undefined reference errors.
    // Color a command.
    void color_command(const ast::string_t &node);
    // Color a node as if it were an argument.
    void color_as_argument(const ast::node_t &node, bool options_allowed = true);
    // Colors the source range of a node with a given color.
    void color_node(const ast::node_t &node, highlight_spec_t color);
    // Colors a range with a given color.
    void color_range(source_range_t range, highlight_spec_t color);
#endif

   public:
    /// \return a substring of our buffer.
    wcstring get_source(source_range_t r) const;

    // AST visitor implementations.
    void visit_keyword(const ast::node_t *kw);
    void visit_token(const ast::node_t *tok);
    void visit_argument(const void *arg, bool cmd_is_cd, bool options_allowed);
    void visit_redirection(const void *redir);
    void visit_variable_assignment(const void *varas);
    void visit_semi_nl(const ast::node_t *semi_nl);
    void visit_decorated_statement(const void *stmt);
    size_t visit_block_statement1(const void *block);
    void visit_block_statement2(size_t pending_variables_count);

#if INCLUDE_RUST_HEADERS
    // Visit an argument, perhaps knowing that our command is cd.
    void visit(const ast::argument_t &arg, bool cmd_is_cd = false, bool options_allowed = true);
#endif

    // Constructor
    highlighter_t(const wcstring &str, maybe_t<size_t> cursor, const operation_context_t &ctx,
                  wcstring wd, bool can_do_io);

    // Perform highlighting, returning an array of colors.
    color_array_t highlight();
};

#endif
