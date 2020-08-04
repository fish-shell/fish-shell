// Programmatic representation of fish grammar.

#ifndef FISH_AST_H
#define FISH_AST_H

#include <array>
#include <tuple>
#include <type_traits>

#include "flog.h"
#include "parse_constants.h"
#include "tokenizer.h"

namespace ast {

/**
 * This defines the fish abstract syntax tree.
 * The fish ast is a tree data structure. The nodes of the tree
 * are divided into three types:
 *
 * - leaf nodes refer to a range of source, and have no child nodes.
 * - branch nodes have ONLY child nodes, and no other fields.
 * - list nodes contain a list of some other node type (branch or leaf).
 *
 * Most clients will be interested in visiting the nodes of an ast.
 * See node_visitation_t below.
 */

struct node_t;

// Our node categories.
// Note these are not stored directly in a node; they are provided in the Category static constexpr
// variable in each node.
enum class category_t : uint8_t {
    branch,
    leaf,
    list,
};

// Declare our type enum.
// For each member of our ast, this creates an enum value.
// For example this creates `type_t::job_list`.
enum class type_t : uint8_t {
#define ELEM(T) T,
#include "ast_node_types.inc"
};

// Helper to return a string description of a type.
const wchar_t *ast_type_to_string(type_t type);

// Forward declare all AST structs.
#define ELEM(T) struct T##_t;
#include "ast_node_types.inc"

/*
 * A FieldVisitor is something which can visit the fields of an ast node.
 * This is used during ast construction.
 *
 * To trigger field visitation, use the accept() function:
 *    MyFieldVisitor v;
 *    node->accept(v);
 *
 * Example FieldVisitor:
 *
 * struct MyFieldVisitor {
 *
 *    /// will_visit (did_visit) is called before (after) a node's fields are visited.
 *    void will_visit_fields_of(node_t &node);
 *    void did_visit_fields_of(node_t &node);
 *
 *    /// These are invoked with the concrete type of each node,
 *    /// so they may be overloaded to distinguish node types.
 *    /// Example:
 *    void will_visit_fields_of(job_t &job);
 *
 *    /// The visitor needs to be prepared for the following four field types.
 *    /// Naturally the vistor may overload visit_field to carve this
 *    /// arbitrarily finely.
 *
 *    /// A field may be a "direct embedding" of a node.
 *    /// That is, an ast node may have another node as a member.
 *    template <typename Node>
 *    void visit_node_field(Node &node);

 *    /// A field may be a list_t of (pointers to) some other node type.
 *    template <type_t List, typename Node>
 *    void visit_list_field(list_t<List, Node> &list);
 *
 *    /// A field may be a unique_ptr to another node.
 *    /// Every such pointer must be non-null after construction.
 *    template <typename Node>
 *    void visit_pointer_field(std::unique_ptr<Node> &ptr);
 *
 *    /// A field may be optional, meaning it may or may not exist.
 *    template <typename Node>
 *    void visit_optional_field(optional_t<NodeT> &opt);
 *
 *    /// A field may be a union pointer, meaning it points to one of
 *    /// a fixed set of node types. A union pointer is never null
 *    /// after construction.
 *    template <typename... Nodes>
 *    void visit_union_field(union_ptr_t<Nodes...> &union_ptr);
 * };
 */

// Our node base type is not virtual, so we must not invoke its destructor directly.
// If you want to delete a node and don't know its concrete type, use this deleter type.
struct node_deleter_t {
    void operator()(node_t *node);
};
using node_unique_ptr_t = std::unique_ptr<node_t, node_deleter_t>;

// A union pointer field is a pointer to one of a fixed set of node types.
// It is never null after construction.
template <typename... Nodes>
struct union_ptr_t {
    node_unique_ptr_t contents{};

    /// \return a pointer to the node contents.
    const node_t *get() const {
        assert(contents && "Null pointer");
        return contents.get();
    }

    /// \return whether we have non-null contents.
    explicit operator bool() const { return contents != nullptr; }

    const node_t *operator->() const { return get(); }

    union_ptr_t() = default;

    // Allow setting a typed unique pointer.
    template <typename Node>
    inline void operator=(std::unique_ptr<Node> n);

