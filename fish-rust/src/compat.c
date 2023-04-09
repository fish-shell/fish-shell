#include <stdlib.h>
#include <term.h>

size_t C_MB_CUR_MAX() { return MB_CUR_MAX; }

int has_cur_term() { return cur_term != NULL; }
