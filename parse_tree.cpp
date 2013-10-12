#include "parse_productions.h"
#include "tokenizer.h"
#include <vector>

using namespace parse_productions;

/** Returns a string description of this parse error */
wcstring parse_error_t::describe(const wcstring &src) const
{
    wcstring result = text;
    if (source_start < src.size() && source_start + source_length <= src.size())
    {
        // Locate the beginning of this line of source
        size_t line_start = 0;

        // Look for a newline prior to source_start. If we don't find one, start at the beginning of the string; otherwise start one past the newline
        size_t newline = src.find_last_of(L'\n', source_start);
        //fprintf(stderr, "newline: %lu, source_start %lu, source_length %lu\n", newline, source_start, source_length);
        if (newline != wcstring::npos)
        {
            line_start = newline;// + 1;
        }

        size_t line_end = src.find(L'\n', source_start + source_length);
        if (line_end == wcstring::npos)
        {
            line_end = src.size();
        }
        assert(line_end >= line_start);
        //fprintf(stderr, "source start: %lu, line start %lu\n", source_start, line_start);
        assert(source_start >= line_start);

        // Append the line of text
        result.push_back(L'\n');
        result.append(src, line_start, line_end - line_start);

        // Append the caret line
        result.push_back(L'\n');
        result.append(source_start - line_start, L' ');
        result.push_back(L'^');
    }
    return result;
}

/** Returns a string description of the given token type */
wcstring token_type_description(parse_token_type_t type)
{
    switch (type)
    {
        case token_type_invalid:
            return L"invalid";

        case symbol_job_list:
            return L"job_list";
        case symbol_job:
            return L"job";
        case symbol_job_continuation:
            return L"job_continuation";

        case symbol_statement:
            return L"statement";
        case symbol_block_statement:
            return L"block_statement";
        case symbol_block_header:
            return L"block_header";
        case symbol_for_header:
            return L"for_header";
        case symbol_while_header:
            return L"while_header";
        case symbol_begin_header:
            return L"begin_header";
        case symbol_function_header:
            return L"function_header";

        case symbol_if_statement:
            return L"if_statement";
        case symbol_if_clause:
            return L"if_clause";
        case symbol_else_clause:
            return L"else_clause";
        case symbol_else_continuation:
            return L"else_continuation";

        case symbol_switch_statement:
            return L"switch_statement";
        case symbol_case_item_list:
            return L"case_item_list";
        case symbol_case_item:
            return L"case_item";

        case symbol_argument_list:
            return L"argument_list";

        case symbol_boolean_statement:
            return L"boolean_statement";
        case symbol_decorated_statement:
            return L"decorated_statement";
        case symbol_plain_statement:
            return L"plain_statement";
        case symbol_arguments_or_redirections_list:
            return L"arguments_or_redirections_list";
        case symbol_argument_or_redirection:
            return L"argument_or_redirection";
        case symbol_argument:
            return L"symbol_argument";
        case symbol_redirection:
            return L"symbol_redirection";


        case parse_token_type_string:
            return L"token_string";
        case parse_token_type_pipe:
            return L"token_pipe";
        case parse_token_type_redirection:
            return L"token_redirection";
        case parse_token_type_background:
            return L"token_background";
        case parse_token_type_end:
            return L"token_end";
        case parse_token_type_terminate:
            return L"token_terminate";
        case symbol_optional_background:
            return L"optional_background";

        case parse_special_type_parse_error:
            return L"parse_error";
        case parse_special_type_tokenizer_error:
            return L"tokenizer_error";
        case parse_special_type_comment:
            return L"comment";

    }
    return format_string(L"Unknown token type %ld", static_cast<long>(type));
}

