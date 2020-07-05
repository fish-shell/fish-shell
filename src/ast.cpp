#include "config.h"  // IWYU pragma: keep

#include "ast.h"

#include <array>

#include "common.h"
#include "flog.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "wutil.h"

namespace {

/// \return tokenizer flags corresponding to parse tree flags.
static tok_flags_t tokenizer_flags_from_parse_flags(parse_tree_flags_t flags) {
    tok_flags_t tok_flags = 0;
    // Note we do not need to respect parse_flag_show_blank_lines, no clients are interested in
    // them.
    if (flags & parse_flag_include_comments) tok_flags |= TOK_SHOW_COMMENTS;
    if (flags & parse_flag_accept_incomplete_tokens) tok_flags |= TOK_ACCEPT_UNFINISHED;
    if (flags & parse_flag_continue_after_error) tok_flags |= TOK_CONTINUE_AFTER_ERROR;
    return tok_flags;
}

/// A token stream generates a sequence of parser tokens, permitting arbitrary lookahead.
class token_stream_t {
   public:
    explicit token_stream_t(const wcstring &src, parse_tree_flags_t flags)
        : src_(src), tok_(src_.c_str(), tokenizer_flags_from_parse_flags(flags)) {}

    /// \return the token at the given index, without popping it. If the token stream is exhausted,
    /// it will have parse_token_type_terminate. idx = 0 means the next token, idx = 1 means the
    /// next-next token, and so forth.
    /// We must have that idx < kMaxLookahead.
    const parse_token_t &peek(size_t idx = 0) {
        assert(idx < kMaxLookahead && "Trying to look too far ahead");
        while (idx >= count_) {
            lookahead_.at(mask(start_ + count_)) = next_from_tok();
            count_ += 1;
        }
        return lookahead_.at(mask(start_ + idx));
    }

    /// Pop the next token.
    parse_token_t pop() {
        if (count_ == 0) {
            return next_from_tok();
        }
        parse_token_t result = std::move(lookahead_[start_]);
        start_ = mask(start_ + 1);
        count_ -= 1;
        return result;
    }

    /// Provide the orignal source code.
    const wcstring &source() const { return src_; }

    /// Any comment nodes are collected here.
    /// These are only collected if parse_flag_include_comments is set.
    std::vector<source_range_t> comment_ranges;

   private:
    // Helper to mask our circular buffer.
    static constexpr size_t mask(size_t idx) { return idx % kMaxLookahead; }

    parse_token_t next_from_tok() {
        for (;;) {
            maybe_t<tok_t> tokenizer_tok{};
            parse_token_t res = next_parse_token(&tok_, &tokenizer_tok, &storage_);
            if (res.type == parse_special_type_comment) {
                comment_ranges.push_back(res.range());
                continue;
            }
            return res;
        }
    }

    // The maximum number of lookahead supported.
    static constexpr size_t kMaxLookahead = 2;

    // We implement a queue with a simple circular buffer.
    // Note that peek() returns an address, so we must not move elements which are peek'd.
    // This prevents using vector (which may reallocate).
    // Deque would work but is too heavyweight for just 2 items.
    std::array<parse_token_t, kMaxLookahead> lookahead_ = {
        {token_type_invalid, token_type_invalid}};

    // Starting index in our lookahead.
    // The "first" token is at this index.
    size_t start_ = 0;

    // Number of items in our lookahead.
    size_t count_ = 0;

    // A reference to the original source.
    const wcstring &src_;

    // The tokenizer to generate new tokens.
    tokenizer_t tok_;

    // Temporary storage.
    wcstring storage_;
};

}  // namespace

