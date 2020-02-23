// Programmatic representation of fish code.
#ifndef FISH_PARSE_PRODUCTIONS_H
#define FISH_PARSE_PRODUCTIONS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <vector>

#include "common.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parse_grammar.h"
#include "tokenizer.h"

class parse_node_tree_t;

typedef uint32_t node_offset_t;

#define NODE_OFFSET_INVALID (static_cast<node_offset_t>(-1))

typedef uint32_t source_offset_t;

constexpr source_offset_t SOURCE_OFFSET_INVALID = static_cast<source_offset_t>(-1);

struct source_range_t {
    uint32_t start;
    uint32_t length;
};

/// A struct representing the token type that we use internally.
struct parse_token_t {
    enum parse_token_type_t type;  // The type of the token as represented by the parser
    enum parse_keyword_t keyword { parse_keyword_none };  // Any keyword represented by this token
    bool has_dash_prefix{false};       // Hackish: whether the source contains a dash prefix
    bool is_help_argument{false};      // Hackish: whether the source looks like '-h' or '--help'
    bool is_newline{false};            // Hackish: if TOK_END, whether the source is a newline.
    bool preceding_escaped_nl{false};  // Whether there was an escaped newline preceding this token.
    bool may_be_variable_assignment{false};  // Hackish: whether this token is a string like FOO=bar
    source_offset_t source_start{SOURCE_OFFSET_INVALID};
    source_offset_t source_length{0};

    wcstring describe() const;
    wcstring user_presentable_description() const;

    constexpr parse_token_t(parse_token_type_t type) : type(type) {}
};

enum {
    parse_flag_none = 0,

    /// Attempt to build a "parse tree" no matter what. This may result in a 'forest' of
    /// disconnected trees. This is intended to be used by syntax highlighting.
    parse_flag_continue_after_error = 1 << 0,
    /// Include comment tokens.
    parse_flag_include_comments = 1 << 1,
    /// Indicate that the tokenizer should accept incomplete tokens */
    parse_flag_accept_incomplete_tokens = 1 << 2,
    /// Indicate that the parser should not generate the terminate token, allowing an 'unfinished'
    /// tree where some nodes may have no productions.
    parse_flag_leave_unterminated = 1 << 3,
    /// Indicate that the parser should generate job_list entries for blank lines.
    parse_flag_show_blank_lines = 1 << 4
};
typedef unsigned int parse_tree_flags_t;

wcstring parse_dump_tree(const parse_node_tree_t &nodes, const wcstring &src);

const wchar_t *token_type_description(parse_token_type_t type);
const wchar_t *keyword_description(parse_keyword_t type);

// Node flags.
enum {
    /// Flag indicating that the node has associated comment nodes.
    parse_node_flag_has_comments = 1 << 0,

    /// Flag indicating that the token was preceded by an escaped newline, e.g.
    ///   echo abc | \
    ///      cat
    parse_node_flag_preceding_escaped_nl = 1 << 1,
};
typedef uint8_t parse_node_flags_t;

/// Node-type specific tag value.
typedef uint8_t parse_node_tag_t;

/// Class for nodes of a parse tree. Since there's a lot of these, the size and order of the fields
/// is important.
class parse_node_t {
   public:
    // Start in the source code.
    source_offset_t source_start;
    // Length of our range in the source code.
    source_offset_t source_length;
    // Parent
    node_offset_t parent;
    // Children
    node_offset_t child_start;
    // Number of children.
    uint8_t child_count;
    // Type of the node.
    enum parse_token_type_t type;
    // Keyword associated with node.
    enum parse_keyword_t keyword;
    // Node flags.
    parse_node_flags_t flags : 4;
    // This is used to store e.g. the statement decoration.
    parse_node_tag_t tag : 4;
    // Description
    wcstring describe() const;

    // Constructor
    explicit parse_node_t(parse_token_type_t ty)
        : source_start(SOURCE_OFFSET_INVALID),
          source_length(0),
          parent(NODE_OFFSET_INVALID),
          child_start(0),
          child_count(0),
          type(ty),
          keyword(parse_keyword_none),
          flags(0),
          tag(0) {}

    node_offset_t child_offset(node_offset_t which) const {
        PARSE_ASSERT(which < child_count);
        return child_start + which;
    }

    /// Indicate if this node has a range of source code associated with it.
    bool has_source() const {
        // Should never have a nonempty range with an invalid offset.
        assert(this->source_start != SOURCE_OFFSET_INVALID || this->source_length == 0);
        return this->source_length > 0;
    }