    // Construct from a typed unique pointer.
    template <typename Node>
    inline union_ptr_t(std::unique_ptr<Node> n);
};

// A pointer to something, or nullptr if not present.
template <typename AstNode>
struct optional_t {
    std::unique_ptr<AstNode> contents{};

    explicit operator bool() const { return contents != nullptr; }

    AstNode *operator->() const {
        assert(contents && "Null pointer");
        return contents.get();
    }

    const AstNode &operator*() const {
        assert(contents && "Null pointer");
        return *contents;
    }

    bool has_value() const { return contents != nullptr; }
};

namespace template_goo {

// void if B is true, SFINAE'd away otherwise.
template <bool B>
using only_if_t = typename std::enable_if<B>::type;

template <typename FieldVisitor, typename Field>
only_if_t<Field::Category != category_t::list> visit_1_field(FieldVisitor &v, Field &field) {
    v.visit_node_field(field);
}

template <typename FieldVisitor, typename Field>
only_if_t<Field::Category == category_t::list> visit_1_field(FieldVisitor &v, Field &field) {
    v.visit_list_field(field);
}

template <typename FieldVisitor, typename Field>
void visit_1_field(FieldVisitor &v, Field *&field) {
    v.visit_pointer_field(field);
}

template <typename FieldVisitor, typename Field>
void visit_1_field(FieldVisitor &v, optional_t<Field> &field) {
    v.visit_optional_field(field);
}

template <typename FieldVisitor, typename... Nodes>
void visit_1_field(FieldVisitor &v, union_ptr_t<Nodes...> &field) {
    v.visit_union_field(field);
}

// Call the field visit methods on visitor \p v passing field \p field.
template <typename FieldVisitor, typename Field>
void accept_field_visitor(FieldVisitor &v, bool /*reverse*/, Field &field) {
    visit_1_field(v, field);
}

// Call visit_field on visitor \p v, for the field \p field and also \p rest.
template <typename FieldVisitor, typename Field, typename... Rest>
void accept_field_visitor(FieldVisitor &v, bool reverse, Field &field, Rest &... rest) {
    if (!reverse) visit_1_field(v, field);
    accept_field_visitor<FieldVisitor, Rest...>(v, reverse, rest...);
    if (reverse) visit_1_field(v, field);
}

}  // namespace template_goo

#define FIELDS(...)                                                         \
    template <typename FieldVisitor>                                        \
    void accept(FieldVisitor &visitor, bool reversed = false) {             \
        visitor.will_visit_fields_of(*this);                                \
        template_goo::accept_field_visitor(visitor, reversed, __VA_ARGS__); \
        visitor.did_visit_fields_of(*this);                                 \
    }

/// node_t is the base node of all AST nodes.
/// It is not a template: it is possible to work concretely with this type.
struct node_t {
    /// The parent node, or null if this is root.
    const node_t *parent{nullptr};

    /// The type of this node.
    const type_t type;

    /// The category of this node.
    const category_t category;

    constexpr explicit node_t(type_t t, category_t c) : type(t), category(c) {}

    /// Disallow copying, etc.
    node_t(const node_t &) = delete;
    node_t(node_t &&) = delete;
    void operator=(const node_t &) = delete;
    void operator=(node_t &&) = delete;

    /// Cast to a concrete node type, aborting on failure.
    /// Example usage:
    ///   if (node->type == type_t::job_list) node->as<job_list_t>()->...
    template <typename To>
    To *as() {
        assert(this->type == To::AstType && "Invalid type conversion");
        return static_cast<To *>(this);
    }

    template <typename To>
    const To *as() const {
        assert(this->type == To::AstType && "Invalid type conversion");
        return static_cast<const To *>(this);
    }

    /// Try casting to a concrete node type, except returns nullptr on failure.
    /// Example ussage:
    ///     if (const auto *job_list = node->try_as<job_list_t>()) job_list->...
    template <typename To>
    To *try_as() {
        if (this->type == To::AstType) return as<To>();
        return nullptr;
    }

    template <typename To>
    const To *try_as() const {
        if (this->type == To::AstType) return as<To>();
        return nullptr;
    }

    /// Base accept() function which trampolines to overriding implementations for each node type.
    /// This may be used when you don't know what the type of a particular node is.
    template <typename FieldVisitor>
    void base_accept(FieldVisitor &v, bool reverse = false);