namespace ast {

/// Given a node which we believe to be some sort of block statement, attempt to return a source
/// range for the block's keyword (for, if, etc) and a user-presentable description. This is used to
/// provide better error messages. \return {nullptr, nullptr} if we couldn't find it. Note at this
/// point the parse tree is incomplete; in particular parent nodes are not set.
static std::pair<source_range_t, const wchar_t *> find_block_open_keyword(const node_t *node) {
    const node_t *cursor = node;
    while (cursor != nullptr) {
        switch (cursor->type) {
            case type_t::block_statement:
                cursor = cursor->as<block_statement_t>()->header.contents.get();
                break;
            case type_t::for_header: {
                const auto *h = cursor->as<for_header_t>();
                return std::make_pair(h->kw_for.range, L"for loop");
            }
            case type_t::while_header: {
                const auto *h = cursor->as<while_header_t>();
                return std::make_pair(h->kw_while.range, L"while loop");
            }
            case type_t::function_header: {
                const auto *h = cursor->as<function_header_t>();
                return std::make_pair(h->kw_function.range, L"function definition");
            }
            case type_t::begin_header: {
                const auto *h = cursor->as<begin_header_t>();
                return std::make_pair(h->kw_begin.range, L"begin");
            }
            case type_t::if_statement: {
                const auto *h = cursor->as<if_statement_t>();
                return std::make_pair(h->if_clause.kw_if.range, L"if statement");
            }
            case type_t::switch_statement: {
                const auto *h = cursor->as<switch_statement_t>();
                return std::make_pair(h->kw_switch.range, L"switch statement");
            }
            default:
                return std::make_pair(source_range_t{}, nullptr);
        }
    }
    return std::make_pair(source_range_t{}, nullptr);
}

/// \return the decoration for this statement.
parse_statement_decoration_t decorated_statement_t::decoration() const {
    if (!opt_decoration) {
        return parse_statement_decoration_none;
    }
    switch (opt_decoration->kw) {
        case parse_keyword_t::kw_command:
            return parse_statement_decoration_command;
        case parse_keyword_t::kw_builtin:
            return parse_statement_decoration_builtin;
        case parse_keyword_t::kw_exec:
            return parse_statement_decoration_exec;
        default:
            assert(0 && "Unexpected keyword in statement decoration");
            return parse_statement_decoration_none;
    }
}

/// \return a string literal name for an ast type.
const wchar_t *ast_type_to_string(type_t type) {
    switch (type) {
#define ELEM(T)     \
    case type_t::T: \
        return L"" #T;
#include "ast_node_types.inc"
    }
    assert(0 && "unreachable");
    return L"(unknown)";
}

wcstring node_t::describe() const {
    wcstring res = ast_type_to_string(this->type);
    if (const auto *n = this->try_as<token_base_t>()) {
        append_format(res, L" '%ls'", token_type_description(n->type));
    } else if (const auto *n = this->try_as<keyword_base_t>()) {
        append_format(res, L" '%ls'", keyword_description(n->kw));
    }
    return res;
}

node_t::~node_t() = default;

/// From C++14.
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

struct source_range_visitor_t {
    template <typename Node>
    enable_if_t<Node::Category == category_t::leaf> visit(const Node &node) {
        if (node.unsourced) any_unsourced = true;
        // Union with our range.
        if (node.range.length > 0) {
            if (total.length == 0) {
                total = node.range;
            } else {
                auto end =
                    std::max(total.start + total.length, node.range.start + node.range.length);
                total.start = std::min(total.start, node.range.start);
                total.length = end - total.start;
            }
        }
    }

    // Other node types recurse.
    template <typename Node>
    enable_if_t<Node::Category != category_t::leaf> visit(const Node &node) {
        node_visitor(*this).accept_children_of(node);
    }

    // Total range we have encountered.
    source_range_t total{0, 0};

    // Whether any node was found to be unsourced.
    bool any_unsourced{false};
};

maybe_t<source_range_t> node_t::try_source_range() const {
    source_range_visitor_t v;
    node_visitor(v).accept(this);
    if (v.any_unsourced) return none();
    return v.total;
}

// Helper to describe a list of keywords.
// TODO: these need to be localized properly.
static wcstring keywords_user_presentable_description(std::initializer_list<parse_keyword_t> kws) {
    assert(kws.size() > 0 && "Should not be empty list");
    if (kws.size() == 1) {
        return format_string(L"keyword '%ls'", keyword_description(*kws.begin()));
    }
    size_t idx = 0;
    wcstring res = L"keywords ";
    for (parse_keyword_t kw : kws) {
        const wchar_t *optor = (idx++ ? L" or " : L"");
        append_format(res, L"%ls'%ls'", optor, keyword_description(kw));
    }
    return res;
}

// Helper to describe a list of token types.
// TODO: these need to be localized properly.
static wcstring token_types_user_presentable_description(
    std::initializer_list<parse_token_type_t> types) {
    assert(types.size() > 0 && "Should not be empty list");
    if (types.size() == 1) {
        return token_type_user_presentable_description(*types.begin());
    }
    size_t idx = 0;
    wcstring res;
    for (parse_token_type_t type : types) {
        const wchar_t *optor = (idx++ ? L" or " : L"");
        append_format(res, L"%ls%ls", optor, token_type_user_presentable_description(type).c_str());
    }
    return res;
}

class ast_t::populator_t {
    template <typename T>
    using unique_ptr = std::unique_ptr<T>;

   public:
    // Populate \p ast from \p src and \p flags, returning errors (if not null).
    populator_t(ast_t *ast, const wcstring &src, parse_tree_flags_t flags, type_t top_type,
                parse_error_list_t *out_errors)
        : ast_(ast),
          flags_(flags),
          tokens_(src, flags),
          top_type_(top_type),
          out_errors_(out_errors) {
        assert((top_type == type_t::job_list || top_type == type_t::freestanding_argument_list) &&
               "Invalid top type");
        if (top_type == type_t::job_list) {
            unique_ptr<job_list_t> list = allocate<job_list_t>();
            this->populate_list(*list, true /* exhaust_stream */);
            this->ast_->top_ = std::move(list);
        } else {
            unique_ptr<freestanding_argument_list_t> list =
                allocate<freestanding_argument_list_t>();
            this->populate_list(list->arguments, true /* exhaust_stream */);
            this->ast_->top_ = std::move(list);
        }
        // Chomp trailing extras, etc.
        chomp_extras(type_t::job_list);

        // Acquire any comments.
        this->ast_->extras_.comments = std::move(tokens_.comment_ranges);

        assert(this->ast_->top_ && "Should have parsed a node");
    }

    // Given a node type, allocate it and invoke its default constructor.
    // \return the resulting Node pointer. It is never null.
    template <typename Node>
    unique_ptr<Node> allocate() {
        unique_ptr<Node> node = make_unique<Node>();
        FLOGF(ast_construction, L"%*smake %ls %p", spaces(), "", ast_type_to_string(Node::AstType),
              node.get());
        return node;
    }

