// The main loop of the fish program.
/*
Copyright (C) 2005-2008 Axel Liljencrantz

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  // IWYU pragma: keep
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <wchar.h>
#include <memory>
#include <string>
#include <vector>

#include "builtin.h"
#include "common.h"
#include "env.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fish_version.h"
#include "function.h"
#include "history.h"
#include "input.h"
#include "input_common.h"
#include "io.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "wildcard.h"
#include "wutil.h"  // IWYU pragma: keep

// PATH_MAX may not exist.
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/// If we are doing profiling, the filename to output to.
static const char *s_profiling_output_filename = NULL;

static bool has_suffix(const std::string &path, const char *suffix, bool ignore_case) {
    size_t pathlen = path.size(), suffixlen = strlen(suffix);
    return pathlen >= suffixlen &&
           !(ignore_case ? strcasecmp : strcmp)(path.c_str() + pathlen - suffixlen, suffix);
}

/// Modifies the given path by calling realpath. Returns true if realpath succeeded, false
/// otherwise.
static bool get_realpath(std::string &path) {
    char buff[PATH_MAX], *ptr;
    if ((ptr = realpath(path.c_str(), buff))) {
        path = ptr;
    }
    return ptr != NULL;
}

// OS X function for getting the executable path.
extern "C" {
int _NSGetExecutablePath(char *buf, uint32_t *bufsize);
}

/// Return the path to the current executable. This needs to be realpath'd.
static std::string get_executable_path(const char *argv0) {
    char buff[PATH_MAX];

#if __APPLE__
    // On OS X use it's proprietary API to get the path to the executable.
    uint32_t buffSize = sizeof buff;
    if (_NSGetExecutablePath(buff, &buffSize) == 0) return std::string(buff);
#else
    // On non-OS X UNIXes, try /proc directory.
    ssize_t len;
    len = readlink("/proc/self/exe", buff, sizeof buff - 1);  // Linux
    if (len == -1) {
        len = readlink("/proc/curproc/file", buff, sizeof buff - 1);  // BSD
        if (len == -1) {
            len = readlink("/proc/self/path/a.out", buff, sizeof buff - 1);  // Solaris
        }
    }
    if (len > 0) {
        buff[len] = '\0';
        return std::string(buff);
    }
#endif

    // Just return argv0, which probably won't work (i.e. it's not an absolute path or a path
    // relative to the working directory, but instead something the caller found via $PATH). We'll
    // eventually fall back to the compile time paths.
    return std::string(argv0 ? argv0 : "");
}

static struct config_paths_t determine_config_directory_paths(const char *argv0) {
    struct config_paths_t paths;
    bool done = false;
    std::string exec_path = get_executable_path(argv0);
    if (get_realpath(exec_path)) {
#if __APPLE__

        // On OS X, maybe we're an app bundle, and should use the bundle's files. Since we don't
        // link CF, use this lame approach to test it: see if the resolved path ends with
        // /Contents/MacOS/fish, case insensitive since HFS+ usually is.
        if (!done) {
            const char *suffix = "/Contents/MacOS/fish";
            const size_t suffixlen = strlen(suffix);
            if (has_suffix(exec_path, suffix, true)) {
                // Looks like we're a bundle. Cut the string at the / prefixing /Contents... and
                // then the rest.
                wcstring wide_resolved_path = str2wcstring(exec_path);
                wide_resolved_path.resize(exec_path.size() - suffixlen);
                wide_resolved_path.append(L"/Contents/Resources/");

                // Append share, etc, doc.
                paths.data = wide_resolved_path + L"share/fish";
                paths.sysconf = wide_resolved_path + L"etc/fish";
                paths.doc = wide_resolved_path + L"doc/fish";

                // But the bin_dir is the resolved_path, minus fish (aka the MacOS directory).
                paths.bin = str2wcstring(exec_path);
                paths.bin.resize(paths.bin.size() - strlen("/fish"));

                done = true;
            }
        }
#endif

        if (!done) {
            // The next check is that we are in a reloctable directory tree like this:
            //   bin/fish
            //   etc/fish
            //   share/fish
            const char *suffix = "/bin/fish";
            if (has_suffix(exec_path, suffix, false)) {
                wcstring base_path = str2wcstring(exec_path);
                base_path.resize(base_path.size() - strlen(suffix));

                paths.data = base_path + L"/share/fish";
                paths.sysconf = base_path + L"/etc/fish";
                paths.doc = base_path + L"/share/doc/fish";
                paths.bin = base_path + L"/bin";

                // Check only that the data and sysconf directories exist. Handle the doc
                // directories separately.
                struct stat buf;
                if (0 == wstat(paths.data, &buf) && 0 == wstat(paths.sysconf, &buf)) {
                    // The docs dir may not exist; in that case fall back to the compiled in path.
                    if (0 != wstat(paths.doc, &buf)) {
                        paths.doc = L"" DOCDIR;
                    }
                    done = true;
                }
            }
        }
    }

    if (!done) {
        // Fall back to what got compiled in.
        paths.data = L"" DATADIR "/fish";
        paths.sysconf = L"" SYSCONFDIR "/fish";
        paths.doc = L"" DOCDIR;
        paths.bin = L"" BINDIR;

        done = true;
    }

    return paths;
}

// Source the file config.fish in the given directory.
static void source_config_in_directory(const wcstring &dir) {
    // If the config.fish file doesn't exist or isn't readable silently return. Fish versions up
    // thru 2.2.0 would instead try to source the file with stderr redirected to /dev/null to deal
    // with that possibility.
    //
    // This introduces a race condition since the readability of the file can change between this
    // test and the execution of the 'source' command. However, that is not a security problem in
    // this context so we ignore it.
    const wcstring config_pathname = dir + L"/config.fish";
    const wcstring escaped_dir = escape_string(dir, ESCAPE_ALL);
    const wcstring escaped_pathname = escaped_dir + L"/config.fish";
    if (waccess(config_pathname, R_OK) != 0) {
        debug(2, L"not sourcing %ls (not readable or does not exist)", escaped_pathname.c_str());
        return;
    }
    debug(2, L"sourcing %ls", escaped_pathname.c_str());

    const wcstring cmd = L"builtin source " + escaped_pathname;
    parser_t &parser = parser_t::principal_parser();
    parser.set_is_within_fish_initialization(true);
    parser.eval(cmd, io_chain_t(), TOP);
    parser.set_is_within_fish_initialization(false);
}

static int try_connect_socket(std::string &name) {
    int s, r, ret = -1;

    /// Connect to a DGRAM socket rather than the expected STREAM. This avoids any notification to a
    /// remote socket that we have connected, preventing any surprising behaviour. If the connection
    /// fails with EPROTOTYPE, the connection is probably a STREAM; if it succeeds or fails any
    /// other way, there is no cause for alarm. With thanks to Andrew Lutomirski <github.com/amluto>
    if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        wperror(L"socket");
        return -1;
    }

    debug(3, L"Connect to socket %s at fd %d", name.c_str(), s);

    struct sockaddr_un local = {};
    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, name.c_str(), (sizeof local.sun_path) - 1);

    r = connect(s, (struct sockaddr *)&local, sizeof local);

    if (r == -1 && errno == EPROTOTYPE) {
        ret = 0;
    }

    close(s);
    return ret;
}

/// Check for a running fishd from old versions and warn about not being able to share variables.
/// https://github.com/fish-shell/fish-shell/issues/1730
static void check_running_fishd() {
    // There are two paths to check:
    // $FISHD_SOCKET_DIR/fishd.socket.$USER or /tmp/fishd.socket.$USER
    //   - referred to as the "old socket"
    // $XDG_RUNTIME_DIR/fishd.socket or /tmp/fish.$USER/fishd.socket
    //   - referred to as the "new socket"
    // All existing versions of fish attempt to create the old socket, but
    // failure in newer versions is not treated as critical, so both need
    // to be checked.
    const char *uname = getenv("USER");
    if (uname == NULL) {
        const struct passwd *pw = getpwuid(getuid());
        uname = pw->pw_name;
    }

    const char *dir_old_socket = getenv("FISHD_SOCKET_DIR");
    std::string path_old_socket;

    if (dir_old_socket == NULL) {
        path_old_socket = "/tmp/";
    } else {
        path_old_socket.append(dir_old_socket);
    }

    path_old_socket.append("fishd.socket.");
    path_old_socket.append(uname);

    const char *dir_new_socket = getenv("XDG_RUNTIME_DIR");
    std::string path_new_socket;
    if (dir_new_socket == NULL) {
        path_new_socket = "/tmp/fish.";
        path_new_socket.append(uname);
        path_new_socket.push_back('/');
    } else {
        path_new_socket.append(dir_new_socket);
    }

    path_new_socket.append("fishd.socket");

    if (try_connect_socket(path_old_socket) == 0 || try_connect_socket(path_new_socket) == 0) {
        debug(1, _(L"Old versions of fish appear to be running. You will not be able to share "
                   L"variable values between old and new fish sessions. For best results, restart "
                   L"all running instances of fish."));
    }
}

/// Parse init files. exec_path is the path of fish executable as determined by argv[0].
static int read_init(const struct config_paths_t &paths) {
    source_config_in_directory(paths.data);
    source_config_in_directory(paths.sysconf);

    // We need to get the configuration directory before we can source the user configuration file.
    // If path_get_config returns false then we have no configuration directory and no custom config
    // to load.
    wcstring config_dir;
    if (path_get_config(config_dir)) {
        source_config_in_directory(config_dir);
    }

    return 1;
}

/// Parse the argument list, return the index of the first non-flag arguments.
static int fish_parse_opt(int argc, char **argv, std::vector<std::string> *cmds) {
    const char *short_opts = "+hilnvc:p:d:D:";
    const struct option long_opts[] = {{"command", required_argument, NULL, 'c'},
                                       {"debug-level", required_argument, NULL, 'd'},
                                       {"debug-stack-frames", required_argument, NULL, 'D'},
                                       {"interactive", no_argument, NULL, 'i'},
                                       {"login", no_argument, NULL, 'l'},
                                       {"no-execute", no_argument, NULL, 'n'},
                                       {"profile", required_argument, NULL, 'p'},
                                       {"help", no_argument, NULL, 'h'},
                                       {"version", no_argument, NULL, 'v'},
                                       {NULL, 0, NULL, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (opt) {
            case 0: {
                fwprintf(stderr, _(L"getopt_long() unexpectedly returned zero\n"));
                exit(127);
            }
            case 'c': {
                cmds->push_back(optarg);
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
            case 'h': {
                cmds->push_back("__fish_print_help fish");
                break;
            }
            case 'i': {
                is_interactive_session = 1;
                break;
            }
            case 'l': {
                is_login = 1;
                break;
            }
            case 'n': {
                no_exec = 1;
                break;
            }
            case 'p': {
                s_profiling_output_filename = optarg;
                g_profiling_active = true;
                break;
            }
            case 'v': {
                fwprintf(stdout, _(L"%s, version %s\n"), PACKAGE_NAME, get_fish_version());
                exit(0);
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

    // If our command name begins with a dash that implies we're a login shell.
    is_login |= argv[0][0] == '-';

    // We are an interactive session if we have not been given an explicit
    // command or file to execute and stdin is a tty. Note that the -i or
    // --interactive options also force interactive mode.
    if (cmds->size() == 0 && optind == argc && isatty(STDIN_FILENO)) {
        is_interactive_session = 1;
    }

    return optind;
}

int main(int argc, char **argv) {
    int res = 1;
    int my_optind = 0;

    // We can't do this at compile time due to the use of enum symbols.
    assert(EXPAND_SENTINAL >= EXPAND_RESERVED_BASE && EXPAND_SENTINAL <= EXPAND_RESERVED_END);
    assert(ANY_SENTINAL >= WILDCARD_RESERVED_BASE && ANY_SENTINAL <= WILDCARD_RESERVED_END);
    assert(R_SENTINAL >= INPUT_COMMON_BASE && R_SENTINAL <= INPUT_COMMON_END);

    set_main_thread();
    setup_fork_guards();

    wsetlocale(LC_ALL, L"");
    program_name = L"fish";

    // struct stat tmp;
    // stat("----------FISH_HIT_MAIN----------", &tmp);

    std::vector<std::string> cmds;
    my_optind = fish_parse_opt(argc, argv, &cmds);

    // No-exec is prohibited when in interactive mode.
    if (is_interactive_session && no_exec) {
        debug(1, _(L"Can not use the no-execute mode when running an interactive session"));
        no_exec = 0;
    }

    // Only save (and therefore restore) the fg process group if we are interactive. See issues
    // #197 and #1002.
    if (is_interactive_session) {
        save_term_foreground_process_group();
    }

    const struct config_paths_t paths = determine_config_directory_paths(argv[0]);

    proc_init();
    event_init();
    builtin_init();
    function_init();
    env_init(&paths);
    reader_init();
    history_init();
    // For set_color to support term256 in config.fish (issue #1022).
    update_fish_color_support();

    parser_t &parser = parser_t::principal_parser();

    const io_chain_t empty_ios;
    if (read_init(paths)) {
        // Stomp the exit status of any initialization commands (issue #635).
        proc_set_last_status(STATUS_BUILTIN_OK);

        // Run the commands specified as arguments, if any.
        if (!cmds.empty()) {
            // Do something nasty to support OpenSUSE assuming we're bash. This may modify cmds.
            if (is_login) {
                fish_xdm_login_hack_hack_hack_hack(&cmds, argc - my_optind, argv + my_optind);
            }
            for (size_t i = 0; i < cmds.size(); i++) {
                const wcstring cmd_wcs = str2wcstring(cmds.at(i));
                res = parser.eval(cmd_wcs, empty_ios, TOP);
            }
            reader_exit(0, 0);
        } else if (my_optind == argc) {
            // Interactive mode
            check_running_fishd();
            res = reader_read(STDIN_FILENO, empty_ios);
        } else {
            char *file = *(argv + (my_optind++));
            int fd = open(file, O_RDONLY);
            if (fd == -1) {
                perror(file);
            } else {
                // OK to not do this atomically since we cannot have gone multithreaded yet.
                set_cloexec(fd);

                if (*(argv + my_optind)) {
                    wcstring sb;
                    char **ptr;
                    int i;
                    for (i = 1, ptr = argv + my_optind; *ptr; i++, ptr++) {
                        if (i != 1) sb.append(ARRAY_SEP_STR);
                        sb.append(str2wcstring(*ptr));
                    }

                    env_set(L"argv", sb.c_str(), 0);
                }

                const wcstring rel_filename = str2wcstring(file);

                reader_push_current_filename(rel_filename.c_str());

                res = reader_read(fd, empty_ios);

                if (res) {
                    debug(1, _(L"Error while reading file %ls\n"), reader_current_filename()
                                                                       ? reader_current_filename()
                                                                       : _(L"Standard input"));
                }
                reader_pop_current_filename();
            }
        }
    }

    int exit_status = res ? STATUS_UNKNOWN_COMMAND : proc_get_last_status();

    proc_fire_event(L"PROCESS_EXIT", EVENT_EXIT, getpid(), exit_status);

    restore_term_mode();
    restore_term_foreground_process_group();

    if (g_profiling_active) {
        parser.emit_profiling(s_profiling_output_filename);
    }

    history_destroy();
    proc_destroy();
    builtin_destroy();
    reader_destroy();
    event_destroy();
    exit_without_destructors(exit_status);
    return EXIT_FAILURE;  // above line should always exit
}
