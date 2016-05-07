// A small utility to print information related to pressing keys. This is similar to using tools
// like `xxd` and `od -tx1z` but provides more information such as the time delay between each
// character. It also allows pressing and interpreting keys that are normally special such as
// [ctrl-C] (interrupt the program) or [ctrl-D] (EOF to signal the program should exit).
// And unlike those other tools this one disables ICRNL mode so it can distinguish between
// carriage-return (\cM) and newline (\cJ).
//
// Type "exit" or "quit" to terminate the program.
#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <wctype.h>
#include <string>

#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input.h"
#include "input_common.h"

struct config_paths_t determine_config_directory_paths(const char *argv0);

static struct termios saved_modes;  // so we can reset the modes when we're done
static long long int prev_tstamp = 0;
static const char *ctrl_equivalents[] = {NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL, "\\a",
    "\\b", "\\t", "\\n", "\\v", "\\f", "\\r", NULL, NULL,
    NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL, NULL,
    NULL,  NULL,  NULL,  "\\e", NULL,  NULL,  NULL, NULL};

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
bool should_exit(unsigned char c) {
    static unsigned char recent_chars[4] = {0};

    recent_chars[0] = recent_chars[1];
    recent_chars[1] = recent_chars[2];
    recent_chars[2] = recent_chars[3];
    recent_chars[3] = c;
    return memcmp(recent_chars, "exit", 4) == 0 || memcmp(recent_chars, "quit", 4) == 0;
}

/// Return the key name if the recent sequence of characters matches a known terminfo sequence.
char * const key_name(unsigned char c) {
    static char recent_chars[8] = {0};

    recent_chars[0] = recent_chars[1];
    recent_chars[1] = recent_chars[2];
    recent_chars[2] = recent_chars[3];
    recent_chars[3] = recent_chars[4];
    recent_chars[4] = recent_chars[5];
    recent_chars[5] = recent_chars[6];
    recent_chars[6] = recent_chars[7];
    recent_chars[7] = c;

    for (int idx = 7; idx >= 0; idx--) {
        wcstring out_name;
        wcstring seq = str2wcstring(recent_chars + idx, 8 - idx);
        bool found = input_terminfo_get_name(seq, &out_name);
        if (found) {
            return strdup(wcs2string(out_name).c_str());
        }
    }

    return NULL;
}

/// Process the characters we receive as the user presses keys.
void process_input(bool continuous_mode) {
    bool first_char_seen = false;
    while (true) {
        wchar_t wc = input_common_readch(first_char_seen && !continuous_mode);
        if (wc == WEOF) {
            return;
        }
        if (wc > 255) {
            printf("\nUnexpected wide character from input_common_readch(): %lld / 0x%llx\n",
                   (long long)wc, (long long)wc);
            return;
        }

        long long int curr_tstamp, delta_tstamp;
        timeval char_tstamp;
        gettimeofday(&char_tstamp, NULL);
        curr_tstamp = char_tstamp.tv_sec * 1000000 + char_tstamp.tv_usec;
        delta_tstamp = curr_tstamp - prev_tstamp;
        if (delta_tstamp >= 1000000) delta_tstamp = 999999;
        if (delta_tstamp >= 200000 && continuous_mode) {
            printf("\n");
            printf("Type 'exit' or 'quit' to terminate this program.\n");
            printf("\n");
        }
        prev_tstamp = curr_tstamp;

        unsigned char c = wc;
        if (c < 32) {
            // Control characters.
            if (ctrl_equivalents[c]) {
                printf("%6lld usec  dec: %3u  hex: %2x  char: %s (aka \\c%c)\n", delta_tstamp, c, c,
                       ctrl_equivalents[c], c + 64);
            } else {
                printf("%6lld usec  dec: %3u  hex: %2x  char: \\c%c\n", delta_tstamp, c, c, c + 64);
            }
        } else if (c == 32) {
            // The space character.
            printf("%6lld usec  dec: %3u  hex: %2x  char: <space>\n", delta_tstamp, c, c);
        } else if (c == 127) {
            // The "del" character.
            printf("%6lld usec  dec: %3u  hex: %2x  char: \\x7f (aka del)\n", delta_tstamp, c, c);
        } else if (c >= 128) {
            // Non-ASCII characters (i.e., those with bit 7 set).
            printf("%6lld usec  dec: %3u  hex: %2x  char: non-ASCII\n", delta_tstamp, c, c);
        } else {
            // ASCII characters that are not control characters.
            printf("%6lld usec  dec: %3u  hex: %2x  char: %c\n", delta_tstamp, c, c, c);
        }

        char * const name = key_name(c);
        if (name) {
            printf("FYI: Saw sequence for bind key name \"%s\"\n", name);
            free(name);
        }

        if (should_exit(c)) {
            printf("\nExiting at your request.\n");
            break;
        }

        first_char_seen = true;
    }
}