    // Given a node type, allocate it, invoke its default constructor,
    // and then visit it as a field.
    // \return the resulting Node pointer. It is never null.
    template <typename Node>
    unique_ptr<Node> allocate_visit() {
        unique_ptr<Node> node = allocate<Node>();
        this->visit_node_field(*node);
        return node;
    }

    /// Helper for FLOGF. This returns a number of spaces appropriate for a '%*c' format.
    int spaces() const { return static_cast<int>(visit_stack_.size() * 2); }

    /// The status of our parser.
    enum class status_t {
        // Parsing is going just fine, thanks for asking.
        ok,

        // We have exhausted the token stream, but the caller was OK with an incomplete parse tree.
        // All further leaf nodes should have the unsourced flag set.
        unsourcing,

        // We encountered an parse error and are "unwinding."
        // Do not consume any tokens until we get back to a list type which stops unwinding.
        unwinding,
    };

    /// \return the parser's status.
    status_t status() {
        if (unwinding_) {
            return status_t::unwinding;
        } else if ((flags_ & parse_flag_leave_unterminated) &&
                   peek_type() == parse_token_type_terminate) {
            return status_t::unsourcing;
        }
        return status_t::ok;
    }

    /// \return whether the status is unwinding.
    /// This is more efficient than checking the status directly.
    bool is_unwinding() { return unwinding_; }

    /// \return whether any leaf nodes we visit should be marked as unsourced.
    bool unsource_leaves() {
        status_t s = status();
        return s == status_t::unsourcing || s == status_t::unwinding;
    }

    /// \return whether we permit an incomplete parse tree.
    bool allow_incomplete() const { return flags_ & parse_flag_leave_unterminated; }

    /// This indicates a bug in fish code.
    void internal_error(const char *func, const wchar_t *fmt, ...) const {
        va_list va;
        va_start(va, fmt);
        wcstring msg = vformat_string(fmt, va);
        va_end(va);

        FLOG(debug, "Internal parse error from", func, "- this indicates a bug in fish.", msg);
        FLOG(debug, "Encountered while parsing:<<<\n%ls\n>>>", tokens_.source().c_str());
        abort();
    }

    /// \return whether a list type \p type allows arbitrary newlines in it.
    bool list_type_chomps_newlines(type_t type) const {
        switch (type) {
            case type_t::argument_list:
                // Hackish. If we are producing a freestanding argument list, then it allows
                // semicolons, for hysterical raisins.
                return top_type_ == type_t::freestanding_argument_list;

            case type_t::argument_or_redirection_list:
                // No newlines inside arguments.
                return false;

            case type_t::variable_assignment_list:
                // No newlines inside variable assignment lists.
                return false;

            case type_t::job_list:
                // Like echo a \n \n echo b
                return true;

            case type_t::case_item_list:
                // Like switch foo \n \n \n case a \n end
                return true;

            case type_t::andor_job_list:
                // Like while true ; \n \n and true ; end
                return true;

            case type_t::elseif_clause_list:
                // Like if true ; \n \n else if false; end
                return true;

            case type_t::job_conjunction_continuation_list:
                // This would be like echo a && echo b \n && echo c
                // We could conceivably support this but do not now.
                return false;

            case type_t::job_continuation_list:
                // This would be like echo a \n | echo b
                // We could conceivably support this but do not now.
                return false;

            default:
                internal_error(__FUNCTION__, L"Type %ls not handled", ast_type_to_string(type));
                return false;
        }
    }

    /// \return whether a list type \p type allows arbitrary semicolons in it.
    bool list_type_chomps_semis(type_t type) const {
        switch (type) {
            case type_t::argument_list:
                // Hackish. If we are producing a freestanding argument list, then it allows
                // semicolons, for hysterical raisins.
                // That is, this is OK: complete -c foo -a 'x ; y ; z'
                // But this is not: foo x ; y ; z
                return top_type_ == type_t::freestanding_argument_list;

            case type_t::argument_or_redirection_list:
            case type_t::variable_assignment_list:
                return false;

            case type_t::job_list:
                // Like echo a ; ;  echo b
                return true;

            case type_t::case_item_list:
                // Like switch foo ; ; ;  case a \n end
                // This is historically allowed.
                return true;

            case type_t::andor_job_list:
                // Like while true ; ; ;  and true ; end
                return true;

            case type_t::elseif_clause_list:
                // Like if true ; ; ;  else if false; end
                return false;

            case type_t::job_conjunction_continuation_list:
                // Like echo a ; ; && echo b. Not supported.
                return false;

            case type_t::job_continuation_list:
                // This would be like echo a ; | echo b
                // Not supported.
                // We could conceivably support this but do not now.
                return false;

            default:
                internal_error(__FUNCTION__, L"Type %ls not handled", ast_type_to_string(type));
                return false;
        }
    }

    // Chomp extra comments, semicolons, etc. for a given list type.
    void chomp_extras(type_t type) {
        bool chomp_semis = list_type_chomps_semis(type);
        bool chomp_newlines = list_type_chomps_newlines(type);
        for (;;) {
            const auto &peek = this->tokens_.peek();
            if (chomp_newlines && peek.type == parse_token_type_end && peek.is_newline) {
                // Just skip this newline, no need to save it.
                this->tokens_.pop();
            } else if (chomp_semis && peek.type == parse_token_type_end && !peek.is_newline) {
                auto tok = this->tokens_.pop();
                // Perhaps save this extra semi.
                if (flags_ & parse_flag_show_extra_semis) {
                    ast_->extras_.semis.push_back(tok.range());
                }
            } else {
                break;
            }
        }
    }

