// Type-safe access to fish parse trees.
#ifndef FISH_TNODE_H
#define FISH_TNODE_H

#include "parse_grammar.h"
#include "parse_tree.h"

struct source_range_t {
    uint32_t start;
    uint32_t length;
};

// Check if a child type is possible for a parent type at a given index.
template <typename Parent, typename Child, size_t Index>
constexpr bool child_type_possible_at_index() {
    return Parent::template type_possible<Child, Index>();
}

// Check if a child type is possible for a parent type at any index.
// The number of cases here should match MAX_PRODUCTION_LENGTH.
template <typename Parent, typename Child>
constexpr bool child_type_possible() {
    return child_type_possible_at_index<Parent, Child, 0>() ||
           child_type_possible_at_index<Parent, Child, 1>() ||
           child_type_possible_at_index<Parent, Child, 2>() ||
           child_type_possible_at_index<Parent, Child, 3>() ||
           child_type_possible_at_index<Parent, Child, 4>() ||
           child_type_possible_at_index<Parent, Child, 5>();
}

/// tnode_t ("typed node") is type-safe access to a parse_tree. A tnode_t holds both a pointer to a
/// parse_node_tree_t and a pointer to a parse_node_t. (Note that the parse_node_tree_t is unowned;
/// the caller must ensure that the tnode does not outlive the tree.
///
/// tnode_t is a lightweight value-type class. It ought to be passed by value. A tnode_t may also be
/// "missing", associated with a null parse_node_t pointer. operator bool() may be used to check if
/// a tnode_t is misisng.
///
/// A tnode_t is parametrized by a grammar element, and uses the fish grammar to statically
/// type-check accesses to children and parents. Any particular tnode either corresponds to a
/// sequence (a single child) or an alternation (multiple possible children). A sequence may have
/// its children accessed directly via child(), which is templated on the index  (and returns a
/// tnode of the proper type). Alternations may be disambiguated via try_get_child(), which returns
/// an empty child if the child has the wrong type, or require_get_child() which aborts if the child
/// has the wrong type.
template <typename Type>
class tnode_t {
    /// The tree containing our node.
    const parse_node_tree_t *tree = nullptr;

    /// The node in the tree
    const parse_node_t *nodeptr = nullptr;

    // Helper to get a child type at a given index.
    template <class Element, uint32_t Index>
    using child_at = typename std::tuple_element<Index, typename Element::type_tuple>::type;

   public:
    tnode_t() = default;

    tnode_t(const parse_node_tree_t *t, const parse_node_t *n) : tree(t), nodeptr(n) {
        assert(t && "tree cannot be null in this constructor");
        assert((!n || n->type == Type::token) && "node has wrong type");
    }

    // Try to create a tnode from the given tree and parse node.
    // Returns an empty node if the parse node is null, or has the wrong type.
    static tnode_t try_create(const parse_node_tree_t *tree, const parse_node_t *node) {
        assert(tree && "tree cannot be null");
        return tnode_t(tree, node && node->type == Type::token ? node : nullptr);
    }

    /// Temporary conversion to parse_node_t to assist in migration.
    /* implicit */ operator const parse_node_t &() const {
        assert(nodeptr && "Empty tnode_t");
        return *nodeptr;
    }

    /* implicit */ operator const parse_node_t *() const { return nodeptr; }

    /// \return the underlying (type-erased) node.
    const parse_node_t *node() const { return nodeptr; }

    /// Check whether we're populated.
    explicit operator bool() const { return nodeptr != nullptr; }

    bool operator==(const tnode_t &rhs) const { return tree == rhs.tree && nodeptr == rhs.nodeptr; }

    bool operator!=(const tnode_t &rhs) const { return !(*this == rhs); }

    // Helper to return whether the given tree is the same as ours.
    bool matches_node_tree(const parse_node_tree_t &t) const { return &t == tree; }

    const parse_node_tree_t *get_tree() const { return tree; }