wcstring keyword_description(parse_keyword_t k)
{
    switch (k)
    {
        case parse_keyword_none:
            return L"none";
        case parse_keyword_if:
            return L"if";
        case parse_keyword_else:
            return L"else";
        case parse_keyword_for:
            return L"for";
        case parse_keyword_in:
            return L"in";
        case parse_keyword_while:
            return L"while";
        case parse_keyword_begin:
            return L"begin";
        case parse_keyword_function:
            return L"function";
        case parse_keyword_switch:
            return L"switch";
        case parse_keyword_end:
            return L"end";
        case parse_keyword_and:
            return L"and";
        case parse_keyword_or:
            return L"or";
        case parse_keyword_not:
            return L"not";
        case parse_keyword_command:
            return L"command";
        case parse_keyword_builtin:
            return L"builtin";
        default:
            return format_string(L"Unknown keyword type %ld", static_cast<long>(k));
    }
}

/** Returns a string description of the given parse node */
wcstring parse_node_t::describe(void) const
{
    wcstring result = token_type_description(type);
    return result;
}


/** Returns a string description of the given parse token */
wcstring parse_token_t::describe() const
{
    wcstring result = token_type_description(type);
    if (keyword != parse_keyword_none)
    {
        append_format(result, L" <%ls>", keyword_description(keyword).c_str());
    }
    return result;
}


/* Convert from tokenizer_t's token type to a parse_token_t type */
static inline parse_token_type_t parse_token_type_from_tokenizer_token(enum token_type tokenizer_token_type)
{
    parse_token_type_t result = token_type_invalid;
    switch (tokenizer_token_type)
    {
        case TOK_STRING:
            result = parse_token_type_string;
            break;

        case TOK_PIPE:
            result = parse_token_type_pipe;
            break;

        case TOK_END:
            result = parse_token_type_end;
            break;

        case TOK_BACKGROUND:
            result = parse_token_type_background;
            break;

        case TOK_REDIRECT_OUT:
        case TOK_REDIRECT_APPEND:
        case TOK_REDIRECT_IN:
        case TOK_REDIRECT_FD:
        case TOK_REDIRECT_NOCLOB:
            result = parse_token_type_redirection;
            break;

        case TOK_ERROR:
            result = parse_special_type_tokenizer_error;
            break;

        case TOK_COMMENT:
            result = parse_special_type_comment;
            break;


        default:
            fprintf(stderr, "Bad token type %d passed to %s\n", (int)tokenizer_token_type, __FUNCTION__);
            assert(0);
            break;
    }
    return result;
}

/* Helper function for dump_tree */
static void dump_tree_recursive(const parse_node_tree_t &nodes, const wcstring &src, size_t start, size_t indent, wcstring *result, size_t *line)
{
    assert(start < nodes.size());
    const parse_node_t &node = nodes.at(start);

    const size_t spacesPerIndent = 2;

    // unindent statement lists by 1 to flatten them
    if (node.type == symbol_job_list || node.type == symbol_arguments_or_redirections_list)
    {
        if (indent > 0) indent -= 1;
    }

    append_format(*result, L"%2lu - %l2u  ", *line, start);
    result->append(indent * spacesPerIndent, L' ');;
    result->append(node.describe());
    if (node.child_count > 0)
    {
        append_format(*result, L" <%lu children>", node.child_count);
    }
    if (node.type == parse_token_type_string)
    {
        if (node.source_start == -1)
        {
            append_format(*result, L" (no source)");
        }
        else
        {
            result->append(L": \"");
            result->append(src, node.source_start, node.source_length);
            result->append(L"\"");
        }
    }
    result->push_back(L'\n');
    ++*line;
    for (size_t child_idx = node.child_start; child_idx < node.child_start + node.child_count; child_idx++)
    {
        dump_tree_recursive(nodes, src, child_idx, indent + 1, result, line);
    }
}

/* Gives a debugging textual description of a parse tree */
wcstring parse_dump_tree(const parse_node_tree_t &nodes, const wcstring &src)
{
    if (nodes.empty())
        return L"(empty!)";

    size_t line = 0;
    wcstring result;
    dump_tree_recursive(nodes, src, 0, 0, &result, &line);
    return result;
}

/* Struct representing elements of the symbol stack, used in the internal state of the LL parser */
struct parse_stack_element_t
{
    enum parse_token_type_t type;
    enum parse_keyword_t keyword;
    node_offset_t node_idx;

    explicit parse_stack_element_t(parse_token_type_t t, node_offset_t idx) : type(t), keyword(parse_keyword_none), node_idx(idx)
    {
    }