    /// \return whether a list type should recover from errors.s
    /// That is, whether we should stop unwinding when we encounter this type.
    bool list_type_stops_unwind(type_t type) const {
        return type == type_t::job_list && (flags_ & parse_flag_continue_after_error);
    }

    /// Report an error based on \p fmt for the source range \p range.
    void parse_error_impl(source_range_t range, parse_error_code_t code, const wchar_t *fmt,
                          va_list va) {
        ast_->any_error_ = true;

        // Ignore additional parse errors while unwinding.
        // These may come about e.g. from `true | and`.
        if (unwinding_) return;
        unwinding_ = true;

        FLOGF(ast_construction, L"%*sparse error - begin unwinding", spaces(), "");
        // TODO: can store this conditionally dependent on flags.
        if (range.start != SOURCE_OFFSET_INVALID) {
            ast_->extras_.errors.push_back(range);
        }

        if (out_errors_) {
            parse_error_t err;
            err.text = vformat_string(fmt, va);
            err.code = code;
            err.source_start = range.start;
            err.source_length = range.length;
            out_errors_->push_back(std::move(err));
        }
    }

    /// Report an error based on \p fmt for the source range \p range.
    void parse_error(source_range_t range, parse_error_code_t code, const wchar_t *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        parse_error_impl(range, code, fmt, va);
        va_end(va);
    }

    /// Report an error based on \p fmt for the source range \p range.
    void parse_error(const parse_token_t &token, parse_error_code_t code, const wchar_t *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        parse_error_impl(token.range(), code, fmt, va);
        va_end(va);
    }

    // \return a reference to a non-comment token at index \p idx.
    const parse_token_t &peek_token(size_t idx = 0) { return tokens_.peek(idx); }

    // \return the type of a non-comment token.
    parse_token_type_t peek_type(size_t idx = 0) { return peek_token(idx).type; }

    // Consume the next token, chomping any comments.
    // It is an error to call this unless we know there is a non-terminate token available.
    // \return the token.
    parse_token_t consume_any_token() {
        parse_token_t tok = tokens_.pop();
        assert(tok.type != parse_special_type_comment && "Should not be a comment");
        assert(tok.type != parse_token_type_terminate &&
               "Cannot consume terminate token, caller should check status first");
        return tok;
    }

    // Consume the next token which is expected to be of the given type.
    source_range_t consume_token_type(parse_token_type_t type) {
        assert(type != parse_token_type_terminate &&
               "Should not attempt to consume terminate token");
        auto tok = consume_any_token();
        if (tok.type != type) {
            parse_error(tok, parse_error_generic, _(L"Expected %ls, but found %ls"),
                        token_type_user_presentable_description(type).c_str(),
                        tok.user_presentable_description().c_str());
            return source_range_t{0, 0};
        }
        return tok.range();
    }

    // The next token could not be parsed at the top level.
    // For example a trailing end like `begin ; end ; end`
    // Or an unexpected redirection like `>`
    // Consume it and add an error.
    void consume_excess_token_generating_error() {
        auto tok = consume_any_token();

        // In the rare case that we are parsing a freestanding argument list and not a job list,
        // generate a generic error.
        // TODO: this is a crummy message if we get a tokenizer error, for example:
        //   complete -c foo -a "'abc"
        if (this->top_type_ == type_t::freestanding_argument_list) {
            this->parse_error(
                tok, parse_error_generic, _(L"Expected %ls, but found %ls"),
                token_type_user_presentable_description(parse_token_type_string).c_str(),
                tok.user_presentable_description().c_str());
            return;
        }

        assert(this->top_type_ == type_t::job_list);
        switch (tok.type) {
            case parse_token_type_string:
                // There are three keywords which end a job list.
                switch (tok.keyword) {
                    case parse_keyword_t::kw_end:
                        this->parse_error(tok, parse_error_unbalancing_end,
                                          _(L"'end' outside of a block"));
                        break;
                    case parse_keyword_t::kw_else:
                        this->parse_error(tok, parse_error_unbalancing_else,
                                          _(L"'else' builtin not inside of if block"));
                        break;
                    case parse_keyword_t::kw_case:
                        this->parse_error(tok, parse_error_unbalancing_case,
                                          _(L"'case' builtin not inside of switch block"));
                        break;
                    default:
                        internal_error(__FUNCTION__,
                                       L"Token %ls should not have prevented parsing a job list",
                                       tok.user_presentable_description().c_str());
                        break;
                }
                break;
            case parse_token_type_pipe:
            case parse_token_type_redirection:
            case parse_token_type_background:
            case parse_token_type_andand:
            case parse_token_type_oror:
                parse_error(tok, parse_error_generic, _(L"Expected a string, but found %ls"),
                            tok.user_presentable_description().c_str());
                break;

            case parse_special_type_tokenizer_error:
                parse_error(tok, parse_error_from_tokenizer_error(tok.tok_error), L"%ls",
                            tokenizer_get_error_message(tok.tok_error));
                break;

            case parse_token_type_end:
                internal_error(__FUNCTION__, L"End token should never be excess");
                break;
            case parse_token_type_terminate:
                internal_error(__FUNCTION__, L"Terminate token should never be excess");
                break;
            default:
                internal_error(__FUNCTION__, L"Unexpected excess token type: %ls",
                               tok.user_presentable_description().c_str());
                break;
        }
    }

