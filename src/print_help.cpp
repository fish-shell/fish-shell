// SPDX-FileCopyrightText: © 2006 Axel Liljencrantz
// SPDX-FileCopyrightText: © 2009 fish-shell contributors
// SPDX-FileCopyrightText: © 2022 fish-shell contributors
//
// SPDX-License-Identifier: GPL-2.0-only

// Print help message for the specified command.
#include "config.h"  // IWYU pragma: keep

#include "print_help.h"

#include <stdio.h>
#include <stdlib.h>

#include <cstring>

#include "common.h"

#define CMD_LEN 1024

#define HELP_ERR "Could not show help message\n"

void print_help(const char *c, int fd) {
    char cmd[CMD_LEN];
    int printed = snprintf(cmd, CMD_LEN, "fish -c '__fish_print_help %s >&%d'", c, fd);

    if (printed < CMD_LEN && system(cmd) == -1) {
        write_loop(2, HELP_ERR, std::strlen(HELP_ERR));
    }
}