    explicit parse_stack_element_t(production_element_t e, node_offset_t idx) : type(production_element_type(e)), keyword(production_element_keyword(e)), node_idx(idx)
    {
    }

    wcstring describe(void) const
    {
        wcstring result = token_type_description(type);
        if (keyword != parse_keyword_none)
        {
            append_format(result, L" <%ls>", keyword_description(keyword).c_str());
        }
        return result;
    }
};

/* The parser itself, private implementation of class parse_t. This is a hand-coded table-driven LL parser. Most hand-coded LL parsers are recursive descent, but recursive descent parsers are difficult to "pause", unlike table-driven parsers. */
class parse_ll_t
{
    /* Traditional symbol stack of the LL parser */
    std::vector<parse_stack_element_t> symbol_stack;
    
    /* Parser output. This is a parse tree, but stored in an array. */
    parse_node_tree_t nodes;

    /* Whether we ran into a fatal error, including parse errors or tokenizer errors */
    bool fatal_errored;
    
    /* Whether we should collect error messages or not */
    bool should_generate_error_messages;
    
    /* List of errors we have encountered */
    parse_error_list_t errors;
    
    /* The symbol stack can contain terminal types or symbols. Symbols go on to do productions, but terminal types are just matched against input tokens. */
    bool top_node_handle_terminal_types(parse_token_t token);

    void parse_error(const wchar_t *expected, parse_token_t token);
    void parse_error(parse_token_t token, const wchar_t *format, ...);
    void append_error_callout(wcstring &error_message, parse_token_t token);

    void dump_stack(void) const;

    // Get the node corresponding to the top element of the stack
    parse_node_t &node_for_top_symbol()
    {
        PARSE_ASSERT(! symbol_stack.empty());
        const parse_stack_element_t &top_symbol = symbol_stack.back();
        PARSE_ASSERT(top_symbol.node_idx != -1);
        PARSE_ASSERT(top_symbol.node_idx < nodes.size());
        return nodes.at(top_symbol.node_idx);
    }

    parse_token_type_t stack_top_type() const
    {
        return symbol_stack.back().type;
    }

    // Pop from the top of the symbol stack, then push the given production, updating node counts. Note that production_t has type "pointer to array" so some care is required.
    inline void symbol_stack_pop_push_production(const production_t *production)
    {
        bool logit = false;
        if (logit)
        {
            size_t count = 0;
            fprintf(stderr, "Applying production:\n");
            for (size_t i=0; i < MAX_SYMBOLS_PER_PRODUCTION; i++)
            {
                production_element_t elem = (*production)[i];
                if (production_element_is_valid(elem))
                {
                    parse_token_type_t type = production_element_type(elem);
                    parse_keyword_t keyword = production_element_keyword(elem);
                    fprintf(stderr, "\t%ls <%ls>\n", token_type_description(type).c_str(), keyword_description(keyword).c_str());
                    count++;
                }
            }
            if (! count) fprintf(stderr, "\t<empty>\n");
        }
        
        // Get the parent index. But we can't get the parent parse node yet, since it may be made invalid by adding children
        const size_t parent_node_idx = symbol_stack.back().node_idx;

        // Add the children. Confusingly, we want our nodes to be in forwards order (last token last, so dumps look nice), but the symbols should be reverse order (last token first, so it's lowest on the stack)
        const size_t child_start = nodes.size();
        size_t child_count = 0;
        for (size_t i=0; i < MAX_SYMBOLS_PER_PRODUCTION; i++)
        {
            production_element_t elem = (*production)[i];
            if (!production_element_is_valid(elem))
            {
                // All done, bail out
                break;
            }
            else
            {
                // Generate the parse node. Note that this push_back may invalidate node.
                parse_token_type_t child_type = production_element_type(elem);
                parse_node_t child = parse_node_t(child_type);
                child.parent = parent_node_idx;
                nodes.push_back(child);
                child_count++;
            }
        }

        // Update the parent
        parse_node_t &parent_node = nodes.at(parent_node_idx);

        // Should have no children yet
        PARSE_ASSERT(parent_node.child_count == 0);

        // Tell the node about its children
        parent_node.child_start = child_start;
        parent_node.child_count = child_count;

        // Replace the top of the stack with new stack elements corresponding to our new nodes. Note that these go in reverse order.
        symbol_stack.pop_back();
        symbol_stack.reserve(symbol_stack.size() + child_count);
        size_t idx = child_count;
        while (idx--)
        {
            production_element_t elem = (*production)[idx];
            PARSE_ASSERT(production_element_is_valid(elem));
            symbol_stack.push_back(parse_stack_element_t(elem, child_start + idx));
        }
    }