    /// \return a helpful string description of this node.
    wcstring describe() const;

    /// \return the source range for this node, or none if unsourced.
    /// This may return none if the parse was incomplete or had an error.
    maybe_t<source_range_t> try_source_range() const;

    /// \return the source range for this node, or an empty range {0, 0} if unsourced.
    source_range_t source_range() const {
        if (auto r = try_source_range()) return *r;
        return source_range_t{0, 0};
    }

    /// \return the source code for this node, or none if unsourced.
    maybe_t<wcstring> try_source(const wcstring &orig) const {
        if (auto r = try_source_range()) return orig.substr(r->start, r->length);
        return none();
    }

    /// \return the source code for this node, or an empty string if unsourced.
    wcstring source(const wcstring &orig) const {
        wcstring res{};
        if (auto s = try_source(orig)) res = s.acquire();
        return res;
    }

    /// \return the source code for this node, or an empty string if unsourced.
    /// This uses \p storage to reduce allocations.
    const wcstring &source(const wcstring &orig, wcstring *storage) const {
        if (auto r = try_source_range()) {
            storage->assign(orig, r->start, r->length);
        } else {
            storage->clear();
        }
        return *storage;
    }

   protected:
    // We are NOT a virtual class - we have no vtable or virtual methods and our destructor is not
    // virtual, so as to keep the size down. Only typed nodes should invoke the destructor.
    // Use node_deleter_t to delete an untyped node.
    ~node_t() = default;
};

// Base class for all "branch" nodes: nodes with at least one ast child.
template <type_t Type>
struct branch_t : public node_t {
    static constexpr type_t AstType = Type;
    static constexpr category_t Category = category_t::branch;

    branch_t() : node_t(Type, Category) {}
};

// Base class for all "leaf" nodes: nodes with no ast children.
// It declares an empty visit method to avoid requiring the CHILDREN macro.
template <type_t Type>
struct leaf_t : public node_t {
    static constexpr type_t AstType = Type;
    static constexpr category_t Category = category_t::leaf;

    // Whether this node is "unsourced." This happens if for whatever reason we are unable to parse
    // the node, either because we had a parse error and recovered, or because we accepted
    // incomplete and the token stream was exhausted.
    bool unsourced{false};

    // The source range.
    source_range_t range{0, 0};

    // Convenience helper to return whether we are not unsourced.
    bool has_source() const { return !unsourced; }

    template <typename FieldVisitor>
    void accept(FieldVisitor &visitor, bool /* reverse */ = false) {
        visitor.will_visit_fields_of(*this);
        visitor.did_visit_fields_of(*this);
    }

    leaf_t() : node_t(Type, Category) {}
};

// A simple fixed-size array, possibly empty.
template <type_t ListType, typename ContentsNode>
struct list_t : public node_t {
    static constexpr type_t AstType = ListType;
    static constexpr category_t Category = category_t::list;

    // A list wraps a "contents pointer" which is just a unique_ptr that converts to a reference.
    // This enables more natural iteration:
    //    for (const argument_t &arg : argument_list) ...
    struct contents_ptr_t {
        std::unique_ptr<ContentsNode> ptr{};

        void operator=(std::unique_ptr<ContentsNode> p) { ptr = std::move(p); }

        const ContentsNode *get() const {
            assert(ptr && "Null pointer");
            return ptr.get();
        }

        /* implicit */ operator const ContentsNode &() const { return *get(); }
    };

    // We use a new[]-allocated array to store our contents pointers, to reduce size.
    // This would be a nice use case for std::dynarray.
    uint32_t length{0};
    const contents_ptr_t *contents{};

    /// \return a node at a given index, or nullptr if out of range.
    const ContentsNode *at(size_t idx, bool reverse = false) const {
        if (idx >= count()) return nullptr;
        return contents[reverse ? count() - idx - 1 : idx].get();
    }

    /// \return our count.
    size_t count() const { return length; }

    /// \return whether we are empty.
    bool empty() const { return length == 0; }

    /// Iteration support.
    using iterator = const contents_ptr_t *;
    iterator begin() const { return contents; }
    iterator end() const { return contents + length; }

