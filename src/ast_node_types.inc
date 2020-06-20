// Define ELEM and optionally ELEMLIST before including this file.
// ELEM is for ordinary nodes.
// ELEMLIST(x, y) marks list nodes and the type they contain.
#ifndef ELEMLIST
#define ELEMLIST(x, y) ELEM(x)
#endif

ELEM(keyword_base)
ELEM(token_base)
ELEM(maybe_newlines)

ELEM(argument)
ELEMLIST(argument_list, argument)

ELEM(redirection)
ELEM(argument_or_redirection)
ELEMLIST(argument_or_redirection_list, argument_or_redirection)

ELEM(variable_assignment)
ELEMLIST(variable_assignment_list, variable_assignment)

ELEM(job)
ELEM(job_conjunction)
// For historical reasons, a job list is a list of job *conjunctions*. This should be fixed.
ELEMLIST(job_list, job_conjunction)
ELEM(job_conjunction_continuation)
ELEMLIST(job_conjunction_continuation_list, job_conjunction_continuation)

ELEM(job_continuation)
ELEMLIST(job_continuation_list, job_continuation)

ELEM(andor_job)
ELEMLIST(andor_job_list, andor_job)

ELEM(statement)

ELEM(not_statement)

ELEM(block_statement)
ELEM(for_header)
ELEM(while_header)
ELEM(function_header)
ELEM(begin_header)

ELEM(if_statement)
ELEM(if_clause)
ELEM(elseif_clause)
ELEMLIST(elseif_clause_list, elseif_clause)
ELEM(else_clause)

ELEM(switch_statement)
ELEM(case_item)
ELEMLIST(case_item_list, case_item)

ELEM(decorated_statement)

ELEM(freestanding_argument_list)

#undef ELEM
#undef ELEMLIST