    bool has_source() const { return nodeptr && nodeptr->has_source(); }

    // return the tag, or 0 if missing.
    parse_node_tag_t tag() const { return nodeptr ? nodeptr->tag : 0; }

    // return the number of children, or 0 if missing.
    uint8_t child_count() const { return nodeptr ? nodeptr->child_count : 0; }

    maybe_t<source_range_t> source_range() const {
        if (nodeptr->source_start == NODE_OFFSET_INVALID) return none();
        return source_range_t{nodeptr->source_start, nodeptr->source_length};
    }

    wcstring get_source(const wcstring &str) const {
        if (!nodeptr) {
            return L"";
        }
        return nodeptr->get_source(str);
    }

    bool location_in_or_at_end_of_source_range(size_t loc) const {
        return nodeptr && nodeptr->location_in_or_at_end_of_source_range(loc);
    }

    static tnode_t find_node_matching_source_location(const parse_node_tree_t *tree,
                                                      size_t source_loc,
                                                      const parse_node_t *parent) {
        assert(tree && "null tree");
        return tnode_t{tree,
                       tree->find_node_matching_source_location(Type::token, source_loc, parent)};
    }

    /// Type-safe access to a child at the given index.
    template <node_offset_t Index>
    tnode_t<child_at<Type, Index>> child() const {
        using child_type = child_at<Type, Index>;
        const parse_node_t *child = nullptr;
        if (nodeptr) child = tree->get_child(*nodeptr, Index, child_type::token);
        return tnode_t<child_type>{tree, child};
    }

    /// Return a parse_node_t for a child.
    /// This is used to disambiguate alts.
    template <node_offset_t Index>
    const parse_node_t &get_child_node() const {
        assert(nodeptr && "receiver is missing in get_child_node");
        return *tree->get_child(*nodeptr, Index);
    }

    /// If the child at the given index has the given type, return it; otherwise return an empty
    /// child. Note this will refuse to compile if the child type is not possible.
    /// This is used for e.g. alternations.
    template <class ChildType, node_offset_t Index>
    tnode_t<ChildType> try_get_child() const {
        static_assert(child_type_possible_at_index<Type, ChildType, Index>(),
                      "Cannot contain a child of this type");
        const parse_node_t *child = nullptr;
        if (nodeptr) child = tree->get_child(*nodeptr, Index);
        if (child && child->type == ChildType::token) return {tree, child};
        return {tree, nullptr};
    }

    /// assert that this is not empty and that the child at index Index has the given type, then
    /// return that child. Note this will refuse to compile if the child type is not possible.
    template <class ChildType, node_offset_t Index>
    tnode_t<ChildType> require_get_child() const {
        assert(nodeptr && "receiver is missing in require_get_child()");
        auto result = try_get_child<ChildType, Index>();
        assert(result && "require_get_child(): wrong child type");
        return result;
    }

    /// Find the first direct child of the given node of the given type. asserts on failure.
    template <class ChildType>
    tnode_t<ChildType> find_child() const {
        static_assert(child_type_possible<Type, ChildType>(), "Cannot have that type as a child");
        assert(nodeptr && "receiver is missing in find_child()");
        tnode_t<ChildType> result{tree, &tree->find_child(*nodeptr, ChildType::token)};
        assert(result && "cannot find child");
        return result;
    }

    /// Type-safe access to a node's parent.
    /// If the parent exists and has type ParentType, return it.
    /// Otherwise return a missing tnode.
    template <class ParentType>
    tnode_t<ParentType> try_get_parent() const {
        static_assert(child_type_possible<ParentType, Type>(), "Parent cannot have us as a child");
        if (!nodeptr) return {};
        return {tree, tree->get_parent(*nodeptr, ParentType::token)};
    }