    // list types pretend their child nodes are direct embeddings.
    // This isn't used during AST construction because we need to construct the list.
    // It is used by node_visitation_t.
    template <typename FieldVisitor>
    void accept(FieldVisitor &visitor, bool reverse = false) {
        visitor.will_visit_fields_of(*this);
        for (size_t i = 0; i < count(); i++) visitor.visit_node_field(*this->at(i, reverse));
        visitor.did_visit_fields_of(*this);
    }

    list_t() : node_t(ListType, Category) {}
    ~list_t() { delete[] contents; }

    // Disallow moving as we own a raw pointer.
    list_t(list_t &&) = delete;
    void operator=(list_t &&) = delete;
};

// Fully define all list types, as they are very uniform.
// This is where types like job_list_t come from.
#define ELEM(T)
#define ELEMLIST(ListT, ContentsT) \
    struct ListT##_t final : public list_t<type_t::ListT, ContentsT##_t> {};
#include "ast_node_types.inc"

struct keyword_base_t : public leaf_t<type_t::keyword_base> {
    // The keyword which was parsed.
    parse_keyword_t kw;
};

// A keyword node is a node which contains a keyword, which must be one of the provided values.
template <parse_keyword_t... KWs>
struct keyword_t final : public keyword_base_t {
    static bool allows_keyword(parse_keyword_t);
};

struct token_base_t : public leaf_t<type_t::token_base> {
    // The token type which was parsed.
    parse_token_type_t type{parse_token_type_t::invalid};
};

// A token node is a node which contains a token, which must be one of the provided values.
template <parse_token_type_t... Toks>
struct token_t final : public token_base_t {
    /// \return whether a token type is allowed in this token_t, i.e. is a member of our Toks list.
    static bool allows_token(parse_token_type_t);
};

// Zero or more newlines.
struct maybe_newlines_t final : public leaf_t<type_t::maybe_newlines> {};

// A single newline or semicolon, terminating statements.
// Note this is not a separate type, it is just a convenience typedef.
using semi_nl_t = token_t<parse_token_type_t::end>;

// Convenience typedef for string nodes.
using string_t = token_t<parse_token_type_t::string>;

// An argument is just a node whose source range determines its contents.
// This is a separate type because it is sometimes useful to find all arguments.
struct argument_t final : public leaf_t<type_t::argument> {};

// A redirection has an operator like > or 2>, and a target like /dev/null or &1.
// Note that pipes are not redirections.
struct redirection_t final : public branch_t<type_t::redirection> {
    token_t<parse_token_type_t::redirection> oper;
    string_t target;

    FIELDS(oper, target)
};

// A variable_assignment_t contains a source range like FOO=bar.
struct variable_assignment_t final : public leaf_t<type_t::variable_assignment> {};

// An argument or redirection holds either an argument or redirection.
struct argument_or_redirection_t final : public branch_t<type_t::argument_or_redirection> {
    using contents_ptr_t = union_ptr_t<argument_t, redirection_t>;
    contents_ptr_t contents{};

    /// \return whether this represents an argument.
    bool is_argument() const { return contents->type == type_t::argument; }

    /// \return whether this represents a redirection
    bool is_redirection() const { return contents->type == type_t::redirection; }

    /// \return this as an argument, assuming it wraps one.
    const argument_t &argument() const {
        assert(is_argument() && "Is not an argument");
        return *this->contents.contents->as<argument_t>();
    }

    /// \return this as an argument, assuming it wraps one.
    const redirection_t &redirection() const {
        assert(is_redirection() && "Is not a redirection");
        return *this->contents.contents->as<redirection_t>();
    }

    FIELDS(contents);
};

// A statement is a normal command, or an if / while / etc
struct statement_t final : public branch_t<type_t::statement> {
    using contents_ptr_t = union_ptr_t<not_statement_t, block_statement_t, if_statement_t,
                                       switch_statement_t, decorated_statement_t>;
    contents_ptr_t contents{};

    FIELDS(contents)
};

// A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases
// like if statements, where we require a command).
struct job_t final : public branch_t<type_t::job> {
    // Maybe the time keyword.
    optional_t<keyword_t<parse_keyword_t::kw_time>> time;

