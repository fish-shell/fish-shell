/*
    A small utility to print the resulting key codes from pressing a
  key. Servers the same function as hitting ^V in bash, but I prefer
  the way key_reader works.

  Type ^C to exit the program.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <locale.h>
#include <termcap.h>

#include "common.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input_common.h"

int writestr(char *str)
{
    write_ignore(1, str, strlen(str));
    return 0;
}

int main(int argc, char **argv)
{
    set_main_thread();
    setup_fork_guards();
    setlocale(LC_ALL, "");


    if (argc == 2)
    {
        static char term_buffer[2048];
        char *termtype = getenv("TERM");
        char *tbuff = new char[9999];
        char *res;

        tgetent(term_buffer, termtype);
        res = tgetstr(argv[1], &tbuff);
        if (res != 0)
        {
            while (*res != 0)
            {
                printf("%d ", *res);


                res++;
            }
            printf("\n");
        }
        else
        {
            printf("Undefined sequence\n");
        }
    }
    else
    {
        char scratch[1024];
        unsigned int c;

        struct termios modes,      /* so we can change the modes */
                savemodes;  /* so we can reset the modes when we're done */

        input_common_init(0);


        tcgetattr(0,&modes);        /* get the current terminal modes */
        savemodes = modes;          /* save a copy so we can reset them */

        modes.c_lflag &= ~ICANON;   /* turn off canonical mode */
        modes.c_lflag &= ~ECHO;   /* turn off echo mode */
        modes.c_cc[VMIN]=1;
        modes.c_cc[VTIME]=0;
        tcsetattr(0,TCSANOW,&modes);      /* set the new modes */
        while (1)
    {
            if ((c=input_common_readch(0)) == EOF)
                break;
            if ((c > 31) && (c != 127))
                sprintf(scratch, "dec: %u hex: %x char: %c\n", c, c, c);
            else
                sprintf(scratch, "dec: %u hex: %x\n", c, c);
            writestr(scratch);
        }
        /* reset the terminal to the saved mode */
        tcsetattr(0,TCSANOW,&savemodes);

        input_common_destroy();
    }

    return 0;
}
