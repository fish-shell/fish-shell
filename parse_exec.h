/**\file parse_exec.h

    Programmatic execution of a parse tree
*/

#ifndef FISH_PARSE_EXEC_H
#define FISH_PARSE_EXEC_H

#include "parse_tree.h"

class parse_exec_t;
class parse_execution_context_t
{
    parse_exec_t *ctx;
    
    public:
    parse_execution_context_t(const parse_node_tree_t &n, const wcstring &s);
    wcstring simulate(void);
};


#endif