    public:
    
    /* Constructor */
    parse_ll_t() : fatal_errored(false), should_generate_error_messages(true)
    {
        this->symbol_stack.reserve(16);
        this->nodes.reserve(64);
        this->reset_symbols_and_nodes();
    }

    /* Input */
    void accept_tokens(parse_token_t token1, parse_token_t token2);
    
    /* Indicate if we hit a fatal error */
    bool has_fatal_error(void) const
    {
        return this->fatal_errored;
    }
    
    /* Indicate whether we want to generate error messages */
    void set_should_generate_error_messages(bool flag)
    {
        this->should_generate_error_messages = flag;
    }
    
    /* Clear the parse symbol stack (but not the node tree). Add a new job_list_t goal node. This is called from the constructor */
    void reset_symbols(void);

    /* Clear the parse symbol stack and the node tree. Add a new job_list_t goal node. This is called from the constructor. */
    void reset_symbols_and_nodes(void);
    
    /* Once parsing is complete, determine the ranges of intermediate nodes */
    void determine_node_ranges();
    
    /* Acquire output after parsing. This transfers directly from within self */
    void acquire_output(parse_node_tree_t *output, parse_error_list_t *errors);
};

void parse_ll_t::dump_stack(void) const
{
    // Walk backwards from the top, looking for parents
    wcstring_list_t lines;
    if (symbol_stack.empty())
    {
        lines.push_back(L"(empty)");
    }
    else
    {
        node_offset_t child = symbol_stack.back().node_idx;
        node_offset_t cursor = child;
        lines.push_back(nodes.at(cursor).describe());
        while (cursor--)
        {
            const parse_node_t &node = nodes.at(cursor);
            if (node.child_start <= child && node.child_start + node.child_count > child)
            {
                lines.push_back(node.describe());
                child = cursor;
            }
        }
    }

    fprintf(stderr, "Stack dump (%lu elements):\n", symbol_stack.size());
    for (size_t idx = 0; idx < lines.size(); idx++)
    {
        fprintf(stderr, "    %ls\n", lines.at(idx).c_str());
    }
}

// Give each node a source range equal to the union of the ranges of its children
// Terminal nodes already have source ranges (and no children)
// Since children always appear after their parents, we can implement this very simply by walking backwards
void parse_ll_t::determine_node_ranges(void)
{
    const size_t source_start_invalid = -1;
    size_t idx = nodes.size();
    while (idx--)
    {
        parse_node_t *parent = &nodes.at(idx);

        // Skip nodes that already have a source range. These are terminal nodes.
        if (parent->source_start != source_start_invalid)
            continue;

        // Ok, this node needs a source range. Get all of its children, and then set its range.
        size_t min_start = source_start_invalid, max_end = 0; //note source_start_invalid is huge
        for (node_offset_t i=0; i < parent->child_count; i++)
        {
            const parse_node_t &child = nodes.at(parent->child_offset(i));
            min_start = std::min(min_start, child.source_start);
            max_end = std::max(max_end, child.source_start + child.source_length);
        }

        if (min_start != source_start_invalid)
        {
            assert(max_end >= min_start);
            parent->source_start = min_start;
            parent->source_length = max_end - min_start;
        }
    }
}

void parse_ll_t::acquire_output(parse_node_tree_t *output, parse_error_list_t *errors)
{
    if (output != NULL)
    {
        std::swap(*output, this->nodes);
    }
    this->nodes.clear();
    
    if (errors != NULL)
    {
        std::swap(*errors, this->errors);
    }
    this->errors.clear();
    this->symbol_stack.clear();
}