    // A (possibly empty) list of variable assignments.
    variable_assignment_list_t variables;

    // The statement.
    statement_t statement;

    // Piped remainder.
    job_continuation_list_t continuation;

    // Maybe backgrounded.
    optional_t<token_t<parse_token_type_t::background>> bg;

    FIELDS(time, variables, statement, continuation, bg)
};

// A job_conjunction is a job followed by a && or || continuations.
struct job_conjunction_t final : public branch_t<type_t::job_conjunction> {
    // The job conjunction decorator.
    using decorator_t = keyword_t<parse_keyword_t::kw_and, parse_keyword_t::kw_or>;
    optional_t<decorator_t> decorator{};

    // The job itself.
    job_t job;

    // The rest of the job conjunction, with && or ||s.
    job_conjunction_continuation_list_t continuations;

    // A terminating semicolon or newline.
    // This is marked optional because it may not be present, for example the command `echo foo` may
    // not have a terminating newline. It will only fail to be present if we ran out of tokens.
    optional_t<semi_nl_t> semi_nl;

    FIELDS(decorator, job, continuations, semi_nl)
};

struct for_header_t final : public branch_t<type_t::for_header> {
    // 'for'
    keyword_t<parse_keyword_t::kw_for> kw_for;

    // var_name
    string_t var_name;

    // 'in'
    keyword_t<parse_keyword_t::kw_in> kw_in;

    // list of arguments
    argument_list_t args;

    // newline or semicolon
    semi_nl_t semi_nl;

    FIELDS(kw_for, var_name, kw_in, args, semi_nl)
};

struct while_header_t final : public branch_t<type_t::while_header> {
    // 'while'
    keyword_t<parse_keyword_t::kw_while> kw_while;

    job_conjunction_t condition{};
    andor_job_list_t andor_tail{};

    FIELDS(kw_while, condition, andor_tail)
};

struct function_header_t final : public branch_t<type_t::function_header> {
    // functions require at least one argument.
    keyword_t<parse_keyword_t::kw_function> kw_function;
    argument_t first_arg;
    argument_list_t args;
    semi_nl_t semi_nl;

    FIELDS(kw_function, first_arg, args, semi_nl)
};

struct begin_header_t final : public branch_t<type_t::begin_header> {
    keyword_t<parse_keyword_t::kw_begin> kw_begin;

    // Note that 'begin' does NOT require a semi or nl afterwards.
    // This is valid: begin echo hi; end
    optional_t<semi_nl_t> semi_nl;

    FIELDS(kw_begin, semi_nl)
};

struct block_statement_t final : public branch_t<type_t::block_statement> {
    // A header like for, while, etc.
    using header_ptr_t =
        union_ptr_t<for_header_t, while_header_t, function_header_t, begin_header_t>;
    header_ptr_t header;

    // List of jobs in this block.
    job_list_t jobs;

    // The 'end' node.
    keyword_t<parse_keyword_t::kw_end> end;

    // Arguments and redirections associated with the block.
    argument_or_redirection_list_t args_or_redirs;

    FIELDS(header, jobs, end, args_or_redirs)
};

// Represents an 'if', either as the first part of an if statement or after an 'else'.
struct if_clause_t final : public branch_t<type_t::if_clause> {
    // The 'if' keyword.
    keyword_t<parse_keyword_t::kw_if> kw_if;

    // The 'if' condition.
    job_conjunction_t condition{};

    // 'and/or' tail.
    andor_job_list_t andor_tail{};

    // The body to execute if the condition is true.
    job_list_t body;

    FIELDS(kw_if, condition, andor_tail, body)
};

struct elseif_clause_t final : public branch_t<type_t::elseif_clause> {
    // The 'else' keyword.
    keyword_t<parse_keyword_t::kw_else> kw_else;

    // The 'if' clause following it.
    if_clause_t if_clause;

    FIELDS(kw_else, if_clause)
};

struct else_clause_t final : public branch_t<type_t::else_clause> {
    // else ; body
    keyword_t<parse_keyword_t::kw_else> kw_else;
    semi_nl_t semi_nl;
    job_list_t body;

    FIELDS(kw_else, semi_nl, body)
};

struct if_statement_t final : public branch_t<type_t::if_statement> {
    // if part
    if_clause_t if_clause;