    // Our can_parse implementations are for optional values and for lists.
    // A true return means we should descend into the production, false means stop.
    // Note that the argument is always nullptr and should be ignored. It is provided strictly for
    // overloading purposes.
    bool can_parse(job_conjunction_t *) {
        const auto &token = peek_token();
        if (token.type != parse_token_type_string) return false;
        switch (peek_token().keyword) {
            case parse_keyword_t::kw_end:
            case parse_keyword_t::kw_else:
            case parse_keyword_t::kw_case:
                // These end a job list.
                return false;
            case parse_keyword_t::none:
            default:
                return true;
        }
    }

    bool can_parse(argument_t *) { return peek_type() == parse_token_type_string; }
    bool can_parse(redirection_t *) { return peek_type() == parse_token_type_redirection; }
    bool can_parse(argument_or_redirection_t *) {
        return can_parse((argument_t *)nullptr) || can_parse((redirection_t *)nullptr);
    }

    bool can_parse(variable_assignment_t *) {
        // Do we have a variable assignment at all?
        if (!peek_token(0).may_be_variable_assignment) return false;

        // What is the token after it?
        switch (peek_type(1)) {
            case parse_token_type_string:
                // We have `a= cmd` and should treat it as a variable assignment.
                return true;
            case parse_token_type_terminate:
                // We have `a=` which is OK if we are allowing incomplete, an error otherwise.
                return allow_incomplete();
            default:
                // We have e.g. `a= >` which is an error.
                // Note that we do not produce an error here. Instead we return false so this the
                // token will be seen by allocate_populate_statement_contents.
                return false;
        }
    }

    template <parse_token_type_t... Tok>
    bool can_parse(token_t<Tok...> *tok) {
        return tok->allows_token(peek_token().type);
    }

    // Note we have specific overloads for our keyword nodes, as they need custom logic.
    bool can_parse(job_conjunction_t::decorator_t *) {
        // This is for a job conjunction like `and stuff`
        // But if it's `and --help` then we treat it as an ordinary command.
        return job_conjunction_t::decorator_t::allows_keyword(peek_token(0).keyword) &&
               !peek_token(1).is_help_argument;
    }

    bool can_parse(decorated_statement_t::decorator_t *) {
        // Here the keyword is 'command' or 'builtin' or 'exec'.
        // `command stuff` executes a command called stuff.
        // `command -n` passes the -n argument to the 'command' builtin.
        // `command` by itself is a command.
        if (!decorated_statement_t::decorator_t::allows_keyword(peek_token(0).keyword)) {
            return false;
        }
        // Is it like `command --stuff` or `command` by itself?
        auto tok1 = peek_token(1);
        return tok1.type == parse_token_type_string && !tok1.is_dash_prefix_string();
    }

    bool can_parse(keyword_t<parse_keyword_t::kw_time> *) {
        // Time keyword is only the time builtin if the next argument doesn't have a dash.
        return keyword_t<parse_keyword_t::kw_time>::allows_keyword(peek_token(0).keyword) &&
               !peek_token(1).is_dash_prefix_string();
    }

    bool can_parse(job_continuation_t *) { return peek_type() == parse_token_type_pipe; }

    bool can_parse(job_conjunction_continuation_t *) {
        auto type = peek_type();
        return type == parse_token_type_andand || type == parse_token_type_oror;
    }

    bool can_parse(andor_job_t *) {
        switch (peek_token().keyword) {
            case parse_keyword_t::kw_and:
            case parse_keyword_t::kw_or: {
                // Check that the argument to and/or is a string that's not help. Otherwise it's
                // either 'and
                // --help' or a naked 'and', and not part of this list.
                const auto &nexttok = peek_token(1);
                return nexttok.type == parse_token_type_string && !nexttok.is_help_argument;
            }
            default:
                return false;
        }
    }

    bool can_parse(elseif_clause_t *) {
        return peek_token(0).keyword == parse_keyword_t::kw_else &&
               peek_token(1).keyword == parse_keyword_t::kw_if;
    }

    bool can_parse(else_clause_t *) { return peek_token().keyword == parse_keyword_t::kw_else; }
    bool can_parse(case_item_t *) { return peek_token().keyword == parse_keyword_t::kw_case; }

