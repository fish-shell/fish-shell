/**\file parse_tree.h

    Programmatic representation of fish code.
*/

#ifndef FISH_PARSE_PRODUCTIONS_H
#define FISH_PARSE_PRODUCTIONS_H

#include <wchar.h>

#include "config.h"
#include "util.h"
#include "common.h"
#include "tokenizer.h"
#include "parse_constants.h"
#include <vector>
#include <inttypes.h>

class parse_node_t;
class parse_node_tree_t;
typedef size_t node_offset_t;
#define NODE_OFFSET_INVALID (static_cast<node_offset_t>(-1))

struct parse_error_t
{
    /** Text of the error */
    wcstring text;

    /** Code for the error */
    enum parse_error_code_t code;

    /** Offset and length of the token in the source code that triggered this error */
    size_t source_start;
    size_t source_length;

    /** Return a string describing the error, suitable for presentation to the user. If skip_caret is false, the offending line with a caret is printed as well */
    wcstring describe(const wcstring &src, bool skip_caret = false) const;
};
typedef std::vector<parse_error_t> parse_error_list_t;

/* Returns a description of a list of parse errors */
wcstring parse_errors_description(const parse_error_list_t &errors, const wcstring &src, const wchar_t *prefix = NULL);

/** A struct representing the token type that we use internally */
struct parse_token_t
{
    enum parse_token_type_t type; // The type of the token as represented by the parser
    enum parse_keyword_t keyword; // Any keyword represented by this token
    bool has_dash_prefix; // Hackish: whether the source contains a dash prefix
    bool is_help_argument; // Hackish: whether the source looks like '-h' or '--help'
    size_t source_start;
    size_t source_length;

    wcstring describe() const;
    wcstring user_presentable_description() const;
};


enum
{
    parse_flag_none = 0,

    /* Attempt to build a "parse tree" no matter what. This may result in a 'forest' of disconnected trees. This is intended to be used by syntax highlighting. */
    parse_flag_continue_after_error = 1 << 0,

    /* Include comment tokens */
    parse_flag_include_comments = 1 << 1,

    /* Indicate that the tokenizer should accept incomplete tokens */
    parse_flag_accept_incomplete_tokens = 1 << 2,

    /* Indicate that the parser should not generate the terminate token, allowing an 'unfinished' tree where some nodes may have no productions. */
    parse_flag_leave_unterminated = 1 << 3

};
typedef unsigned int parse_tree_flags_t;

wcstring parse_dump_tree(const parse_node_tree_t &tree, const wcstring &src);

wcstring token_type_description(parse_token_type_t type);
wcstring keyword_description(parse_keyword_t type);

/** Class for nodes of a parse tree */
class parse_node_t
{
public:

    /* Type of the node */
    enum parse_token_type_t type;

    /* Start in the source code */
    size_t source_start;

    /* Length of our range in the source code */
    size_t source_length;

    /* Parent */
    node_offset_t parent;

    /* Children */
    node_offset_t child_start;
    uint8_t child_count;

    /* Which production was used */
    uint8_t production_idx;

    /* Description */
    wcstring describe(void) const;

    /* Constructor */
    explicit parse_node_t(parse_token_type_t ty) : type(ty), source_start(-1), source_length(0), parent(NODE_OFFSET_INVALID), child_start(0), child_count(0), production_idx(-1)
    {
    }

    node_offset_t child_offset(node_offset_t which) const
    {
        PARSE_ASSERT(which < child_count);
        return child_start + which;
    }

    /* Indicate if this node has a range of source code associated with it */
    bool has_source() const
    {
        return source_start != (size_t)(-1);
    }

    /* Gets source for the node, or the empty string if it has no source */
    wcstring get_source(const wcstring &str) const
    {
        if (! has_source())
            return wcstring();
        else
            return wcstring(str, this->source_start, this->source_length);
    }

    /* Returns whether the given location is within the source range or at its end */
    bool location_in_or_at_end_of_source_range(size_t loc) const
    {
        return has_source() && source_start <= loc && loc - source_start <= source_length;
    }
};


/* The parse tree itself */
class parse_node_tree_t : public std::vector<parse_node_t>
{
public:

