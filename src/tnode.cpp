#include "tnode.h"

const parse_node_t *parse_node_tree_t::next_node_in_node_list(
    const parse_node_t &node_list, parse_token_type_t entry_type,
    const parse_node_t **out_list_tail) const {
    parse_token_type_t list_type = node_list.type;

    // Paranoia - it doesn't make sense for a list type to contain itself.
    assert(list_type != entry_type);

    const parse_node_t *list_cursor = &node_list;
    const parse_node_t *list_entry = nullptr;

    // Loop while we don't have an item but do have a list. Note that some nodes may contain
    // nothing; e.g. job_list contains blank lines as a production.
    while (list_entry == nullptr && list_cursor != nullptr) {
        const parse_node_t *next_cursor = nullptr;

        // Walk through the children.
        for (node_offset_t i = 0; i < list_cursor->child_count; i++) {
            const parse_node_t *child = this->get_child(*list_cursor, i);
            if (child->type == entry_type) {
                // This is the list entry.
                list_entry = child;
            } else if (child->type == list_type) {
                // This is the next in the list.
                next_cursor = child;
            }
        }
        // Go to the next entry, even if it's NULL.
        list_cursor = next_cursor;
    }

    // Return what we got.
    assert(list_cursor == nullptr || list_cursor->type == list_type);
    assert(list_entry == nullptr || list_entry->type == entry_type);
    if (out_list_tail != nullptr) *out_list_tail = list_cursor;
    return list_entry;
}

enum parse_statement_decoration_t get_decoration(tnode_t<grammar::plain_statement> stmt) {
    parse_statement_decoration_t decoration = parse_statement_decoration_none;
    if (auto decorated_statement = stmt.try_get_parent<grammar::decorated_statement>()) {
        decoration = static_cast<parse_statement_decoration_t>(decorated_statement.tag());
    }
    return decoration;
}

enum parse_job_decoration_t bool_statement_type(tnode_t<grammar::job_decorator> stmt) {
    return static_cast<parse_job_decoration_t>(stmt.tag());
}

enum parse_job_decoration_t bool_statement_type(
    tnode_t<grammar::job_conjunction_continuation> cont) {
    return static_cast<parse_job_decoration_t>(cont.tag());
}

maybe_t<pipe_or_redir_t> redirection_for_node(tnode_t<grammar::redirection> redirection,
                                              const wcstring &src, wcstring *out_target) {
    assert(redirection && "redirection is missing");
    tnode_t<grammar::tok_redirection> prim = redirection.child<0>();  // like 2>
    assert(prim && "expected to have primitive");

    maybe_t<pipe_or_redir_t> result{};
    if (prim.has_source()) {
        result = pipe_or_redir_t::from_string(prim.get_source(src));
        assert(result.has_value() && "Failed to parse valid redirection");
        assert(!result->is_pipe && "Should not be a pipe");
    }
    if (out_target != nullptr) {
        tnode_t<grammar::tok_string> target = redirection.child<1>();  // like 1 or file path
        *out_target = target.has_source() ? target.get_source(src) : wcstring();
    }
    return result;
}

std::vector<tnode_t<grammar::comment>> parse_node_tree_t::comment_nodes_for_node(
    const parse_node_t &parent) const {
    std::vector<tnode_t<grammar::comment>> result;
    if (parent.has_comments()) {
        // Walk all our nodes, looking for comment nodes that have the given node as a parent.
        for (size_t i = 0; i < this->size(); i++) {
            const parse_node_t &potential_comment = this->at(i);
            if (potential_comment.type == parse_special_type_comment &&
                this->get_parent(potential_comment) == &parent) {
                result.emplace_back(this, &potential_comment);
            }
        }
    }
    return result;
}

variable_assignment_node_list_t get_variable_assignment_nodes(
    tnode_t<grammar::variable_assignments> list, size_t max) {
    return list.descendants<grammar::variable_assignment>(max);
}

maybe_t<wcstring> command_for_plain_statement(tnode_t<grammar::plain_statement> stmt,
                                              const wcstring &src) {
    tnode_t<grammar::tok_string> cmd = stmt.child<0>();
    if (cmd && cmd.has_source()) {
        return cmd.get_source(src);
    }
    return none();
}

arguments_node_list_t get_argument_nodes(tnode_t<grammar::argument_list> list, size_t max) {
    return list.descendants<grammar::argument>(max);
}

arguments_node_list_t get_argument_nodes(tnode_t<grammar::arguments_or_redirections_list> list,
                                         size_t max) {
    return list.descendants<grammar::argument>(max);
}

bool job_node_is_background(tnode_t<grammar::job> job) {
    tnode_t<grammar::optional_background> bg = job.child<4>();
    return bg.tag() == parse_background;
}

parse_job_decoration_t get_decorator(tnode_t<grammar::job_conjunction> conj) {
    using namespace grammar;
    tnode_t<job_decorator> dec;
    // We have two possible parents: job_list and andor_job_list.
    if (auto p = conj.try_get_parent<job_list>()) {
        dec = p.require_get_child<job_decorator, 0>();
    } else if (auto p = conj.try_get_parent<andor_job_list>()) {
        dec = p.require_get_child<job_decorator, 0>();
    }
    // note this returns 0 (none) if dec is empty.
    return bool_statement_type(dec);
}

pipeline_position_t get_pipeline_position(tnode_t<grammar::statement> st) {
    using namespace grammar;
    if (!st) {
        return pipeline_position_t::none;
    }

    // If we're part of a job continuation, we're definitely in a pipeline.
    if (st.try_get_parent<job_continuation>()) {
        return pipeline_position_t::subsequent;
    }

    // Check if we're the beginning of a job, and if so, whether that job
    // has a non-empty continuation.
    tnode_t<job_continuation> jc = st.try_get_parent<job>().child<3>();
    if (jc.try_get_child<statement, 3>()) {
        return pipeline_position_t::first;
    }
    return pipeline_position_t::none;
}