    // Given that we are a list of type ListNodeType, whose contents type is ContentsNode, populate
    // as many elements as we can.
    // If exhaust_stream is set, then keep going until we get parse_token_type_terminate.
    template <type_t ListType, typename ContentsNode>
    void populate_list(list_t<ListType, ContentsNode> &list, bool exhaust_stream = false) {
        // Do not attempt to parse a list if we are unwinding.
        if (is_unwinding()) {
            assert(!exhaust_stream &&
                   "exhaust_stream should only be set at top level, and so we should not be "
                   "unwinding");
            // Mark in the list that it was unwound.
            FLOGF(ast_construction, L"%*sunwinding %ls", spaces(), "",
                  ast_type_to_string(ListType));
            assert(list.empty() && "Should be an empty list");
            return;
        }

        for (;;) {
            // If we are unwinding, then either we recover or we break the loop, dependent on the
            // loop type.
            if (is_unwinding()) {
                if (!list_type_stops_unwind(ListType)) {
                    break;
                }
                // We are going to stop unwinding.
                // Rather hackish. Just chomp until we get to a string or end node.
                for (auto type = peek_type();
                     type != parse_token_type_string && type != parse_token_type_terminate &&
                     type != parse_token_type_end;
                     type = peek_type()) {
                    parse_token_t tok = tokens_.pop();
                    ast_->extras_.errors.push_back(tok.range());
                    FLOGF(ast_construction, L"%*schomping range %u-%u", spaces(), "",
                          tok.source_start, tok.source_length);
                }
                FLOGF(ast_construction, L"%*sdone unwinding", spaces(), "");
                unwinding_ = false;
            }

            // Chomp semis and newlines.
            chomp_extras(ListType);

            // Now try parsing a node.
            if (auto node = this->try_parse<ContentsNode>()) {
                list.contents.push_back(std::move(node));
            } else if (exhaust_stream && peek_type() != parse_token_type_terminate) {
                // We aren't allowed to stop. Produce an error and keep going.
                consume_excess_token_generating_error();
            } else {
                // We either stop once we can't parse any more of this contents node, or we
                // exhausted the stream as requested.
                break;
            }
        }

        FLOGF(ast_construction, L"%*s%ls size: %lu", spaces(), "", ast_type_to_string(ListType),
              (unsigned long)list.count());
    }

    /// Allocate and populate a statement contents pointer.
    /// This must never return null.
    statement_t::contents_ptr_t allocate_populate_statement_contents() {
        // In case we get a parse error, we still need to return something non-null. Use a decorated
        // statement; all of its leaf nodes will end up unsourced.
        auto got_error = [this] {
            assert(unwinding_ && "Should have produced an error");
            return this->allocate_visit<decorated_statement_t>();
        };

        using pkt = parse_keyword_t;
        const auto &token1 = peek_token(0);
        if (token1.type == parse_token_type_terminate && allow_incomplete()) {
            // This may happen if we just have a 'time' prefix.
            // Construct a decorated statement, which will be unsourced.
            return this->allocate_visit<decorated_statement_t>();
        } else if (token1.type != parse_token_type_string) {
            // We may be unwinding already; do not produce another error.
            // For example in `true | and`.
            parse_error(token1, parse_error_generic, _(L"Expected a command, but found %ls"),
                        token1.user_presentable_description().c_str());
            return got_error();
        } else if (token1.may_be_variable_assignment) {
            // Here we have a variable assignment which we chose to not parse as a variable
            // assignment because there was no string after it.
            // Ensure we consume the token, so we don't get back here again at the same place.
            parse_error(consume_any_token(), parse_error_bare_variable_assignment, L"");
            return got_error();
        }

        // The only block-like builtin that takes any parameters is 'function'. So go to decorated
        // statements if the subsequent token looks like '--'. The logic here is subtle:
        //
        // If we are 'begin', then we expect to be invoked with no arguments.
        // If we are 'function', then we are a non-block if we are invoked with -h or --help
        // If we are anything else, we require an argument, so do the same thing if the subsequent
        // token is a statement terminator.
        if (token1.type == parse_token_type_string) {
            const auto &token2 = peek_token(1);
            // If we are a function, then look for help arguments. Otherwise, if the next token
            // looks like an option (starts with a dash), then parse it as a decorated statement.
            if (token1.keyword == pkt::kw_function && token2.is_help_argument) {
                return allocate_visit<decorated_statement_t>();
            } else if (token1.keyword != pkt::kw_function && token2.has_dash_prefix) {
                return allocate_visit<decorated_statement_t>();
            }

            // Likewise if the next token doesn't look like an argument at all. This corresponds to
            // e.g. a "naked if".
            bool naked_invocation_invokes_help =
                (token1.keyword != pkt::kw_begin && token1.keyword != pkt::kw_end);
            if (naked_invocation_invokes_help && (token2.type == parse_token_type_end ||
                                                  token2.type == parse_token_type_terminate)) {
                return allocate_visit<decorated_statement_t>();
            }
        }

        switch (token1.keyword) {
            case pkt::kw_not:
            case pkt::kw_exclam:
                return allocate_visit<not_statement_t>();
            case pkt::kw_for:
            case pkt::kw_while:
            case pkt::kw_function:
            case pkt::kw_begin:
                return allocate_visit<block_statement_t>();
            case pkt::kw_if:
                return allocate_visit<if_statement_t>();
            case pkt::kw_switch:
                return allocate_visit<switch_statement_t>();

            case pkt::kw_end:
                // 'end' is forbidden as a command.
                // For example, `if end` or `while end` will produce this error.
                // We still have to descend into the decorated statement because
                // we can't leave our pointer as null.
                parse_error(token1, parse_error_generic, _(L"Expected a command, but found %ls"),
                            token1.user_presentable_description().c_str());
                return got_error();

            default:
                return allocate_visit<decorated_statement_t>();
        }
    }

    /// Allocate and populate a block statement header.
    /// This must never return null.
    block_statement_t::header_ptr_t allocate_populate_block_header() {
        switch (peek_token().keyword) {
            case parse_keyword_t::kw_for:
                return allocate_visit<for_header_t>();
            case parse_keyword_t::kw_while:
                return allocate_visit<while_header_t>();
            case parse_keyword_t::kw_function:
                return allocate_visit<function_header_t>();
            case parse_keyword_t::kw_begin:
                return allocate_visit<begin_header_t>();
            default:
                internal_error(__FUNCTION__, L"should not have descended into block_header");
                DIE("Unreachable");
        }
    }