    /* Get the node corresponding to a child of the given node, or NULL if there is no such child. If expected_type is provided, assert that the node has that type.
     */
    const parse_node_t *get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type = token_type_invalid) const;

    /* Find the first direct child of the given node of the given type. asserts on failure
     */
    const parse_node_t &find_child(const parse_node_t &parent, parse_token_type_t type) const;

    /* Get the node corresponding to the parent of the given node, or NULL if there is no such child. If expected_type is provided, only returns the parent if it is of that type. Note the asymmetry: get_child asserts since the children are known, but get_parent does not, since the parent may not be known. */
    const parse_node_t *get_parent(const parse_node_t &node, parse_token_type_t expected_type = token_type_invalid) const;

    /* Returns the first ancestor of the given type, or NULL. */
    const parse_node_t *get_first_ancestor_of_type(const parse_node_t &node, parse_token_type_t desired_type) const;

    /* Find all the nodes of a given type underneath a given node, up to max_count of them */
    typedef std::vector<const parse_node_t *> parse_node_list_t;
    parse_node_list_t find_nodes(const parse_node_t &parent, parse_token_type_t type, size_t max_count = (size_t)(-1)) const;

    /* Finds the last node of a given type underneath a given node, or NULL if it could not be found. If parent is NULL, this finds the last node in the tree of that type. */
    const parse_node_t *find_last_node_of_type(parse_token_type_t type, const parse_node_t *parent = NULL) const;

    /* Finds a node containing the given source location. If 'parent' is not NULL, it must be an ancestor. */
    const parse_node_t *find_node_matching_source_location(parse_token_type_t type, size_t source_loc, const parse_node_t *parent) const;

    /* Indicate if the given argument_list or arguments_or_redirections_list is a root list, or has a parent */
    bool argument_list_is_root(const parse_node_t &node) const;

    /* Utilities */

    /* Given a plain statement, get the decoration (from the parent node), or none if there is no decoration */
    enum parse_statement_decoration_t decoration_for_plain_statement(const parse_node_t &node) const;

    /* Given a plain statement, get the command by reference (from the child node). Returns true if successful. Clears the command on failure. */
    bool command_for_plain_statement(const parse_node_t &node, const wcstring &src, wcstring *out_cmd) const;

    /* Given a plain statement, return true if the statement is part of a pipeline. If include_first is set, the first command in a pipeline is considered part of it; otherwise only the second or additional commands are */
    bool statement_is_in_pipeline(const parse_node_t &node, bool include_first) const;

    /* Given a redirection, get the redirection type (or TOK_NONE) and target (file path, or fd) */
    enum token_type type_for_redirection(const parse_node_t &node, const wcstring &src, int *out_fd, wcstring *out_target) const;

    /* If the given node is a block statement, returns the header node (for_header, while_header, begin_header, or function_header). Otherwise returns NULL */
    const parse_node_t *header_node_for_block_statement(const parse_node_t &node) const;

    /* Given a node list (e.g. of type symbol_job_list) and a node type (e.g. symbol_job), return the next element of the given type in that list, and the tail (by reference). Returns NULL if we've exhausted the list. */
    const parse_node_t *next_node_in_node_list(const parse_node_t &node_list, parse_token_type_t item_type, const parse_node_t **list_tail) const;

    /* Given a job, return all of its statements. These are 'specific statements' (e.g. symbol_decorated_statement, not symbol_statement) */
    parse_node_list_t specific_statements_for_job(const parse_node_t &job) const;
};

/* The big entry point. Parse a string! */
bool parse_tree_from_string(const wcstring &str, parse_tree_flags_t flags, parse_node_tree_t *output, parse_error_list_t *errors);

/* Fish grammar:

# A job_list is a list of jobs, separated by semicolons or newlines

    job_list = <empty> |
                job job_list
                <TOK_END> job_list

# A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation

    job = statement job_continuation
    job_continuation = <empty> |
                       <TOK_PIPE> statement job_continuation

# A statement is a normal command, or an if / while / and etc

    statement = boolean_statement | block_statement | if_statement | switch_statement | decorated_statement

# A block is a conditional, loop, or begin/end

    if_statement = if_clause else_clause end_command arguments_or_redirections_list
    if_clause = <IF> job STATEMENT_TERMINATOR job_list
    else_clause = <empty> |
                 <ELSE> else_continuation
    else_continuation = if_clause else_clause |
                        STATEMENT_TERMINATOR job_list

    switch_statement = SWITCH <TOK_STRING> STATEMENT_TERMINATOR case_item_list end_command arguments_or_redirections_list
    case_item_list = <empty> |
                    case_item case_item_list |
                    <TOK_END> case_item_list

    case_item = CASE argument_list STATEMENT_TERMINATOR job_list

    block_statement = block_header <TOK_END> job_list end_command arguments_or_redirections_list
    block_header = for_header | while_header | function_header | begin_header
    for_header = FOR var_name IN argument_list
    while_header = WHILE job
    begin_header = BEGIN

# Functions take arguments, and require at least one (the name). No redirections allowed.
    function_header = FUNCTION argument argument_list

# A boolean statement is AND or OR or NOT

    boolean_statement = AND statement | OR statement | NOT statement

# A decorated_statement is a command with a list of arguments_or_redirections, possibly with "builtin" or "command"

    decorated_statement = plain_statement | COMMAND plain_statement | BUILTIN plain_statement
    plain_statement = <TOK_STRING> arguments_or_redirections_list optional_background

    argument_list = <empty> | argument argument_list

    arguments_or_redirections_list = <empty> |
                                     argument_or_redirection arguments_or_redirections_list
    argument_or_redirection = argument | redirection
    argument = <TOK_STRING>

    redirection = <TOK_REDIRECTION> <TOK_STRING>

    terminator = <TOK_END> | <TOK_BACKGROUND>

    optional_background = <empty> | <TOK_BACKGROUND>

    end_command = END

*/

#endif