void parse_ll_t::parse_error(parse_token_t token, const wchar_t *fmt, ...)
{
    this->fatal_errored = true;
    if (this->should_generate_error_messages)
    {
        //this->dump_stack();
        parse_error_t err;

        va_list va;
        va_start(va, fmt);
        err.text = vformat_string(fmt, va);
        va_end(va);

        err.source_start = token.source_start;
        err.source_length = token.source_length;
        this->errors.push_back(err);
    }
}


void parse_ll_t::parse_error(const wchar_t *expected, parse_token_t token)
{
    fatal_errored = true;
    if (this->should_generate_error_messages)
    {
        wcstring desc = token_type_description(token.type);
        parse_error_t error;
        error.text = format_string(L"Expected a %ls, instead got a token of type %ls", expected, desc.c_str());
        error.source_start = token.source_start;
        error.source_start = token.source_length;
        errors.push_back(error);
    }
}

void parse_ll_t::reset_symbols(void)
{
    /* Add a new job_list node, and then reset our symbol list to point at it */
    node_offset_t where = nodes.size();
    nodes.push_back(parse_node_t(symbol_job_list));

    symbol_stack.clear();
    symbol_stack.push_back(parse_stack_element_t(symbol_job_list, where)); // goal token
    this->fatal_errored = false;
}

/* Reset both symbols and nodes */
void parse_ll_t::reset_symbols_and_nodes(void)
{
    nodes.clear();
    this->reset_symbols();
}

static bool type_is_terminal_type(parse_token_type_t type)
{
    switch (type)
    {
        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_end:
        case parse_token_type_terminate:
            return true;

        default:
            return false;
    }
}

bool parse_ll_t::top_node_handle_terminal_types(parse_token_t token)
{
    if (symbol_stack.empty())
    {
        // This can come about with an unbalanced 'end' or 'else', which causes us to terminate the outermost job list.
        this->fatal_errored = true;
        return false;
    }

    PARSE_ASSERT(! symbol_stack.empty());
    PARSE_ASSERT(token.type >= FIRST_PARSE_TOKEN_TYPE);
    bool handled = false;
    parse_stack_element_t &stack_top = symbol_stack.back();
    if (type_is_terminal_type(stack_top.type))
    {
        // The top of the stack is terminal. We are going to handle this (because we can't produce from a terminal type)
        handled = true;

        // Now see if we actually matched
        bool matched = false;
        if (stack_top.type == token.type)
        {
            switch (stack_top.type)
            {
                case parse_token_type_string:
                    // We matched if the keywords match, or no keyword was required
                    matched = (stack_top.keyword == parse_keyword_none || stack_top.keyword == token.keyword);
                    break;

                default:
                    // For other types, we only require that the types match
                    matched = true;
                    break;
            }
        }

        if (matched)
        {
            // Success. Tell the node that it matched this token
            parse_node_t &node = node_for_top_symbol();
            node.source_start = token.source_start;
            node.source_length = token.source_length;
        }
        else
        {
            // Failure
            this->fatal_errored = true;
        }

        // We handled the token, so pop the symbol stack
        symbol_stack.pop_back();
    }
    return handled;
}

