/** \file postfork.h

	Functions that we may safely call after fork(), of which there are very few. In particular we cannot allocate memory, since we're insane enough to call fork from a multithreaded process.
*/

#ifndef FISH_POSTFORK_H
#define FISH_POSTFORK_H

#include <wchar.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <list>

#include "config.h"
#include "common.h"
#include "util.h"
#include "proc.h"
#include "wutil.h"
#include "io.h"


int set_child_group( job_t *j, process_t *p, int print_errors );
int setup_child_process( job_t *j, process_t *p );

#endif