    // else if list
    elseif_clause_list_t elseif_clauses;

    // else part
    optional_t<else_clause_t> else_clause;

    // literal end
    keyword_t<parse_keyword_t::kw_end> end;

    // block args / redirs
    argument_or_redirection_list_t args_or_redirs;

    FIELDS(if_clause, elseif_clauses, else_clause, end, args_or_redirs)
};

struct case_item_t final : public branch_t<type_t::case_item> {
    // case <arguments> ; body
    keyword_t<parse_keyword_t::kw_case> kw_case;
    argument_list_t arguments;
    semi_nl_t semi_nl;
    job_list_t body;
    FIELDS(kw_case, arguments, semi_nl, body)
};

struct switch_statement_t final : public branch_t<type_t::switch_statement> {
    // switch <argument> ; body ; end args_redirs
    keyword_t<parse_keyword_t::kw_switch> kw_switch;
    argument_t argument;
    semi_nl_t semi_nl;
    case_item_list_t cases;
    keyword_t<parse_keyword_t::kw_end> end;
    argument_or_redirection_list_t args_or_redirs;

    FIELDS(kw_switch, argument, semi_nl, cases, end, args_or_redirs)
};

// A decorated_statement is a command with a list of arguments_or_redirections, possibly with
// "builtin" or "command" or "exec"
struct decorated_statement_t final : public branch_t<type_t::decorated_statement> {
    // An optional decoration (command, builtin, exec, etc).
    using pk = parse_keyword_t;
    using decorator_t = keyword_t<pk::kw_command, pk::kw_builtin, pk::kw_exec>;
    optional_t<decorator_t> opt_decoration;

    // Command to run.
    string_t command;

    // Args and redirs
    argument_or_redirection_list_t args_or_redirs;

    // Helper to return the decoration.
    statement_decoration_t decoration() const;

    FIELDS(opt_decoration, command, args_or_redirs)
};

// A not statement like `not true` or `! true`
struct not_statement_t final : public branch_t<type_t::not_statement> {
    // Keyword, either not or exclam.
    keyword_t<parse_keyword_t::kw_not, parse_keyword_t::kw_exclam> kw;

    variable_assignment_list_t variables;
    optional_t<keyword_t<parse_keyword_t::kw_time>> time{};
    statement_t contents{};

    FIELDS(kw, variables, time, contents)
};

struct job_continuation_t final : public branch_t<type_t::job_continuation> {
    token_t<parse_token_type_t::pipe> pipe;
    maybe_newlines_t newlines;
    variable_assignment_list_t variables;
    statement_t statement;

    FIELDS(pipe, newlines, variables, statement)
};

struct job_conjunction_continuation_t final
    : public branch_t<type_t::job_conjunction_continuation> {
    // The && or || token.
    token_t<parse_token_type_t::andand, parse_token_type_t::oror> conjunction;
    maybe_newlines_t newlines;

    // The job itself.
    job_t job;

    FIELDS(conjunction, newlines, job)
};

// An andor_job just wraps a job, but requires that the job have an 'and' or 'or' job_decorator.
// Note this is only used for andor_job_list; jobs that are not part of an andor_job_list are not
// instances of this.
struct andor_job_t final : public branch_t<type_t::andor_job> {
    job_conjunction_t job;