/// Set the tty modes to not interpret any characters. We want every character to be passed thru to
/// this program. Including characters such as [ctrl-C] and [ctrl-D] that might normally have
/// special significance (e.g., terminate the program).
bool set_tty_modes(void) {
    struct termios modes;

    tcgetattr(0, &modes);  // get the current tty modes
    saved_modes = modes;   // save a copy so we can reset them on exit

    modes.c_lflag &= ~ICANON;  // turn off canonical mode
    modes.c_lflag &= ~ECHO;    // turn off echo mode
    modes.c_lflag &= ~ISIG;    // turn off recognizing signal generating characters
    modes.c_iflag &= ~ICRNL;   // turn off mapping CR to NL
    modes.c_iflag &= ~INLCR;   // turn off mapping NL to CR
    modes.c_cc[VMIN] = 1;      // return each character as they arrive
    modes.c_cc[VTIME] = 0;     // wait forever for the next character

    if (tcsetattr(0, TCSANOW, &modes) != 0) {  // set the new modes
        return false;
    }
    return true;
}

/// Restore the tty modes to what they were before this program was run. This shouldn't be required
/// but we do it just in case the program that ran us doesn't handle tty modes for external programs
/// in a sensible manner.
void reset_tty_modes() { tcsetattr(0, TCSANOW, &saved_modes); }

/// Make sure we cleanup before exiting if we're signaled.
void signal_handler(int signo) {
    printf("\nExiting on receipt of signal #%d\n", signo);
    reset_tty_modes();
    exit(1);
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
void setup_and_process_keys(bool continuous_mode) {
    set_main_thread();
    setup_fork_guards();
    wsetlocale(LC_ALL, L"POSIX");
    program_name = L"fish_key_reader";
    env_init();
    input_init();

    // Installing our handler for every signal (e.g., SIGSEGV) is dubious because it means that
    // signals that might generate a core dump will not do so. On the other hand this allows us
    // to restore the tty modes so the terminal is still usable when we die.
    for (int signo = 1; signo < 32; signo++) {
        signal(signo, signal_handler);
    }

    if (continuous_mode) {
        printf("\n");
        printf("Type 'exit' or 'quit' to terminate this program.\n");
        printf("\n");
        printf("Characters such as [ctrl-D] (EOF) and [ctrl-C] (interrupt)\n");
        printf("have no special meaning and will not terminate this program.\n");
        printf("\n");
    } else {
        set_wait_on_escape_ms(500);
    }

    if (!set_tty_modes()) {
        printf("Could not set the tty modes. Refusing to continue running.\n");
        exit(1);
    }
    // TODO: We really should enable keypad mode but see issue #838.
    process_input(continuous_mode);
    reset_tty_modes();
}

int main(int argc, char **argv) {
    bool continuous_mode = false;
    const char *short_opts = "+c";
    const struct option long_opts[] = {{"continuous", no_argument, NULL, 'd'}, {NULL, 0, NULL, 0}};
    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 0: {
                fprintf(stderr, "getopt_long() unexpectedly returned zero\n");
                exit(1);
            }
            case 'c': {
                continuous_mode = true;
                break;
            }
            default: {
                // We assume getopt_long() has already emitted a diagnostic msg.
                exit(1);
            }
        }
    }

    argc -= optind;
    if (argc != 0) {
        fprintf(stderr, "Expected no CLI arguments, got %d\n", argc);
        return 1;
    }

    setup_and_process_keys(continuous_mode);
    return 0;
}