void parse_ll_t::accept_tokens(parse_token_t token1, parse_token_t token2)
{
    bool logit = false;
    if (logit)
    {
        fprintf(stderr, "Accept token %ls\n", token1.describe().c_str());
    }
    PARSE_ASSERT(token1.type >= FIRST_PARSE_TOKEN_TYPE);

    bool consumed = false;

    // Handle special types specially. Note that these are the only types that can be pushed if the symbol stack is empty.
    if (token1.type == parse_special_type_parse_error || token1.type == parse_special_type_tokenizer_error || token1.type == parse_special_type_comment)
    {
        parse_node_t err_node(token1.type);
        err_node.source_start = token1.source_start;
        err_node.source_length = token1.source_length;
        nodes.push_back(err_node);
        consumed = true;
    }

    while (! consumed && ! this->fatal_errored)
    {
        PARSE_ASSERT(! symbol_stack.empty());

        if (top_node_handle_terminal_types(token1))
        {
            if (logit)
            {
                fprintf(stderr, "Consumed token %ls\n", token1.describe().c_str());
            }
            consumed = true;
            break;
        }

        // top_node_match_token may indicate an error if our stack is empty
        if (this->fatal_errored)
            break;

        // Get the production for the top of the stack
        parse_stack_element_t &stack_elem = symbol_stack.back();
        parse_node_t &node = nodes.at(stack_elem.node_idx);
        const production_t *production = production_for_token(stack_elem.type, token1, token2, &node.production_idx, NULL /* error text */);
        if (production == NULL)
        {
            if (should_generate_error_messages)
            {
                this->parse_error(token1, L"Unable to produce a '%ls' from input '%ls'", stack_elem.describe().c_str(), token1.describe().c_str());
            }
            else
            {
                this->parse_error(token1, NULL);
            }
            // parse_error sets fatal_errored, which ends the loop
        }
        else
        {
            // Manipulate the symbol stack.
            // Note that stack_elem is invalidated by popping the stack.
            symbol_stack_pop_push_production(production);

            // If we end up with an empty stack, something bad happened, like an unbalanced end
            if (symbol_stack.empty())
            {
                this->parse_error(token1, L"All symbols removed from symbol stack. Likely unbalanced else or end?");
            }
        }
    }
}

parse_t::parse_t() : parser(new parse_ll_t())
{
}

parse_t::~parse_t()
{
    delete parser;
}

static parse_keyword_t keyword_for_token(token_type tok, const wchar_t *tok_txt)
{
    parse_keyword_t result = parse_keyword_none;
    if (tok == TOK_STRING)
    {

        const struct
        {
            const wchar_t *txt;
            parse_keyword_t keyword;
        } keywords[] =
        {
            {L"if", parse_keyword_if},
            {L"else", parse_keyword_else},
            {L"for", parse_keyword_for},
            {L"in", parse_keyword_in},
            {L"while", parse_keyword_while},
            {L"begin", parse_keyword_begin},
            {L"function", parse_keyword_function},
            {L"switch", parse_keyword_switch},
            {L"case", parse_keyword_case},
            {L"end", parse_keyword_end},
            {L"and", parse_keyword_and},
            {L"or", parse_keyword_or},
            {L"not", parse_keyword_not},
            {L"command", parse_keyword_command},
            {L"builtin", parse_keyword_builtin}
        };

        for (size_t i=0; i < sizeof keywords / sizeof *keywords; i++)
        {
            if (! wcscmp(keywords[i].txt, tok_txt))
            {
                result = keywords[i].keyword;
                break;
            }
        }
    }
    return result;
}

/* Placeholder invalid token */
static const parse_token_t kInvalidToken = {token_type_invalid, parse_keyword_none, false, -1, -1};

/* Return a new parse token, advancing the tokenizer */
static inline parse_token_t next_parse_token(tokenizer_t *tok)
{
    if (! tok_has_next(tok))
    {
        return kInvalidToken;
    }
    
    token_type tok_type = static_cast<token_type>(tok_last_type(tok));
    int tok_start = tok_get_pos(tok);
    size_t tok_extent = tok_get_extent(tok);
    assert(tok_extent < 10000000); //paranoia
    const wchar_t *tok_txt = tok_last(tok);

    parse_token_t result;
    
    /* Set the type, keyword, and whether there's a dash prefix. Note that this is quite sketchy, because it ignores quotes. This is the historical behavior. For example, `builtin --names` lists builtins, but `builtin "--names"` attempts to run --names as a command. Amazingly as of this writing (10/12/13) nobody seems to have noticed this. Squint at it really hard ant it even starts to look like a feature. */
    result.type = parse_token_type_from_tokenizer_token(tok_type);
    result.keyword = keyword_for_token(tok_type, tok_txt);
    result.has_dash_prefix = (tok_txt[0] == L'-');
    result.source_start = (size_t)tok_start;
    result.source_length = tok_extent;
    
    tok_next(tok);
    return result;
}