    FIELDS(job)
};

// A freestanding_argument_list is equivalent to a normal argument list, except it may contain
// TOK_END (newlines, and even semicolons, for historical reasons).
// In practice the tok_ends are ignored by fish code so we do not bother to store them.
struct freestanding_argument_list_t final : public branch_t<type_t::freestanding_argument_list> {
    argument_list_t arguments;
    FIELDS(arguments)
};

template <typename FieldVisitor>
void node_t::base_accept(FieldVisitor &v, bool reverse) {
    switch (this->type) {
#define ELEM(T)                                \
    case type_t::T:                            \
        this->as<T##_t>()->accept(v, reverse); \
        break;

#include "ast_node_types.inc"
    }
}

// static
template <parse_token_type_t... Toks>
bool token_t<Toks...>::allows_token(parse_token_type_t type) {
    for (parse_token_type_t t : {Toks...}) {
        if (type == t) return true;
    }
    return false;
}

// static
template <parse_keyword_t... KWs>
bool keyword_t<KWs...>::allows_keyword(parse_keyword_t kw) {
    for (parse_keyword_t k : {KWs...}) {
        if (k == kw) return true;
    }
    return false;
}

namespace template_goo {
/// \return true if type Type is in the Candidates list.
template <typename Type>
constexpr bool type_in_list() {
    return false;
}

template <typename Type, typename Candidate, typename... Rest>
constexpr bool type_in_list() {
    return std::is_same<Type, Candidate>::value || type_in_list<Type, Rest...>();
}
}  // namespace template_goo

template <typename... Nodes>
template <typename Node>
void union_ptr_t<Nodes...>::operator=(std::unique_ptr<Node> n) {
    static_assert(template_goo::type_in_list<Node, Nodes...>(),
                  "Cannot construct from this node type");
    contents.reset(n.release());
}

template <typename... Nodes>
template <typename Node>
union_ptr_t<Nodes...>::union_ptr_t(std::unique_ptr<Node> n) : contents(n.release()) {
    static_assert(template_goo::type_in_list<Node, Nodes...>(),
                  "Cannot construct from this node type");
}

/**
 * A node visitor is like a field visitor, but adapted to only visit actual nodes, as const
 * references. It calls the visit() function of its visitor with a const reference to each node
 * found under a given node.
 *
 * Example:
 * struct MyNodeVisitor {
 *    template <typename Node>
 *    void visit(const Node &n) {...}
 * };
 */
template <typename NodeVisitor>
class node_visitation_t {
   public:
    explicit node_visitation_t(NodeVisitor &v, bool reverse = false) : v_(v), reverse_(reverse) {}

    // Visit the (direct) child nodes of a given node.
    template <typename Node>
    void accept_children_of(const Node &n) {
        // We play fast and loose with const to avoid having to duplicate our FIELDS macros.
        const_cast<Node &>(n).accept(*this, reverse_);
    }

    // Visit the (direct) child nodes of a given node.
    void accept_children_of(const node_t *n) {
        const_cast<node_t *>(n)->base_accept(*this, reverse_);
    }