    /// Indicate if the node has comment nodes.
    bool has_comments() const { return this->flags & parse_node_flag_has_comments; }

    /// Indicates if we have a preceding escaped newline.
    bool has_preceding_escaped_newline() const {
        return this->flags & parse_node_flag_preceding_escaped_nl;
    }

    source_range_t source_range() const {
        assert(has_source());
        return {source_start, source_length};
    }

    /// Gets source for the node, or the empty string if it has no source.
    wcstring get_source(const wcstring &str) const {
        if (!has_source())
            return wcstring();
        else
            return wcstring(str, this->source_start, this->source_length);
    }

    /// Returns whether the given location is within the source range or at its end.
    bool location_in_or_at_end_of_source_range(size_t loc) const {
        return has_source() && source_start <= loc && loc - source_start <= source_length;
    }
};

template <typename Type>
class tnode_t;

/// The parse tree itself.
class parse_node_tree_t : public std::vector<parse_node_t> {
   public:
    parse_node_tree_t() {}
    parse_node_tree_t(parse_node_tree_t &&) = default;
    parse_node_tree_t &operator=(parse_node_tree_t &&) = default;
    parse_node_tree_t(const parse_node_tree_t &) = delete;             // no copying
    parse_node_tree_t &operator=(const parse_node_tree_t &) = delete;  // no copying

    // Get the node corresponding to a child of the given node, or NULL if there is no such child.
    // If expected_type is provided, assert that the node has that type.
    const parse_node_t *get_child(const parse_node_t &parent, node_offset_t which,
                                  parse_token_type_t expected_type = token_type_invalid) const;

    // Find the first direct child of the given node of the given type. asserts on failure.
    const parse_node_t &find_child(const parse_node_t &parent, parse_token_type_t type) const;

    template <typename Type>
    tnode_t<Type> find_child(const parse_node_t &parent) const;

    // Get the node corresponding to the parent of the given node, or NULL if there is no such
    // child. If expected_type is provided, only returns the parent if it is of that type. Note the
    // asymmetry: get_child asserts since the children are known, but get_parent does not, since the
    // parent may not be known.
    const parse_node_t *get_parent(const parse_node_t &node,
                                   parse_token_type_t expected_type = token_type_invalid) const;

    // Finds a node containing the given source location. If 'parent' is not NULL, it must be an
    // ancestor.
    const parse_node_t *find_node_matching_source_location(parse_token_type_t type,
                                                           size_t source_loc,
                                                           const parse_node_t *parent) const;
    // Utilities

    /// Given a node, return all of its comment nodes.
    std::vector<tnode_t<grammar::comment>> comment_nodes_for_node(const parse_node_t &parent) const;

   private:
    template <typename Type>
    friend class tnode_t;
    /// Given a node list (e.g. of type symbol_job_list) and a node type (e.g. symbol_job), return
    /// the next element of the given type in that list, and the tail (by reference). Returns NULL
    /// if we've exhausted the list.
    const parse_node_t *next_node_in_node_list(const parse_node_t &node_list,
                                               parse_token_type_t entry_type,
                                               const parse_node_t **list_tail) const;
};

/// The big entry point. Parse a string, attempting to produce a tree for the given goal type.
bool parse_tree_from_string(const wcstring &str, parse_tree_flags_t flags,
                            parse_node_tree_t *output, parse_error_list_t *errors,
                            parse_token_type_t goal = symbol_job_list);

/// A type wrapping up a parse tree and the original source behind it.
struct parsed_source_t {
    wcstring src;
    parse_node_tree_t tree;

    parsed_source_t(wcstring s, parse_node_tree_t t) : src(std::move(s)), tree(std::move(t)) {}

    parsed_source_t(const parsed_source_t &) = delete;
    void operator=(const parsed_source_t &) = delete;
    parsed_source_t(parsed_source_t &&) = default;
    parsed_source_t &operator=(parsed_source_t &&) = default;
};
/// Return a shared pointer to parsed_source_t, or null on failure.
using parsed_source_ref_t = std::shared_ptr<const parsed_source_t>;
parsed_source_ref_t parse_source(wcstring src, parse_tree_flags_t flags, parse_error_list_t *errors,
                                 parse_token_type_t goal = symbol_job_list);

/// Error message for improper use of the exec builtin.
#define EXEC_ERR_MSG _(L"The '%ls' command can not be used in a pipeline")

#endif
