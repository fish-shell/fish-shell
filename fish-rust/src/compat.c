#include <paths.h>
#include <stdint.h>
#include <stdlib.h>
#include <term.h>

size_t C_MB_CUR_MAX() { return MB_CUR_MAX; }

int has_cur_term() { return cur_term != NULL; }

uint64_t C_ST_LOCAL() {
#if defined(ST_LOCAL)
    return ST_LOCAL;
#else
    return 0;
#endif
}

uint64_t C_MNT_LOCAL() {
#if defined(MNT_LOCAL)
    return MNT_LOCAL;
#else
    return 0;
#endif
}

const char* C_PATH_BSHELL() { return _PATH_BSHELL; }
