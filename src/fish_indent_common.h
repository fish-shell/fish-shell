#ifndef FISH_INDENT_STAGING_H
#define FISH_INDENT_STAGING_H

#include "ast.h"
#include "common.h"
#include "cxx.h"

struct PrettyPrinter;
struct pretty_printer_t {
    // Note: this got somewhat more complicated after introducing the new AST, because that AST no
    // longer encodes detailed lexical information (e.g. every newline). This feels more complex
    // than necessary and would probably benefit from a more layered approach where we identify
    // certain runs, weight line breaks, have a cost model, etc.
    pretty_printer_t(const wcstring &src, bool do_indent);

    // Original source.
    const wcstring &source;

    // The indents of our string.
    // This has the same length as 'source' and describes the indentation level.
    const std::vector<int> indents;

    // The parsed ast.
    rust::Box<Ast> ast;

    rust::Box<PrettyPrinter> visitor;

    // The prettifier output.
    wcstring output;

    // The indent of the source range which we are currently emitting.
    int current_indent{0};

    // Whether to indent, or just insert spaces.
    const bool do_indent;

    // Whether the next gap text should hide the first newline.
    bool gap_text_mask_newline{false};

    // The "gaps": a sorted set of ranges between tokens.
    // These contain whitespace, comments, semicolons, and other lexical elements which are not
    // present in the ast.
    const std::vector<source_range_t> gaps;

    // The sorted set of source offsets of nl_semi_t which should be set as semis, not newlines.
    // This is computed ahead of time for convenience.
    const std::vector<uint32_t> preferred_semi_locations;

    // Flags we support.
    using gap_flags_t = uint32_t;
    enum {
        default_flags = 0,

        // Whether to allow line splitting via escaped newlines.
        // For example, in argument lists:
        //
        //   echo a \
        //   b
        //
        // If this is not set, then split-lines will be joined.
        allow_escaped_newlines = 1 << 0,

        // Whether to require a space before this token.
        // This is used when emitting semis:
        //    echo a; echo b;
        // No space required between 'a' and ';', or 'b' and ';'.
        skip_space = 1 << 1,
    };

#if INCLUDE_RUST_HEADERS
    // \return gap text flags for the gap text that comes *before* a given node type.
    static gap_flags_t gap_text_flags_before_node(const ast::node_t &node);
#endif

    // \return whether we are at the start of a new line.
    bool at_line_start() const { return output.empty() || output.back() == L'\n'; }

    // \return whether we have a space before the output.
    // This ignores escaped spaces and escaped newlines.
    bool has_preceding_space() const;

    // Entry point. Prettify our source code and return it.
    wcstring prettify();

    // \return a substring of source.
    wcstring substr(source_range_t r) const { return source.substr(r.start, r.length); }

    // Return the gap ranges from our ast.
    std::vector<source_range_t> compute_gaps() const;

    // Return sorted list of semi-preferring semi_nl nodes.
    std::vector<uint32_t> compute_preferred_semi_locations() const;

    // Emit a space or indent as necessary, depending on the previous output.
    void emit_space_or_indent(gap_flags_t flags = default_flags);

    // Emit "gap text:" newlines and comments from the original source.
    // Gap text may be a few things:
    //
    // 1. Just a space is common. We will trim the spaces to be empty.
    //
    // Here the gap text is the comment, followed by the newline:
    //
    //    echo abc # arg
    //    echo def
    //
    // 2. It may also be an escaped newline:
    // Here the gap text is a space, backslash, newline, space.
    //
    //     echo \
    //       hi
    //
    // 3. Lastly it may be an error, if there was an error token. Here the gap text is the pipe:
    //
    //   begin | stuff
    //
    //  We do not handle errors here - instead our caller does.
    bool emit_gap_text(source_range_t range, gap_flags_t flags);

    /// \return the gap text ending at a given index into the string, or empty if none.
    source_range_t gap_text_to(uint32_t end) const;

    /// \return whether a range \p r overlaps an error range from our ast.
    bool range_contained_error(source_range_t r) const;

    // Emit the gap text before a source range.
    bool emit_gap_text_before(source_range_t r, gap_flags_t flags);

    /// Given a string \p input, remove unnecessary quotes, etc.
    wcstring clean_text(const wcstring &input);

    // Emit a range of original text. This indents as needed, and also inserts preceding gap text.
    // If \p tolerate_line_splitting is set, then permit escaped newlines; otherwise collapse such
    // lines.
    void emit_text(source_range_t r, gap_flags_t flags);

    void emit_node_text(const void *node);

    // Emit one newline.
    void emit_newline() { output.push_back(L'\n'); }

    // Emit a semicolon.
    void emit_semi() { output.push_back(L';'); }

    void visit_semi_nl(const void *node_);

    void visit_redirection(const void *node_);

    void visit_maybe_newlines(const void *node_);

    void visit_begin_header();

    // The flags we use to parse.
    static parse_tree_flags_t parse_flags() {
        return parse_flag_continue_after_error | parse_flag_include_comments |
               parse_flag_leave_unterminated | parse_flag_show_blank_lines;
    }
};

#endif  // FISH_INDENT_STAGING_H
