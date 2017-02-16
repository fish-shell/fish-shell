// A small utility to print information related to pressing keys. This is similar to using tools
// like `xxd` and `od -tx1z` but provides more information such as the time delay between each
// character. It also allows pressing and interpreting keys that are normally special such as
// [ctrl-C] (interrupt the program) or [ctrl-D] (EOF to signal the program should exit).
// And unlike those other tools this one disables ICRNL mode so it can distinguish between
// carriage-return (\cM) and newline (\cJ).
//
// Type "exit" or "quit" to terminate the program.
#include "config.h"  // IWYU pragma: keep

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

#include <memory>
#include <string>
#include <vector>

#include "common.h"
#include "env.h"
#include "fallback.h"  // IWYU pragma: keep
#include "input.h"
#include "input_common.h"
#include "print_help.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

struct config_paths_t determine_config_directory_paths(const char *argv0);

static const wchar_t *ctrl_symbolic_names[] = {
    NULL,   NULL,   NULL,   NULL, NULL, NULL,   NULL, L"\\a", L"\\b", L"\\t", L"\\n",
    L"\\v", L"\\f", L"\\r", NULL, NULL, NULL,   NULL, NULL,   NULL,   NULL,   NULL,
    NULL,   NULL,   NULL,   NULL, NULL, L"\\e", NULL, NULL,   NULL,   NULL};
static bool keep_running = true;

/// Return true if the recent sequence of characters indicates the user wants to exit the program.
static bool should_exit(wchar_t wc) {
    unsigned char c = wc < 0x80 ? wc : 0;
    static unsigned char recent_chars[4] = {0};

    recent_chars[0] = recent_chars[1];
    recent_chars[1] = recent_chars[2];
    recent_chars[2] = recent_chars[3];
    recent_chars[3] = c;
    if (c == shell_modes.c_cc[VINTR]) {
        if (recent_chars[2] == shell_modes.c_cc[VINTR]) return true;
        fwprintf(stderr, L"Press [ctrl-%c] again to exit\n", shell_modes.c_cc[VINTR] + 0x40);
        return false;
    }
    if (c == shell_modes.c_cc[VEOF]) {
        if (recent_chars[2] == shell_modes.c_cc[VEOF]) return true;
        fwprintf(stderr, L"Press [ctrl-%c] again to exit\n", shell_modes.c_cc[VEOF] + 0x40);
        return false;
    }
    return memcmp(recent_chars, "exit", 4) == 0 || memcmp(recent_chars, "quit", 4) == 0;
}

