// A small utility to print information related to pressing keys. This is similar to using tools
// like `xxd` and `od -tx1z` but provides more information such as the time delay between each
// character. It also allows pressing and interpreting keys that are normally special such as
// [ctrl-C] (interrupt the program) or [ctrl-D] (EOF to signal the program should exit).
// And unlike those other tools this one disables ICRNL mode so it can distinguish between
// carriage-return (\cM) and newline (\cJ).
//
// Type "exit" or "quit" to terminate the program.
#include "config.h"  // IWYU pragma: keep

#include <string>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input.h"
#include "input_common.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"
#include "wutil.h" // IWYU pragma: keep

struct config_paths_t determine_config_directory_paths(const char *argv0);

static const char *ctrl_symbolic_names[] = {NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL, "\\a",
                                            "\\b", "\\t", "\\n", "\\v", "\\f", "\\r", NULL, NULL,
                                            NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL, NULL,
                                            NULL,  NULL,  NULL,  "\\e", NULL,  NULL,  NULL, NULL};
static bool keep_running = true;

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
static bool should_exit(unsigned char c) {
    static unsigned char recent_chars[4] = {0};

    recent_chars[0] = recent_chars[1];
    recent_chars[1] = recent_chars[2];
    recent_chars[2] = recent_chars[3];
    recent_chars[3] = c;
    return (memcmp(recent_chars, "exit", 4) == 0 || memcmp(recent_chars, "quit", 4) == 0 ||
            memcmp(recent_chars + 2, "\x3\x3", 2) == 0 ||  // ctrl-C, ctrl-C
            memcmp(recent_chars + 2, "\x4\x4", 2) == 0);   // ctrl-D, ctrl-D
}

