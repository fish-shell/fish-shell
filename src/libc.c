#include <locale.h>
#include <paths.h>  // _PATH_BSHELL
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>     // MB_CUR_MAX
#include <sys/mount.h>  // MNT_LOCAL
#include <sys/resource.h>
#include <sys/statvfs.h>  // ST_LOCAL
#include <unistd.h>       // _CS_PATH, _PC_CASE_SENSITIVE

size_t C_MB_CUR_MAX() { return MB_CUR_MAX; }

uint64_t C_ST_LOCAL() {
#if defined(ST_LOCAL)
    return ST_LOCAL;
#else
    return 0;
#endif
}

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

int C_RLIMIT_CORE() { return RLIMIT_CORE; }
int C_RLIMIT_DATA() { return RLIMIT_DATA; }
int C_RLIMIT_FSIZE() { return RLIMIT_FSIZE; }
int C_RLIMIT_NOFILE() { return RLIMIT_NOFILE; }
int C_RLIMIT_STACK() { return RLIMIT_STACK; }
int C_RLIMIT_CPU() { return RLIMIT_CPU; }

int C_RLIMIT_SBSIZE() {
#ifdef RLIMIT_SBSIZE
    return RLIMIT_SBSIZE;
#else
    return -1;
#endif
}

int C_RLIMIT_NICE() {
#ifdef RLIMIT_NICE
    return RLIMIT_NICE;
#else
    return -1;
#endif
}

int C_RLIMIT_SIGPENDING() {
#ifdef RLIMIT_SIGPENDING
    return RLIMIT_SIGPENDING;
#else
    return -1;
#endif
}

int C_RLIMIT_MEMLOCK() {
#ifdef RLIMIT_MEMLOCK
    return RLIMIT_MEMLOCK;
#else
    return -1;
#endif
}

int C_RLIMIT_RSS() {
#ifdef RLIMIT_RSS
    return RLIMIT_RSS;
#else
    return -1;
#endif
}

int C_RLIMIT_MSGQUEUE() {
#ifdef RLIMIT_MSGQUEUE
    return RLIMIT_MSGQUEUE;
#else
    return -1;
#endif
}

int C_RLIMIT_RTPRIO() {
#ifdef RLIMIT_RTPRIO
    return RLIMIT_RTPRIO;
#else
    return -1;
#endif
}

int C_RLIMIT_NPROC() {
#ifdef RLIMIT_NPROC
    return RLIMIT_NPROC;
#else
    return -1;
#endif
}

int C_RLIMIT_AS() {
#ifdef RLIMIT_AS
    return RLIMIT_AS;
#else
    return -1;
#endif
}

int C_RLIMIT_SWAP() {
#ifdef RLIMIT_SWAP
    return RLIMIT_SWAP;
#else
    return -1;
#endif
}

int C_RLIMIT_RTTIME() {
#ifdef RLIMIT_RTTIME
    return RLIMIT_RTTIME;
#else
    return -1;
#endif
}

int C_RLIMIT_KQUEUES() {
#ifdef RLIMIT_KQUEUES
    return RLIMIT_KQUEUES;
#else
    return -1;
#endif
}

int C_RLIMIT_NPTS() {
#ifdef RLIMIT_NPTS
    return RLIMIT_NPTS;
#else
    return -1;
#endif
}

int C_RLIMIT_NTHR() {
#ifdef RLIMIT_NTHR
    return RLIMIT_NTHR;
#else
    return -1;
#endif
}