/// Return the name if the recent sequence of characters matches a known terminfo sequence.
static char *sequence_name(wchar_t wc) {
    unsigned char c = wc < 0x80 ? wc : 0;
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

/// Return true if the character must be escaped in the sequence of chars to be bound in `bind`
/// command.
static bool must_escape(wchar_t wc) {
    switch (wc) {
        case '[':
        case ']':
        case '(':
        case ')':
        case '<':
        case '>':
        case '{':
        case '}':
        case '*':
        case '\\':
        case '?':
        case '$':
        case '#':
        case ';':
        case '&':
        case '|':
        case '\'':
        case '"':
            return true;
        default:
            return false;
    }
}

static wchar_t *char_to_symbol(wchar_t wc, bool bind_friendly) {
#define BUF_LEN 64
    static wchar_t buf[BUF_LEN];

    if (wc < L' ') {
        // ASCII control character.
        if (ctrl_symbolic_names[wc]) {
            if (bind_friendly) {
                swprintf(buf, BUF_LEN, L"%ls", ctrl_symbolic_names[wc]);
            } else {
                swprintf(buf, BUF_LEN, L"\\c%c  (or %ls)", wc + 0x40, ctrl_symbolic_names[wc]);
            }
        } else {
            swprintf(buf, BUF_LEN, L"\\c%c", wc + 0x40);
        }
    } else if (wc == L' ') {
        // The "space" character.
        if (bind_friendly) {
            swprintf(buf, BUF_LEN, L"\\x%X", ' ');
        } else {
            swprintf(buf, BUF_LEN, L"\\x%X  (aka \"space\")", ' ');
        }
    } else if (wc == 0x7F) {
        // The "del" character.
        if (bind_friendly) {
            swprintf(buf, BUF_LEN, L"\\x%X", 0x7F);
        } else {
            swprintf(buf, BUF_LEN, L"\\x%X  (aka \"del\")", 0x7F);
        }
    } else if (wc < 0x80) {
        // ASCII characters that are not control characters.
        if (bind_friendly && must_escape(wc)) {
            swprintf(buf, BUF_LEN, L"\\%c", wc);
        } else {
            swprintf(buf, BUF_LEN, L"%c", wc);
        }
    } else if (wc <= 0xFFFF) {
        swprintf(buf, BUF_LEN, L"\\u%04X", (int)wc);
    } else {
        swprintf(buf, BUF_LEN, L"\\U%06X", (int)wc);
    }

    return buf;
}

static void add_char_to_bind_command(wchar_t wc, std::vector<wchar_t> &bind_chars) {
    bind_chars.push_back(wc);
}

static void output_bind_command(std::vector<wchar_t> &bind_chars) {
    if (bind_chars.size()) {
        fputws(L"bind ", stdout);
        for (size_t i = 0; i < bind_chars.size(); i++) {
            fputws(char_to_symbol(bind_chars[i], true), stdout);
        }
        fputws(L" 'do something'\n", stdout);
        bind_chars.clear();
    }
}

static void output_info_about_char(wchar_t wc) {
    fwprintf(stderr, L"hex: %4X  char: %ls\n", wc, char_to_symbol(wc, false));
}

static bool output_matching_key_name(wchar_t wc) {
    char *name = sequence_name(wc);
    if (name) {
        fwprintf(stdout, L"bind -k %s 'do something'\n", name);
        free(name);
        return true;
    }
    return false;
}

static double output_elapsed_time(double prev_tstamp, bool first_char_seen) {
    // How much time has passed since the previous char was received in microseconds.
    double now = timef();
    long long int delta_tstamp_us = 1000000 * (now - prev_tstamp);

    if (delta_tstamp_us >= 200000 && first_char_seen) fputwc(L'\n', stderr);
    if (delta_tstamp_us >= 1000000) {
        fwprintf(stderr, L"              ");
    } else {
        fwprintf(stderr, L"(%3lld.%03lld ms)  ", delta_tstamp_us / 1000, delta_tstamp_us % 1000);
    }
    return now;
}

/// Process the characters we receive as the user presses keys.
static void process_input(bool continuous_mode) {
    bool first_char_seen = false;
    double prev_tstamp = 0.0;
    std::vector<wchar_t> bind_chars;

    fwprintf(stderr, L"Press a key\n\n");
    while (keep_running) {
        wchar_t wc;
        if (reader_interrupted()) {
            wc = shell_modes.c_cc[VINTR];
        } else {
            wc = input_common_readch(true);
        }
        if (wc == R_TIMEOUT || wc == R_EOF) {
            output_bind_command(bind_chars);
            if (first_char_seen && !continuous_mode) {
                return;
            }
            continue;
        }

        prev_tstamp = output_elapsed_time(prev_tstamp, first_char_seen);
        add_char_to_bind_command(wc, bind_chars);
        output_info_about_char(wc);
        if (output_matching_key_name(wc)) {
            output_bind_command(bind_chars);
        }

        if (should_exit(wc)) {
            fwprintf(stderr, L"\nExiting at your request.\n");
            break;
        }

        first_char_seen = true;
    }
}

/// Make sure we cleanup before exiting if we receive a signal that should cause us to exit.
/// Otherwise just report receipt of the signal.
static struct sigaction old_sigactions[32];
static void signal_handler(int signo, siginfo_t *siginfo, void *siginfo_arg) {
    fwprintf(stdout, _(L"signal #%d (%ls) received\n"), signo, sig2wcs(signo));
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
                debug(3, "signal #%d (%ls) was being ignored", signo, sig2wcs(signo));
            }
            if (old_sa.sa_flags && ~SA_SIGINFO != 0) {
                debug(3, L"signal #%d (%ls) handler had flags 0x%X", signo, sig2wcs(signo),
                      old_sa.sa_flags);
            }
        }
    }
}

/// Setup our environment (e.g., tty modes), process key strokes, then reset the environment.
static void setup_and_process_keys(bool continuous_mode) {
    is_interactive_session = 1;  // by definition this program is interactive
    set_main_thread();
    setup_fork_guards();
    env_init();
    reader_init();
    proc_push_interactive(1);
    install_our_signal_handlers();

    if (continuous_mode) {
        fwprintf(stderr, L"\n");
        fwprintf(stderr, L"To terminate this program type \"exit\" or \"quit\" in this window,\n");
        fwprintf(stderr, L"or press [ctrl-%c] or [ctrl-%c] twice in a row.\n",
                 shell_modes.c_cc[VINTR] + 0x40, shell_modes.c_cc[VEOF] + 0x40);
        fwprintf(stderr, L"\n");
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
    const char *short_opts = "+cd:D:h";
    const struct option long_opts[] = {{"continuous", no_argument, NULL, 'c'},
                                       {"debug-level", required_argument, NULL, 'd'},
                                       {"debug-stack-frames", required_argument, NULL, 'D'},
                                       {"help", no_argument, NULL, 'h'},
                                       {NULL, 0, NULL, 0}};
    int opt;
    bool error = false;
    while (!error && (opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 0: {
                fwprintf(stderr, L"getopt_long() unexpectedly returned zero\n");
                error = true;
                break;
            }
            case 'c': {
                continuous_mode = true;
                break;
            }
            case 'h': {
                print_help("fish_key_reader", 0);
                exit(0);
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
                    error = true;
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
                    error = true;
                    break;
                }
                break;
            }
            default: {
                // We assume getopt_long() has already emitted a diagnostic msg.
                error = true;
                break;
            }
        }
    }
    if (error) return 1;

    argc -= optind;
    if (argc != 0) {
        fwprintf(stderr, L"Expected no arguments, got %d\n", argc);
        return 1;
    }

    if (!isatty(STDIN_FILENO)) {
        fwprintf(stderr, L"Stdin must be attached to a tty.\n");
        return 1;
    }

    setup_and_process_keys(continuous_mode);
    return 0;
}
