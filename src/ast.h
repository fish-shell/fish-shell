// Programmatic representation of fish grammar.

#ifndef FISH_AST_H
#define FISH_AST_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "common.h"
#include "cxx.h"
#include "maybe.h"
#include "parse_constants.h"

#if INCLUDE_RUST_HEADERS
#include "ast.rs.h"
namespace ast {
using ast_t = Ast;
using category_t = Category;
using type_t = Type;

using andor_job_list_t = AndorJobList;
using andor_job_t = AndorJob;
using argument_list_t = ArgumentList;
using argument_or_redirection_list_t = ArgumentOrRedirectionList;
using argument_or_redirection_t = ArgumentOrRedirection;
using argument_t = Argument;
using begin_header_t = BeginHeader;
using block_statement_t = BlockStatement;
using case_item_t = CaseItem;
using decorated_statement_t = DecoratedStatement;
using elseif_clause_list_t = ElseifClauseList;
using for_header_t = ForHeader;
using freestanding_argument_list_t = FreestandingArgumentList;
using function_header_t = FunctionHeader;
using if_clause_t = IfClause;
using if_statement_t = IfStatement;
using job_conjunction_continuation_t = JobConjunctionContinuation;
using job_conjunction_t = JobConjunction;
using job_continuation_t = JobContinuation;
using job_list_t = JobList;
using job_pipeline_t = JobPipeline;
using maybe_newlines_t = MaybeNewlines;
using not_statement_t = NotStatement;
using redirection_t = Redirection;
using semi_nl_t = SemiNl;
using statement_t = Statement;
using string_t = String_;
using switch_statement_t = SwitchStatement;
using variable_assignment_list_t = VariableAssignmentList;
using variable_assignment_t = VariableAssignment;
using while_header_t = WhileHeader;

}  // namespace ast

#else
struct Ast;
struct NodeFfi;
struct BlockStatement;
namespace ast {
using ast_t = Ast;

using block_statement_t = BlockStatement;

struct argument_t;
struct statement_t;
struct string_t;
struct maybe_newlines_t;
struct redirection_t;
struct variable_assignment_t;
struct semi_nl_t;
struct decorated_statement_t;

struct keyword_base_t;

}  // namespace ast

#endif

using DecoratedStatement = ast::decorated_statement_t;
using BlockStatement = ast::block_statement_t;

namespace ast {
using node_t = ::NodeFfi;
}

rust::Box<Ast> ast_parse(const wcstring &src, parse_tree_flags_t flags = parse_flag_none,
                         parse_error_list_t *out_errors = nullptr);
rust::Box<Ast> ast_parse_argument_list(const wcstring &src,
                                       parse_tree_flags_t flags = parse_flag_none,
                                       parse_error_list_t *out_errors = nullptr);

#endif  // FISH_AST_H