/// Return the key name if the recent sequence of characters matches a known terminfo sequence.
static char *const key_name(unsigned char c) {
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

static void output_info_about_char(unsigned char c) {
    printf("dec: %3u  oct: %03o  hex: %02X  char: ", c, c, c);
    if (c < 32) {
        // Control characters.
        printf("\\c%c", c + 64);
        if (ctrl_symbolic_names[c]) printf("   (or %s)", ctrl_symbolic_names[c]);
    } else if (c == 32) {
        // The "space" character.
        printf("\\%03o  (aka \"space\")", c);
    } else if (c == 0x7F) {
        // The "del" character.
        printf("\\%03o  (aka \"del\")", c);
    } else if (c >= 128) {
        // Non-ASCII characters (i.e., those with bit 7 set).
        printf("\\%03o  (aka non-ASCII)", c);
    } else {
        // ASCII characters that are not control characters.
        printf("%c", c);
    }
    putchar('\n');
}

static void output_matching_key_name(unsigned char c) {
    char *name = key_name(c);
    if (name) {
        printf("Sequence matches bind key name \"%s\"\n", name);
        free(name);
    }
}

static double output_elapsed_time(double prev_tstamp, bool first_char_seen) {
    // How much time has passed since the previous char was received in microseconds.
    double now = timef();
    long long int delta_tstamp_us = 1000000 * (now - prev_tstamp);

    if (delta_tstamp_us >= 200000 && first_char_seen) putchar('\n');
    if (delta_tstamp_us >= 1000000) {
        printf("              ");
    } else {
        printf("(%3lld.%03lld ms)  ", delta_tstamp_us / 1000, delta_tstamp_us % 1000);
    }
    return now;
}

/// Process the characters we receive as the user presses keys.
static void process_input(bool continuous_mode) {
    bool first_char_seen = false;
    double prev_tstamp = 0.0;

    printf("Press a key\n\n");
    while (keep_running) {
        wchar_t wc = input_common_readch(first_char_seen && !continuous_mode);
        if (wc == WEOF) {
            return;
        } else if (wc > 255) {
            printf("\nUnexpected wide character from input_common_readch(): %lld / 0x%llx\n",
                   (long long)wc, (long long)wc);
            return;
        }

        unsigned char c = wc;
        prev_tstamp = output_elapsed_time(prev_tstamp, first_char_seen);
        output_info_about_char(c);
        output_matching_key_name(c);

        if (should_exit(c)) {
            printf("\nExiting at your request.\n");
            break;
        }

        first_char_seen = true;
    }
}

/// Make sure we cleanup before exiting if we receive a signal that should cause us to exit.
/// Otherwise just report receipt of the signal.
static struct sigaction old_sigactions[32];
static void signal_handler(int signo, siginfo_t *siginfo, void *siginfo_arg) {
    debug(2, L"signal #%d (%ls) received", signo, sig2wcs(signo));
    // SIGINT isn't included in the following conditional because it is handled specially by fish;
    // i.e., it causes \cC to be reinserted into the tty input stream.
    if (signo == SIGHUP || signo == SIGTERM || signo == SIGABRT || signo == SIGSEGV) {
        keep_running = false;
    }
    if (old_sigactions[signo].sa_handler != SIG_IGN &&
        old_sigactions[signo].sa_handler != SIG_DFL) {
        int needs_siginfo = old_sigactions[signo].sa_flags & SA_SIGINFO;
        if (needs_siginfo) {
            old_sigactions[signo].sa_sigaction(signo, siginfo, siginfo_arg);
        } else {
            old_sigactions[signo].sa_handler(signo);
        }
    }
}

/// Install a handler for every signal.  This allows us to restore the tty modes so the terminal is
/// still usable when we die.  If the signal already has a handler arrange to invoke it from within
/// our handler.
static void install_our_signal_handlers() {
    struct sigaction new_sa, old_sa;
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = SA_SIGINFO;
    new_sa.sa_sigaction = signal_handler;

    for (int signo = 1; signo < 32; signo++) {
        if (sigaction(signo, &new_sa, &old_sa) != -1) {
            memcpy(&old_sigactions[signo], &old_sa, sizeof(old_sa));
            if (old_sa.sa_handler == SIG_IGN) {
                debug(2, "signal #%d (%ls) was being ignored", signo, sig2wcs(signo));
            }
            if (old_sa.sa_flags && ~SA_SIGINFO != 0) {
                debug(2, L"signal #%d (%ls) handler had flags 0x%X", signo, sig2wcs(signo),
                      old_sa.sa_flags);
            }
        }
    }
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
static void setup_and_process_keys(bool continuous_mode) {
    is_interactive_session = 1;    // by definition this program is interactive
    setenv("LC_ALL", "POSIX", 1);  // ensure we're in a single-byte locale
    set_main_thread();
    setup_fork_guards();
    env_init();
    reader_init();
    input_init();
    proc_push_interactive(1);
    signal_set_handlers();
    install_our_signal_handlers();

    if (continuous_mode) {
        printf("\n");
        printf("To terminate this program type \"exit\" or \"quit\" in this window,\n");
        printf("or press [ctrl-C] or [ctrl-D] twice in a row.\n");
        printf("\n");
    }

    process_input(continuous_mode);
    restore_term_mode();
    restore_term_foreground_process_group();
    input_destroy();
    reader_destroy();
}

int main(int argc, char **argv) {
    program_name = L"fish_key_reader";
    bool continuous_mode = false;
    const char *short_opts = "+cd:D:";
    const struct option long_opts[] = {{"continuous", no_argument, NULL, 'c'},
                                       {"debug-level", required_argument, NULL, 'd'},
                                       {"debug-stack-frames", required_argument, NULL, 'D'},
                                       {NULL, 0, NULL, 0}};
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
            case 'd': {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp >= 0 && tmp <= 10 && !*end && !errno) {
                    debug_level = (int)tmp;
                } else {
                    fwprintf(stderr, _(L"Invalid value '%s' for debug-level flag"), optarg);
                    exit(1);
                }
                break;
            }
            case 'D': {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp > 0 && tmp <= 128 && !*end && !errno) {
                    debug_stack_frames = (int)tmp;
                } else {
                    fwprintf(stderr, _(L"Invalid value '%s' for debug-stack-frames flag"), optarg);
                    exit(1);
                }
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
        fprintf(stderr, "Expected no arguments, got %d\n", argc);
        return 1;
    }

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Stdin and stdout must be attached to a tty, redirection not allowed.\n");
        return 1;
    }

    setup_and_process_keys(continuous_mode);
    return 0;
}
