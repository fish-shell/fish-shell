#include "config.h"

#include <paths.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <term.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

size_t C_MB_CUR_MAX() { return MB_CUR_MAX; }

int has_cur_term() { return cur_term != NULL; }

uint64_t C_ST_LOCAL() {
#if defined(ST_LOCAL)
    return ST_LOCAL;
#else
    return 0;
#endif
}

// confstr + _CS_PATH is only available on macOS with rust's libc
// we could just declare extern "C" confstr directly in Rust
// that would panic if it failed to link, which C++ did not
// therefore we define a backup, which just returns an error
// which for confstr is 0
#if defined(_CS_PATH)
#else
size_t confstr(int name, char* buf, size_t size) {
    UNUSED(name);
    UNUSED(buf);
    UNUSED(size);
    return 0;
}
#endif

int C_CS_PATH() {
#if defined(_CS_PATH)
    return _CS_PATH;
#else
    return -1;
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

int C_PC_CASE_SENSITIVE() {
#ifdef _PC_CASE_SENSITIVE
    return _PC_CASE_SENSITIVE;
#else
    return 0;
#endif
}

FILE* stdout_stream() { return stdout; }

int C_O_EXLOCK() {
#ifdef O_EXLOCK
    return O_EXLOCK;
#else
    return 0;
#endif
}

static const bool uvar_file_set_mtime_hack =
#ifdef UVAR_FILE_SET_MTIME_HACK
    true;
#else
    false;
#endif
#undef UVAR_FILE_SET_MTIME_HACK
bool UVAR_FILE_SET_MTIME_HACK() { return uvar_file_set_mtime_hack; }