    // Invoke visit() on our visitor for a given node, resolving that node's type.
    void accept(const node_t *n) {
        assert(n && "Node should not be null");
        switch (n->type) {
#define ELEM(T)                      \
    case type_t::T:                  \
        v_.visit(*(n->as<T##_t>())); \
        break;
#include "ast_node_types.inc"
        }
    }

    // Here is our field visit implementations which adapt to the node visiting.

    // Direct embeddings.
    template <typename Node>
    void visit_node_field(const Node &node) {
        v_.visit(node);
    }

    // Pointer embeddings.
    template <typename Node>
    void visit_pointer_field(const Node *ptr) {
        v_.visit(*ptr);
    }

    // List embeddings.
    template <typename List>
    void visit_list_field(const List &list) {
        v_.visit(list);
    }

    // Optional pointers get visited if not null.
    template <typename Node>
    void visit_optional_field(optional_t<Node> &node) {
        if (node.contents) v_.visit(*node.contents);
    }

    // Define our custom implementations of non-node fields.
    // Union pointers just dispatch to the generic one.
    template <typename... Types>
    void visit_union_field(union_ptr_t<Types...> &ptr) {
        assert(ptr && "Should not have null ptr");
        this->accept(ptr.contents.get());
    }

    void will_visit_fields_of(node_t &) {}
    void did_visit_fields_of(node_t &) {}

    node_visitation_t(node_visitation_t &&) = default;

    // We cannot be copied.
    node_visitation_t(const node_visitation_t &) = delete;
    void operator=(const node_visitation_t &) = delete;
    void operator=(node_visitation_t &&) = delete;

   private:
    // Our adapted visitor.
    NodeVisitor &v_;

    // Whether to iterate in reverse order.
    const bool reverse_;
};

// Type-deducing helper.
template <typename NodeVisitor>
node_visitation_t<NodeVisitor> node_visitor(NodeVisitor &nv, bool reverse = false) {
    return node_visitation_t<NodeVisitor>(nv, reverse);
}

// A way to visit nodes iteratively.
// This is pre-order. Each node is visited before its children.
// Example:
//    traversal_t tv(start);
//    while (const node_t *node = tv.next()) {...}
class traversal_t {
   public:
    // Construct starting with a node
    traversal_t(const node_t *n) {
        assert(n && "Should not have null node");
        push(n);
    }

    // \return the next node, or nullptr if exhausted.
    const node_t *next() {
        if (stack_.empty()) return nullptr;
        const node_t *node = stack_.back();
        stack_.pop_back();

        // We want to visit in reverse order so the first child ends up on top of the stack.
        node_visitor(*this, true /* reverse */).accept_children_of(node);
        return node;
    }

   private:
    // Callback for node_visitation_t.
    void visit(const node_t &node) { push(&node); }

    // Construct an empty visitor, used for iterator support.
    traversal_t() = default;

    // \return whether we are finished visiting.
    bool finished() const { return stack_.empty(); }

    // Append a node.
    void push(const node_t *n) {
        assert(n && "Should not push null node");
        stack_.push_back(n);
    }

    // Stack of nodes.
    std::vector<const node_t *> stack_{};

    friend class ast_t;
    friend class node_visitation_t<traversal_t>;
};

/// The ast type itself.
class ast_t {
   public:
    using source_range_list_t = std::vector<source_range_t>;

    /// Construct an ast by parsing \p src as a job list.
    /// The ast attempts to produce \p type as the result.
    /// \p type may only be job_list or freestanding_argument_list.
    static ast_t parse(const wcstring &src, parse_tree_flags_t flags = parse_flag_none,
                       parse_error_list_t *out_errors = nullptr);

    /// Like parse(), but constructs a freestanding_argument_list.
    static ast_t parse_argument_list(const wcstring &src,
                                     parse_tree_flags_t flags = parse_flag_none,
                                     parse_error_list_t *out_errors = nullptr);

    /// \return a traversal, allowing iteration over the nodes.
    traversal_t walk() const { return traversal_t{top()}; }

    /// \return the top node. This has the type requested in the 'parse' method.
    const node_t *top() const { return top_.get(); }

    /// \return whether any errors were encountered during parsing.
    bool errored() const { return any_error_; }

    /// \return a textual representation of the tree.
    /// Pass the original source as \p orig.
    wcstring dump(const wcstring &orig) const;

    /// Extra source ranges.
    /// These are only generated if the corresponding flags are set.
    struct extras_t {
        /// Set of comments, sorted by offset.
        source_range_list_t comments;

        /// Set of semicolons, sorted by offset.
        source_range_list_t semis;

        /// Set of error ranges, sorted by offset.
        source_range_list_t errors;
    };

    /// Access the set of extraneous source ranges.
    const extras_t &extras() const { return extras_; }

    /// Iterator support.
    class iterator {
       public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = void;
        using value_type = node_t;
        using pointer = const node_t *;
        using reference = const node_t &;

        bool operator==(const iterator &rhs) { return current_ == rhs.current_; }
        bool operator!=(const iterator &rhs) { return !(*this == rhs); }

        iterator &operator++() {
            current_ = v_.next();
            return *this;
        }

        const node_t &operator*() const { return *current_; }

       private:
        explicit iterator(const node_t *start) : v_(start), current_(v_.next()) {}
        iterator() = default;

        traversal_t v_{};
        const node_t *current_{};
        friend ast_t;
    };

    iterator begin() const { return iterator{top()}; }
    iterator end() const { return iterator{}; }

    ast_t(ast_t &&) = default;
    ast_t &operator=(ast_t &&) = default;
    ast_t(const ast_t &) = delete;
    void operator=(const ast_t &) = delete;

   private:
    ast_t() = default;

    // Shared parsing code that takes the top type.
    static ast_t parse_from_top(const wcstring &src, parse_tree_flags_t parse_flags,
                                parse_error_list_t *out_errors, type_t top);

    // The top node.
    // Its type depends on what was requested to parse.
    node_unique_ptr_t top_{};

    /// Whether any errors were encountered during parsing.
    bool any_error_{false};

    /// Extra fields.
    extras_t extras_{};

    class populator_t;
    friend populator_t;
};

}  // namespace ast
#endif  // FISH_AST_H