bool parse_t::parse_internal(const wcstring &str, parse_tree_flags_t parse_flags, parse_node_tree_t *output, parse_error_list_t *errors, bool log_it)
{
    this->parser->set_should_generate_error_messages(errors != NULL);

    /* Construct the tokenizer */
    tok_flags_t tok_options = TOK_SQUASH_ERRORS;
    if (parse_flags & parse_flag_include_comments)
        tok_options |= TOK_SHOW_COMMENTS;
    
    if (parse_flags & parse_flag_accept_incomplete_tokens)
        tok_options |= TOK_ACCEPT_UNFINISHED;
    
    tokenizer_t tok = tokenizer_t(str.c_str(), tok_options);
    
    /* We are an LL(2) parser. We pass two tokens at a time. New tokens come in at index 1. Seed our queue with an initial token at index 1. */
    parse_token_t queue[2] = {kInvalidToken, next_parse_token(&tok)};
    
    /* Go until the most recently added token is invalid. Note this may mean we don't process anything if there were no tokens. */
    while (queue[1].type != token_type_invalid)
    {
        /* Push a new token onto the queue */
        queue[0] = queue[1];
        queue[1] = next_parse_token(&tok);
        
        /* Pass these two tokens. We know that queue[0] is valid; queue[1] may be invalid. */
        this->parser->accept_tokens(queue[0], queue[1]);
        
        /* Handle errors */
        if (this->parser->has_fatal_error())
        {
            if (parse_flags & parse_flag_continue_after_error)
            {
                /* Mark a special error token, and then keep going */
                const parse_token_t token = {parse_special_type_parse_error, parse_keyword_none, -1, -1};
                this->parser->accept_tokens(token, kInvalidToken);
                this->parser->reset_symbols();
            }
            else
            {
                /* Bail out */
                break;
            }
        }
    }


    // Teach each node where its source range is
    this->parser->determine_node_ranges();
    
    // Acquire the output from the parser
    this->parser->acquire_output(output, errors);

#if 0
    //wcstring result = dump_tree(this->parser->nodes, str);
    //fprintf(stderr, "Tree (%ld nodes):\n%ls", this->parser->nodes.size(), result.c_str());
    fprintf(stderr, "%lu nodes, node size %lu, %lu bytes\n", output->size(), sizeof(parse_node_t), output->size() * sizeof(parse_node_t));
#endif
    
    // Indicate if we had a fatal error
    return ! this->parser->has_fatal_error();
}

bool parse_t::parse(const wcstring &str, parse_tree_flags_t flags, parse_node_tree_t *output, parse_error_list_t *errors, bool log_it)
{
    parse_t parse;
    return parse.parse_internal(str, flags, output, errors, log_it);
}

bool parse_t::parse_1_token(parse_token_type_t token_type, parse_keyword_t keyword, parse_node_tree_t *output, parse_error_list_t *errors)
{
    const parse_token_t invalid_token = {token_type_invalid, parse_keyword_none, -1, -1};
    
    // Only strings can have keywords. So if we have a keyword, the type must be a string
    assert(keyword == parse_keyword_none || token_type == parse_token_type_string);

    parse_token_t token;
    token.type = token_type;
    token.keyword = keyword;
    token.source_start = -1;
    token.source_length = 0;
    
    bool wants_errors = (errors != NULL);
    this->parser->set_should_generate_error_messages(wants_errors);

    this->parser->accept_tokens(token, invalid_token);

    return ! this->parser->has_fatal_error();
}

void parse_t::clear()
{
    this->parser->reset_symbols_and_nodes();
}

const parse_node_t *parse_node_tree_t::get_child(const parse_node_t &parent, node_offset_t which, parse_token_type_t expected_type) const
{
    const parse_node_t *result = NULL;
    
    /* We may get nodes with no children if we had an imcomplete parse. Don't consider than an error */
    if (parent.child_count > 0)
    {
        PARSE_ASSERT(which < parent.child_count);
        node_offset_t child_offset = parent.child_offset(which);
        if (child_offset < this->size())
        {
            result = &this->at(child_offset);
                        
            /* If we are given an expected type, then the node must be null or that type */
            assert(expected_type == token_type_invalid || expected_type == result->type);
        }
    }

    return result;
}

