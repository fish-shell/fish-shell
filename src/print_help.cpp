/** \file print_help.c
  Print help message for the specified command
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "print_help.h"

#define CMD_LEN 1024

#define HELP_ERR "Could not show help message\n"

/* defined in common.h */
ssize_t write_loop(int fd, const char *buff, size_t count);


void print_help(const char *c, int fd)
{
    char cmd[ CMD_LEN];
    int printed = snprintf(cmd, CMD_LEN, "fish -c '__fish_print_help %s >&%d'", c, fd);

    if (printed < CMD_LEN)
    {
        if ((system(cmd) == -1))
        {
            write_loop(2, HELP_ERR, strlen(HELP_ERR));
        }

    }

}