    template <typename AstNode>
    unique_ptr<AstNode> try_parse() {
        if (!can_parse((AstNode *)nullptr)) return nullptr;
        return allocate_visit<AstNode>();
    }

    void visit_node_field(argument_t &arg) {
        if (unsource_leaves()) {
            arg.unsourced = true;
            return;
        }
        arg.range = consume_token_type(parse_token_type_string);
    }

    void visit_node_field(variable_assignment_t &varas) {
        if (unsource_leaves()) {
            varas.unsourced = true;
            return;
        }
        if (!peek_token().may_be_variable_assignment) {
            internal_error(__FUNCTION__,
                           L"Should not have created variable_assignment_t from this token");
        }
        varas.range = consume_token_type(parse_token_type_string);
    }

    void visit_node_field(job_continuation_t &node) {
        // Special error handling to catch 'and' and 'or' in pipelines, like `true | and false`.
        const auto &tok = peek_token(1);
        if (tok.keyword == parse_keyword_t::kw_and || tok.keyword == parse_keyword_t::kw_or) {
            const wchar_t *cmdname = (tok.keyword == parse_keyword_t::kw_and ? L"and" : L"or");
            parse_error(tok, parse_error_andor_in_pipeline, EXEC_ERR_MSG, cmdname);
        }
        node.accept(*this);
    }

    // Visit branch nodes by just calling accept() to visit their fields.
    template <typename Node>
    enable_if_t<Node::Category == category_t::branch> visit_node_field(Node &node) {
        // This field is a direct embedding of an AST value.
        node.accept(*this);
    }

    template <typename Node>
    void visit_pointer_field(Node *&node) {
        // This field is a pointer embedding of an ast node.
        // Allocate and populate it.
        node = allocate_visit<Node>();
    }

    // Overload for token fields.
    template <parse_token_type_t... TokTypes>
    void visit_node_field(token_t<TokTypes...> &token) {
        if (unsource_leaves()) {
            token.unsourced = true;
            return;
        }

        if (!token.allows_token(peek_token().type)) {
            const auto &peek = peek_token();
            parse_error(peek, parse_error_generic, L"Expected %ls, but found %ls",
                        token_types_user_presentable_description({TokTypes...}).c_str(),
                        peek.user_presentable_description().c_str());
            token.unsourced = true;
            return;
        }
        parse_token_t tok = consume_any_token();
        token.type = tok.type;
        token.range = tok.range();
    }

    // Overload for keyword fields.
    template <parse_keyword_t... KWs>
    void visit_node_field(keyword_t<KWs...> &keyword) {
        if (unsource_leaves()) {
            keyword.unsourced = true;
            return;
        }

        if (!keyword.allows_keyword(peek_token().keyword)) {
            keyword.unsourced = true;
            const auto &peek = peek_token();

            // Special error reporting for keyword_t<kw_end>.
            bool specially_handled = false;
            std::array<parse_keyword_t, sizeof...(KWs)> allowed = {{KWs...}};
            if (allowed.size() == 1 && allowed[0] == parse_keyword_t::kw_end) {
                assert(!visit_stack_.empty() && "Visit stack should not be empty");
                auto p = find_block_open_keyword(visit_stack_.back());
                source_range_t kw_range = p.first;
                const wchar_t *kw_name = p.second;
                if (kw_name) {
                    this->parse_error(kw_range, parse_error_generic,
                                      L"Missing end to balance this %ls", kw_name);
                }
            }
            if (!specially_handled) {
                parse_error(peek, parse_error_generic, L"Expected %ls, but found %ls",
                            keywords_user_presentable_description({KWs...}).c_str(),
                            peek.user_presentable_description().c_str());
            }
            return;
        }
        parse_token_t tok = consume_any_token();
        keyword.kw = tok.keyword;
        keyword.range = tok.range();
    }

    // Overload for maybe_newlines
    void visit_node_field(maybe_newlines_t &nls) {
        if (unsource_leaves()) {
            nls.unsourced = true;
            return;
        }
        // TODO: it would be nice to have the start offset be the current position in the token
        // stream, even if there are no newlines.
        nls.range = {0, 0};
        while (peek_token().is_newline) {
            auto r = consume_token_type(parse_token_type_end);
            if (nls.range.length == 0) {
                nls.range = r;
            } else {
                nls.range.length = r.start + r.length - nls.range.start;
            }
        }
    }

    template <typename AstNode>
    void visit_optional_field(optional_t<AstNode> &ptr) {
        // This field is an optional node.
        ptr.contents = this->try_parse<AstNode>();
    }

    template <type_t ListNodeType, typename ContentsNode>
    void visit_list_field(list_t<ListNodeType, ContentsNode> &list) {
        // This field is an embedding of an array of (pointers to) ContentsNode.
        // Parse as many as we can.
        populate_list(list);
    }

    // We currently only have a handful of union pointer types.
    // Handle them directly.
    void visit_union_field(statement_t::contents_ptr_t &ptr) {
        ptr = this->allocate_populate_statement_contents();
        assert(ptr && "Statement contents must never be null");
    }