    /// Finds all descendants (up to max_count) under this node of the given type.
    template <typename DescendantType>
    std::vector<tnode_t<DescendantType>> descendants(size_t max_count = -1) const {
        if (!nodeptr) return {};
        std::vector<tnode_t<DescendantType>> result;
        std::vector<const parse_node_t *> stack{nodeptr};
        while (!stack.empty() && result.size() < max_count) {
            const parse_node_t *node = stack.back();
            if (node->type == DescendantType::token) result.emplace_back(tree, node);
            stack.pop_back();
            node_offset_t index = node->child_count;
            while (index--) {
                stack.push_back(tree->get_child(*node, index));
            }
        }
        return result;
    }

    /// Given that we are a list type, \return the next node of some Item in some node list,
    /// adjusting 'this' to be the remainder of the list.
    /// Returns an empty item on failure.
    template <class ItemType>
    tnode_t<ItemType> next_in_list() {
        // We require that we can contain ourselves, and ItemType as well.
        static_assert(child_type_possible<Type, Type>(), "Is not a list");
        static_assert(child_type_possible<Type, ItemType>(), "Is not a list of that type");
        if (!nodeptr) return {tree, nullptr};
        const parse_node_t *next =
            tree->next_node_in_node_list(*nodeptr, ItemType::token, &nodeptr);
        return {tree, next};
    }
};

template <typename Type>
tnode_t<Type> parse_node_tree_t::find_child(const parse_node_t &parent) const {
    return tnode_t<Type>(this, &this->find_child(parent, Type::token));
}

template <typename Type>
tnode_t<Type> parse_node_tree_t::find_last_node(const parse_node_t *parent) const {
    return tnode_t<Type>(this, this->find_last_node_of_type(Type::token, parent));
}

/// Given a plain statement, get the command from the child node. Returns the command string on
/// success, none on failure.
maybe_t<wcstring> command_for_plain_statement(tnode_t<grammar::plain_statement> stmt,
                                              const wcstring &src);

/// Return the decoration for a plain statement.
parse_statement_decoration_t get_decoration(tnode_t<grammar::plain_statement> stmt);

/// Return the type for a boolean statement.
enum parse_bool_statement_type_t bool_statement_type(tnode_t<grammar::job_decorator> stmt);

enum parse_bool_statement_type_t bool_statement_type(tnode_t<grammar::job_conjunction_continuation> stmt);

/// Given a redirection, get the redirection type (or none) and target (file path, or fd).
maybe_t<redirection_type_t> redirection_type(tnode_t<grammar::redirection> redirection,
                                             const wcstring &src, int *out_fd,
                                             wcstring *out_target);

/// Return the arguments under an arguments_list or arguments_or_redirection_list
/// Do not return more than max.
using arguments_node_list_t = std::vector<tnode_t<grammar::argument>>;
arguments_node_list_t get_argument_nodes(tnode_t<grammar::argument_list>, size_t max = -1);
arguments_node_list_t get_argument_nodes(tnode_t<grammar::arguments_or_redirections_list>,
                                         size_t max = -1);

/// Return whether the given job is background because it has a & symbol.
bool job_node_is_background(tnode_t<grammar::job>);

/// If the conjunction is has a decorator (and/or), return it; otherwise return none. This only
/// considers the leading conjunction, e.g. in `and true || false` only the 'true' conjunction will
/// return 'and'.
parse_bool_statement_type_t get_decorator(tnode_t<grammar::job_conjunction>);

/// Return whether the statement is part of a pipeline.
/// This doesn't detect e.g. pipelines involving our parent's block statements.
enum class pipeline_position_t {
    none,       // not part of a pipeline
    first,      // first command in a pipeline
    subsequent  // second or further command in a pipeline
};
pipeline_position_t get_pipeline_position(tnode_t<grammar::statement> st);

/// Check whether an argument_list is a root list.
inline bool argument_list_is_root(tnode_t<grammar::argument_list> list) {
    return !list.try_get_parent<grammar::argument_list>();
}

inline bool argument_list_is_root(tnode_t<grammar::arguments_or_redirections_list> list) {
    return !list.try_get_parent<grammar::arguments_or_redirections_list>();
}

#endif