const parse_node_t *parse_node_tree_t::get_parent(const parse_node_t &node, parse_token_type_t expected_type) const
{
    const parse_node_t *result = NULL;
    if (node.parent != NODE_OFFSET_INVALID)
    {
        PARSE_ASSERT(node.parent < this->size());
        const parse_node_t &parent = this->at(node.parent);
        if (expected_type == token_type_invalid || expected_type == parent.type)
        {
            // The type matches (or no type was requested)
            result = &parent;
        }
    }
    return result;
}

static void find_nodes_recursive(const parse_node_tree_t &tree, const parse_node_t &parent, parse_token_type_t type, parse_node_tree_t::parse_node_list_t *result)
{
    if (parent.type == type) result->push_back(&parent);
    for (size_t i=0; i < parent.child_count; i++)
    {
        const parse_node_t *child = tree.get_child(parent, i);
        assert(child != NULL);
        find_nodes_recursive(tree, *child, type, result);
    }
}

parse_node_tree_t::parse_node_list_t parse_node_tree_t::find_nodes(const parse_node_t &parent, parse_token_type_t type) const
{
    parse_node_list_t result;
    find_nodes_recursive(*this, parent, type, &result);
    return result;
}

/* Return true if the given node has the proposed ancestor as an ancestor (or is itself that ancestor) */
static bool node_has_ancestor(const parse_node_tree_t &tree, const parse_node_t &node, const parse_node_t &proposed_ancestor)
{
    if (&node == &proposed_ancestor)
    {
        /* Found it */
        return true;
    }
    else if (node.parent == NODE_OFFSET_INVALID)
    {
        /* No more parents */
        return false;
    }
    else
    {
        /* Recurse to the parent */
        return node_has_ancestor(tree, tree.at(node.parent), proposed_ancestor);
    }
}

const parse_node_t *parse_node_tree_t::find_last_node_of_type(parse_token_type_t type, const parse_node_t *parent) const
{
    const parse_node_t *result = NULL;
    // Find nodes of the given type in the tree, working backwards
    size_t idx = this->size();
    while (idx--)
    {
        const parse_node_t &node = this->at(idx);
        if (node.type == type)
        {
            // Types match. Check if it has the right parent
            if (parent == NULL || node_has_ancestor(*this, node, *parent))
            {
                // Success
                result = &node;
                break;
            }
        }
    }
    return result;
}


bool parse_node_tree_t::argument_list_is_root(const parse_node_t &node) const
{
    bool result = true;
    assert(node.type == symbol_argument_list || node.type == symbol_arguments_or_redirections_list);
    const parse_node_t *parent = this->get_parent(node);
    if (parent != NULL)
    {
        /* We have a parent - check to make sure it's not another list! */
        result = parent->type != symbol_arguments_or_redirections_list && parent->type != symbol_argument_list;
    }
    return result;
}

enum parse_statement_decoration_t parse_node_tree_t::decoration_for_plain_statement(const parse_node_t &node) const
{
    assert(node.type == symbol_plain_statement);
    enum parse_statement_decoration_t decoration = parse_statement_decoration_none;
    const parse_node_t *decorated_statement = this->get_parent(node, symbol_decorated_statement);
    if (decorated_statement != NULL)
    {
        decoration = static_cast<enum parse_statement_decoration_t>(decorated_statement->production_idx);
    }
    return decoration;
}

bool parse_node_tree_t::command_for_plain_statement(const parse_node_t &node, const wcstring &src, wcstring *out_cmd) const
{
    bool result = false;
    assert(node.type == symbol_plain_statement);
    const parse_node_t *cmd_node = this->get_child(node, 0, parse_token_type_string);
    if (cmd_node != NULL && cmd_node->has_source())
    {
        out_cmd->assign(src, cmd_node->source_start, cmd_node->source_length);
        result = true;
    }
    else
    {
        out_cmd->clear();
    }
    return result;
}
