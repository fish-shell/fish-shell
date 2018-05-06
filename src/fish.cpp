//
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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
#include "future_feature_flags.h"
#include "history.h"
#include "io.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "signal.h"
#include "wutil.h"  // IWYU pragma: keep

// PATH_MAX may not exist.
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// container to hold the options specified within the command line
class fish_cmd_opts_t {
   public:
    // Future feature flags values string
    wcstring features;
    // Commands to be executed in place of interactive shell.
    std::vector<std::string> batch_cmds;
    // Commands to execute after the shell's config has been read.
    std::vector<std::string> postconfig_cmds;
};

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
    // This is basically grabbing exec_path after argc, argv, envp, ...: for us
    // https://opensource.apple.com/source/adv_cmds/adv_cmds-163/ps/print.c
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
        debug(2, L"exec_path: '%s'", exec_path.c_str());
        if (!done) {
            // The next check is that we are in a reloctable directory tree
            const char *installed_suffix = "/bin/fish";
            const char *just_a_fish = "/fish";
            const char *suffix = NULL;

            if (has_suffix(exec_path, installed_suffix, false)) {
                suffix = installed_suffix;
            } else if (has_suffix(exec_path, just_a_fish, false)) {
                debug(2, L"'fish' not in a 'bin/', trying paths relative to source tree");
                suffix = just_a_fish;
            }

            if (suffix) {
                bool seems_installed = (suffix == installed_suffix);

                wcstring base_path = str2wcstring(exec_path);
                base_path.resize(base_path.size() - strlen(suffix));

                paths.data = base_path + (seems_installed ? L"/share/fish" : L"/share");
                paths.sysconf = base_path + (seems_installed ? L"/etc/fish" : L"/etc");
                paths.doc = base_path + (seems_installed ? L"/share/doc/fish" : L"/user_doc/html");
                paths.bin = base_path + (seems_installed ? L"/bin" : L"");

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
        debug(2, L"Using compiled in paths:");
        paths.data = L"" DATADIR "/fish";
        paths.sysconf = L"" SYSCONFDIR "/fish";
        paths.doc = L"" DOCDIR;
        paths.bin = L"" BINDIR;
    }

    debug(2,
          L"determine_config_directory_paths() results:\npaths.data: %ls\npaths.sysconf: "
          L"%ls\npaths.doc: %ls\npaths.bin: %ls",
          paths.data.c_str(), paths.sysconf.c_str(), paths.doc.c_str(), paths.bin.c_str());
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

int run_command_list(std::vector<std::string> *cmds, const io_chain_t &io) {
    int res = 1;
    parser_t &parser = parser_t::principal_parser();

    for (size_t i = 0; i < cmds->size(); i++) {
        const wcstring cmd_wcs = str2wcstring(cmds->at(i));
        res = parser.eval(cmd_wcs, io, TOP);
    }

    return res;
}

/// Parse the argument list, return the index of the first non-flag arguments.
static int fish_parse_opt(int argc, char **argv, fish_cmd_opts_t *opts) {
    static const char *short_opts = "+hilnvc:C:p:d:f:D:";
    static const struct option long_opts[] = {{"command", required_argument, NULL, 'c'},
                                              {"init-command", required_argument, NULL, 'C'},
                                              {"features", required_argument, NULL, 'f'},
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
            case 'c': {
                opts->batch_cmds.push_back(optarg);
                break;
            }
            case 'C': {
                opts->postconfig_cmds.push_back(optarg);
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
            case 'f': {
                opts->features = str2wcstring(optarg);
                break;
            }
            case 'h': {
                opts->batch_cmds.push_back("__fish_print_help fish");
                break;
            }
            case 'i': {
                is_interactive_session = true;
                break;
            }
            case 'l': {
                is_login = true;
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
                break;
            }
        }
    }

    // If our command name begins with a dash that implies we're a login shell.
    is_login |= argv[0][0] == '-';

    // We are an interactive session if we have not been given an explicit
    // command or file to execute and stdin is a tty. Note that the -i or
    // --interactive options also force interactive mode.
    if (opts->batch_cmds.size() == 0 && optind == argc && isatty(STDIN_FILENO)) {
        is_interactive_session = 1;
    }

    return optind;
}

int main(int argc, char **argv) {
    int res = 1;
    int my_optind = 0;

    program_name = L"fish";
    set_main_thread();
    setup_fork_guards();
    signal_unblock_all();
    setlocale(LC_ALL, "");
    fish_setlocale();

    // struct stat tmp;
    // stat("----------FISH_HIT_MAIN----------", &tmp);

    if (!argv[0]) {
        static const char *dummy_argv[2] = {"fish", NULL};
        argv = (char **)dummy_argv;  //!OCLINT(parameter reassignment)
        argc = 1;                    //!OCLINT(parameter reassignment)
    }
    fish_cmd_opts_t opts;
    my_optind = fish_parse_opt(argc, argv, &opts);

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
    env_init(&paths);
    // Set features early in case other initialization depends on them.
    // Start with the ones set in the environment, then those set on the command line (so the
    // command line takes precedence).
    if (auto features_var = env_get(L"fish_features")) {
        for (const wcstring &s : features_var->as_list()) {
            mutable_fish_features().set_from_string(s);
        }
    }
    mutable_fish_features().set_from_string(opts.features);
    proc_init();
    builtin_init();
    misc_init();
    reader_init();

    parser_t &parser = parser_t::principal_parser();

    const io_chain_t empty_ios;
    if (read_init(paths)) {
        // Stomp the exit status of any initialization commands (issue #635).
        proc_set_last_status(STATUS_CMD_OK);

        // Run post-config commands specified as arguments, if any.
        if (!opts.postconfig_cmds.empty()) {
            res = run_command_list(&opts.postconfig_cmds, empty_ios);
        }

        if (!opts.batch_cmds.empty()) {
            // Run the commands specified as arguments, if any.
            if (is_login) {
                // Do something nasty to support OpenSUSE assuming we're bash. This may modify cmds.
                fish_xdm_login_hack_hack_hack_hack(&opts.batch_cmds, argc - my_optind,
                                                   argv + my_optind);
            }
            res = run_command_list(&opts.batch_cmds, empty_ios);
            reader_exit(0, 0);
        } else if (my_optind == argc) {
            // Implicitly interactive mode.
            res = reader_read(STDIN_FILENO, empty_ios);
        } else {
            char *file = *(argv + (my_optind++));
            int fd = open(file, O_RDONLY);
            if (fd == -1) {
                perror(file);
            } else {
                // OK to not do this atomically since we cannot have gone multithreaded yet.
                set_cloexec(fd);

                wcstring_list_t list;
                for (char **ptr = argv + my_optind; *ptr; ptr++) {
                    list.push_back(str2wcstring(*ptr));
                }
                env_set(L"argv", ENV_DEFAULT, list);

                const wcstring rel_filename = str2wcstring(file);

                reader_push_current_filename(rel_filename.c_str());

                res = reader_read(fd, empty_ios);

                if (res) {
                    debug(1, _(L"Error while reading file %ls\n"),
                          reader_current_filename() ? reader_current_filename()
                                                    : _(L"Standard input"));
                }
                reader_pop_current_filename();
            }
        }
    }

    int exit_status = res ? STATUS_CMD_UNKNOWN : proc_get_last_status();

    // TODO: The generic process-exit event is useless and unused.
    // Remove this in future.
    proc_fire_event(L"PROCESS_EXIT", EVENT_EXIT, getpid(), exit_status);
    event_fire_generic(L"fish_exit");

    restore_term_mode();
    restore_term_foreground_process_group();

    if (g_profiling_active) {
        parser.emit_profiling(s_profiling_output_filename);
    }

    history_save_all();
    proc_destroy();
    exit_without_destructors(exit_status);
    return EXIT_FAILURE;  // above line should always exit
}