    void visit_union_field(argument_or_redirection_t::contents_ptr_t &ptr) {
        if (auto arg = try_parse<argument_t>()) {
            ptr.contents = std::move(arg);
        } else if (auto redir = try_parse<redirection_t>()) {
            ptr.contents = std::move(redir);
        } else {
            internal_error(__FUNCTION__, L"Unable to parse argument or redirection");
        }
        assert(ptr && "Statement contents must never be null");
    }

    void visit_union_field(block_statement_t::header_ptr_t &ptr) {
        ptr = this->allocate_populate_block_header();
        assert(ptr && "Header pointer must never be null");
    }

    void will_visit_fields_of(const node_t &node) {
        FLOGF(ast_construction, L"%*swill_visit %ls %p", spaces(), "", node.describe().c_str(),
              (const void *)&node);
        visit_stack_.push_back(&node);
    }

    void did_visit_fields_of(const node_t &node) {
        assert(!visit_stack_.empty() && visit_stack_.back() == &node &&
               "Node was not at the top of the visit stack");
        visit_stack_.pop_back();
        FLOGF(ast_construction, L"%*sdid_visit %ls %p", spaces(), "", node.describe().c_str(),
              (const void *)&node);
    }

    // The ast which we are populating.
    ast_t *const ast_;

    // Flags controlling parsing.
    parse_tree_flags_t flags_{};

    // Stream of tokens which we consume.
    token_stream_t tokens_;

    // The type which we are attempting to parse, typically job_list but may be
    // freestanding_argument_list.
    const type_t top_type_;

    // If set, we are unwinding due to error recovery.
    bool unwinding_{false};

    // A stack containing the nodes whose fields we are visiting.
    std::vector<const node_t *> visit_stack_{};

    // If non-null, populate with errors.
    parse_error_list_t *out_errors_{};
};

// Set the parent fields of all nodes in the tree rooted at \p node.
static void set_parents(const node_t *top) {
    struct parent_setter_t {
        void visit(const node_t &node) {
            const_cast<node_t &>(node).parent = parent_;
            const node_t *saved = parent_;
            parent_ = &node;
            node_visitor(*this).accept_children_of(&node);
            parent_ = saved;
        }

        const node_t *parent_{nullptr};
    };
    struct parent_setter_t ps;
    node_visitor(ps).accept(top);
}

// static
ast_t ast_t::parse_from_top(const wcstring &src, parse_tree_flags_t parse_flags,
                            parse_error_list_t *out_errors, type_t top) {
    ast_t ast;

    // Populate our ast.
    populator_t pop(&ast, src, parse_flags, top, out_errors);

    // Set all parent nodes.
    // It turns out to be more convenient to do this after the parse phase.
    set_parents(ast.top());

    return ast;
}

// static
ast_t ast_t::parse(const wcstring &src, parse_tree_flags_t flags, parse_error_list_t *out_errors) {
    return parse_from_top(src, flags, out_errors, type_t::job_list);
}

// static
ast_t ast_t::parse_argument_list(const wcstring &src, parse_tree_flags_t flags,
                                 parse_error_list_t *out_errors) {
    return parse_from_top(src, flags, out_errors, type_t::freestanding_argument_list);
}

// \return the depth of a node, i.e. number of parent links.
static int get_depth(const node_t *node) {
    int result = 0;
    for (const node_t *cursor = node->parent; cursor; cursor = cursor->parent) {
        result += 1;
    }
    return result;
}

wcstring ast_t::dump(const wcstring &orig) const {
    wcstring result;

    // Return a string that repeats "| " \p amt times.
    auto pipespace = [](int amt) {
        std::string result;
        result.reserve(amt * 2);
        for (int i = 0; i < amt; i++) result.append("! ");
        return result;
    };

    traversal_t tv = this->walk();
    while (const auto *node = tv.next()) {
        int depth = get_depth(node);
        // dot-| padding
        append_format(result, L"%s", pipespace(depth).c_str());
        if (const auto *n = node->try_as<argument_t>()) {
            append_format(result, L"argument");
            if (auto argsrc = n->try_source(orig)) {
                append_format(result, L": '%ls'", argsrc->c_str());
            }
        } else if (const auto *n = node->try_as<keyword_base_t>()) {
            append_format(result, L"keyword: %ls", keyword_description(n->kw));
        } else if (const auto *n = node->try_as<token_base_t>()) {
            wcstring desc;
            switch (n->type) {
                case parse_token_type_string:
                    desc = format_string(L"string");
                    if (auto strsource = n->try_source(orig)) {
                        append_format(desc, L": '%ls'", strsource->c_str());
                    }
                    break;
                case parse_token_type_redirection:
                    desc = L"redirection";
                    if (auto strsource = n->try_source(orig)) {
                        append_format(desc, L": '%ls'", strsource->c_str());
                    }
                    break;
                case parse_token_type_end:
                    desc = L"<;>";
                    break;
                case token_type_invalid:
                    // This may occur with errors, e.g. we expected to see a string but saw a
                    // redirection.
                    desc = L"<error>";
                    break;
                default:
                    desc = token_type_user_presentable_description(n->type);
                    break;
            }
            append_format(result, L"%ls", desc.c_str());
        } else {
            append_format(result, L"%ls", node->describe().c_str());
        }
        append_format(result, L"\n");
    }
    return result;
}
}  // namespace ast
