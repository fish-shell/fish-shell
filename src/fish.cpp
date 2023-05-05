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

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "common.h"
#include "cxxgen.h"
#include "env.h"
#include "event.h"
#include "expand.h"
#include "fallback.h"  // IWYU pragma: keep
#include "fds.h"
#include "ffi_baggage.h"
#include "ffi_init.rs.h"
#include "fish_version.h"
#include "flog.h"
#include "function.h"
#include "future_feature_flags.h"
#include "global_safety.h"
#include "history.h"
#include "io.h"
#include "maybe.h"
#include "parse_constants.h"
#include "parse_tree.h"
#include "parse_util.h"
#include "parser.h"
#include "path.h"
#include "proc.h"
#include "reader.h"
#include "signals.h"
#include "threads.rs.h"
#include "wcstringutil.h"
#include "wutil.h"  // IWYU pragma: keep

// container to hold the options specified within the command line
class fish_cmd_opts_t {
   public:
    // Future feature flags values string
    wcstring features;
    // File path for debug output.
    std::string debug_output;
    // File path for profiling output, or empty for none.
    std::string profile_output;
    std::string profile_startup_output;
    // Commands to be executed in place of interactive shell.
    std::vector<std::string> batch_cmds;
    // Commands to execute after the shell's config has been read.
    std::vector<std::string> postconfig_cmds;
    /// Whether to print rusage-self stats after execution.
    bool print_rusage_self{false};
    /// Whether no-config is set.
    bool no_config{false};
    /// Whether no-exec is set.
    bool no_exec{false};
    /// Whether this is a login shell.
    bool is_login{false};
    /// Whether this is an interactive session.
    bool is_interactive_session{false};
    /// Whether to enable private mode.
    bool enable_private_mode{false};
};

/// \return a timeval converted to milliseconds.
static long long tv_to_msec(const struct timeval &tv) {
    long long msec = static_cast<long long>(tv.tv_sec) * 1000;  // milliseconds per second
    msec += tv.tv_usec / 1000;                                  // microseconds per millisecond
    return msec;
}

static void print_rusage_self(FILE *fp) {
#ifndef HAVE_GETRUSAGE
    fprintf(fp, "getrusage() not supported on this platform");
    return;
#else
    struct rusage rs;
    if (getrusage(RUSAGE_SELF, &rs)) {
        perror("getrusage");
        return;
    }
#if defined(__APPLE__) && defined(__MACH__)
    // Macs use bytes.
    long rss_kb = rs.ru_maxrss / 1024;
#else
    // Everyone else uses KB.
    long rss_kb = rs.ru_maxrss;
#endif
    fprintf(fp, "  rusage self:\n");
    fprintf(fp, "      user time: %lld ms\n", tv_to_msec(rs.ru_utime));
    fprintf(fp, "       sys time: %lld ms\n", tv_to_msec(rs.ru_stime));
    fprintf(fp, "     total time: %lld ms\n", tv_to_msec(rs.ru_utime) + tv_to_msec(rs.ru_stime));
    fprintf(fp, "        max rss: %ld kb\n", rss_kb);
    fprintf(fp, "        signals: %ld\n", rs.ru_nsignals);
#endif
}

static bool has_suffix(const std::string &path, const char *suffix, bool ignore_case) {
    size_t pathlen = path.size(), suffixlen = std::strlen(suffix);
    return pathlen >= suffixlen &&
           !(ignore_case ? strcasecmp : std::strcmp)(path.c_str() + pathlen - suffixlen, suffix);
}

/// Modifies the given path by calling realpath. Returns true if realpath succeeded, false
/// otherwise.
static bool get_realpath(std::string &path) {
    char buff[PATH_MAX], *ptr;
    if ((ptr = realpath(path.c_str(), buff))) {
        path = ptr;
    }
    return ptr != nullptr;
}

static struct config_paths_t determine_config_directory_paths(const char *argv0) {
    struct config_paths_t paths;
    bool done = false;
    std::string exec_path = get_executable_path(argv0);
    if (get_realpath(exec_path)) {
        FLOGF(config, L"exec_path: '%s', argv[0]: '%s'", exec_path.c_str(), argv0);
        // TODO: we should determine program_name from argv0 somewhere in this file

#ifdef CMAKE_BINARY_DIR
        // Detect if we're running right out of the CMAKE build directory
        if (string_prefixes_string(CMAKE_BINARY_DIR, exec_path.c_str())) {
            FLOGF(config,
                  "Running out of build directory, using paths relative to CMAKE_SOURCE_DIR:\n %s",
                  CMAKE_SOURCE_DIR);

            done = true;
            paths.data = wcstring{L"" CMAKE_SOURCE_DIR} + L"/share";
            paths.sysconf = wcstring{L"" CMAKE_SOURCE_DIR} + L"/etc";
            paths.doc = wcstring{L"" CMAKE_SOURCE_DIR} + L"/user_doc/html";
            paths.bin = wcstring{L"" CMAKE_BINARY_DIR};
        }
#endif

        if (!done) {
            // The next check is that we are in a reloctable directory tree
            const char *installed_suffix = "/bin/fish";
            const char *just_a_fish = "/fish";
            const char *suffix = nullptr;

            if (has_suffix(exec_path, installed_suffix, false)) {
                suffix = installed_suffix;
            } else if (has_suffix(exec_path, just_a_fish, false)) {
                FLOGF(config, L"'fish' not in a 'bin/', trying paths relative to source tree");
                suffix = just_a_fish;
            }

            if (suffix) {
                bool seems_installed = (suffix == installed_suffix);

                wcstring base_path = str2wcstring(exec_path);
                base_path.resize(base_path.size() - std::strlen(suffix));

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
        FLOGF(config, L"Using compiled in paths:");
        paths.data = L"" DATADIR "/fish";
        paths.sysconf = L"" SYSCONFDIR "/fish";
        paths.doc = L"" DOCDIR;
        paths.bin = L"" BINDIR;
    }

    FLOGF(config,
          L"determine_config_directory_paths() results:\npaths.data: %ls\npaths.sysconf: "
          L"%ls\npaths.doc: %ls\npaths.bin: %ls",
          paths.data.c_str(), paths.sysconf.c_str(), paths.doc.c_str(), paths.bin.c_str());
    return paths;
}

// Source the file config.fish in the given directory.
static void source_config_in_directory(parser_t &parser, const wcstring &dir) {
    // If the config.fish file doesn't exist or isn't readable silently return. Fish versions up
    // thru 2.2.0 would instead try to source the file with stderr redirected to /dev/null to deal
    // with that possibility.
    //
    // This introduces a race condition since the readability of the file can change between this
    // test and the execution of the 'source' command. However, that is not a security problem in
    // this context so we ignore it.
    const wcstring config_pathname = dir + L"/config.fish";
    const wcstring escaped_pathname = escape_string(dir) + L"/config.fish";
    if (waccess(config_pathname, R_OK) != 0) {
        FLOGF(config, L"not sourcing %ls (not readable or does not exist)",
              escaped_pathname.c_str());
        return;
    }
    FLOGF(config, L"sourcing %ls", escaped_pathname.c_str());

    const wcstring cmd = L"builtin source " + escaped_pathname;

    parser.libdata().within_fish_init = true;
    parser.eval(cmd, io_chain_t());
    parser.libdata().within_fish_init = false;
}

/// Parse init files. exec_path is the path of fish executable as determined by argv[0].
static void read_init(parser_t &parser, const struct config_paths_t &paths) {
    source_config_in_directory(parser, paths.data);
    source_config_in_directory(parser, paths.sysconf);

    // We need to get the configuration directory before we can source the user configuration file.
    // If path_get_config returns false then we have no configuration directory and no custom config
    // to load.
    wcstring config_dir;
    if (path_get_config(config_dir)) {
        source_config_in_directory(parser, config_dir);
    }
}

static int run_command_list(parser_t &parser, const std::vector<std::string> &cmds,
                            const io_chain_t &io) {
    for (const auto &cmd : cmds) {
        wcstring cmd_wcs = str2wcstring(cmd);
        // Parse into an ast and detect errors.
        auto errors = new_parse_error_list();
        auto ast = ast_parse(cmd_wcs, parse_flag_none, &*errors);
        bool errored = ast->errored();
        if (!errored) {
            errored = parse_util_detect_errors(*ast, cmd_wcs, &*errors);
        }
        if (!errored) {
            // Construct a parsed source ref.
            // Be careful to transfer ownership, this could be a very large string.
            auto ps = new_parsed_source_ref(cmd_wcs, *ast);
            parser.ffi_eval_parsed_source(*ps, io);
        } else {
            wcstring sb;
            parser.get_backtrace(cmd_wcs, *errors, sb);
            std::fwprintf(stderr, L"%ls", sb.c_str());
        }
    }

    return 0;
}

/// Parse the argument list, return the index of the first non-flag arguments.
static int fish_parse_opt(int argc, char **argv, fish_cmd_opts_t *opts) {
    static const char *const short_opts = "+hPilNnvc:C:p:d:f:D:o:";
    static const struct option long_opts[] = {
        {"command", required_argument, nullptr, 'c'},
        {"init-command", required_argument, nullptr, 'C'},
        {"features", required_argument, nullptr, 'f'},
        {"debug", required_argument, nullptr, 'd'},
        {"debug-output", required_argument, nullptr, 'o'},
        {"debug-stack-frames", required_argument, nullptr, 'D'},
        {"interactive", no_argument, nullptr, 'i'},
        {"login", no_argument, nullptr, 'l'},
        {"no-config", no_argument, nullptr, 'N'},
        {"no-execute", no_argument, nullptr, 'n'},
        {"print-rusage-self", no_argument, nullptr, 1},
        {"print-debug-categories", no_argument, nullptr, 2},
        {"profile", required_argument, nullptr, 'p'},
        {"profile-startup", required_argument, nullptr, 3},
        {"private", no_argument, nullptr, 'P'},
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'v'},
        {}};

    int opt;
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'c': {
                opts->batch_cmds.emplace_back(optarg);
                break;
            }
            case 'C': {
                opts->postconfig_cmds.emplace_back(optarg);
                break;
            }
            case 'd': {
                activate_flog_categories_by_pattern(str2wcstring(optarg));
                rust_activate_flog_categories_by_pattern(str2wcstring(optarg).c_str());
                for (auto cat : get_flog_categories()) {
                    if (cat->enabled) {
                        std::fwprintf(stdout, L"Debug enabled for category: %ls\n", cat->name);
                    }
                }
                break;
            }
            case 'o': {
                opts->debug_output = optarg;
                break;
            }
            case 'f': {
                opts->features = str2wcstring(optarg);
                break;
            }
            case 'h': {
                opts->batch_cmds.emplace_back("__fish_print_help fish");
                break;
            }
            case 'i': {
                opts->is_interactive_session = true;
                break;
            }
            case 'l': {
                opts->is_login = true;
                break;
            }
            case 'N': {
                opts->no_config = true;
                // --no-config implies private mode, we won't be saving history
                opts->enable_private_mode = true;
                break;
            }
            case 'n': {
                opts->no_exec = true;
                break;
            }
            case 1: {
                opts->print_rusage_self = true;
                break;
            }
            case 2: {
                auto cats = get_flog_categories();
                // Compute width of longest name.
                int name_width = 0;
                for (auto cat : cats) {
                    name_width = std::max(name_width, static_cast<int>(wcslen(cat->name)));
                }
                // A little extra space.
                name_width += 2;
                for (auto cat : cats) {
                    // Negating the name width left-justifies.
                    printf("%*ls %ls\n", -name_width, cat->name, _(cat->description));
                }
                exit(0);
            }
            case 'p': {
                // "--profile" - this does not activate profiling right away,
                // rather it's done after startup is finished.
                opts->profile_output = optarg;
                break;
            }
            case 3: {
                // With "--profile-startup" we immediately turn profiling on.
                opts->profile_startup_output = optarg;
                g_profiling_active = true;
                break;
            }
            case 'P': {
                opts->enable_private_mode = true;
                break;
            }
            case 'v': {
                std::fwprintf(stdout, _(L"%s, version %s\n"), PACKAGE_NAME, get_fish_version());
                exit(0);
            }
            case 'D': {
                // TODO: Option is currently useless.
                // Either remove it or make it work with FLOG.
                break;
            }
            default: {
                // We assume getopt_long() has already emitted a diagnostic msg.
                exit(1);
            }
        }
    }

    // If our command name begins with a dash that implies we're a login shell.
    opts->is_login |= argv[0][0] == '-';

    // We are an interactive session if we have not been given an explicit
    // command or file to execute and stdin is a tty. Note that the -i or
    // --interactive options also force interactive mode.
    if (opts->batch_cmds.empty() && optind == argc && isatty(STDIN_FILENO)) {
        set_interactive_session(true);
    }

    return optind;
}

int main(int argc, char **argv) {
    int res = 1;
    int my_optind = 0;

    program_name = L"fish";
    rust_init();
    signal_unblock_all();

    setlocale(LC_ALL, "");

    const char *dummy_argv[2] = {"fish", nullptr};
    if (!argv[0]) {
        argv = const_cast<char **>(dummy_argv);  //!OCLINT(parameter reassignment)
        argc = 1;                                //!OCLINT(parameter reassignment)
    }

    // Enable debug categories set in FISH_DEBUG.
    // This is in *addition* to the ones given via --debug.
    if (const char *debug_categories = getenv("FISH_DEBUG")) {
        activate_flog_categories_by_pattern(str2wcstring(debug_categories));
    }

    fish_cmd_opts_t opts{};
    my_optind = fish_parse_opt(argc, argv, &opts);

    // Direct any debug output right away.
    // --debug-output takes precedence, otherwise $FISH_DEBUG_OUTPUT is used.
    if (opts.debug_output.empty()) {
        const char *var = getenv("FISH_DEBUG_OUTPUT");
        if (var) opts.debug_output = var;
    }

    FILE *debug_output = nullptr;
    if (!opts.debug_output.empty()) {
        debug_output = fopen(opts.debug_output.c_str(), "w");
        if (!debug_output) {
            fprintf(stderr, "Could not open file %s\n", opts.debug_output.c_str());
            perror("fopen");
            exit(-1);
        }
        set_cloexec(fileno(debug_output));
        setlinebuf(debug_output);
        set_flog_output_file(debug_output);
    }

    // No-exec is prohibited when in interactive mode.
    if (opts.is_interactive_session && opts.no_exec) {
        FLOGF(warning, _(L"Can not use the no-execute mode when running an interactive session"));
        opts.no_exec = false;
    }

    // Apply our options.
    if (opts.is_login) mark_login();
    if (opts.no_exec) mark_no_exec();
    if (opts.is_interactive_session) set_interactive_session(true);
    if (opts.enable_private_mode) start_private_mode(env_stack_t::globals());

    // Only save (and therefore restore) the fg process group if we are interactive. See issues
    // #197 and #1002.
    if (is_interactive_session()) {
        save_term_foreground_process_group();
    }

    struct config_paths_t paths;
    // If we're not executing, there's no need to find the config.
    if (!opts.no_exec) {
        paths = determine_config_directory_paths(argv[0]);
        env_init(&paths, /* do uvars */ !opts.no_config, /* default paths */ opts.no_config);
    }

    // Set features early in case other initialization depends on them.
    // Start with the ones set in the environment, then those set on the command line (so the
    // command line takes precedence).
    if (auto features_var = env_stack_t::globals().get(L"fish_features")) {
        for (const wcstring &s : features_var->as_list()) {
            mutable_fish_features()->set_from_string(s.c_str());
        }
    }
    mutable_fish_features()->set_from_string(opts.features.c_str());
    proc_init();
    misc_init();
    reader_init();

    parser_t &parser = parser_t::principal_parser();
    parser.set_syncs_uvars(!opts.no_config);

    if (!opts.no_exec && !opts.no_config) {
        read_init(parser, paths);
    }

    if (is_interactive_session() && opts.no_config && !opts.no_exec) {
        // If we have no config, we default to the default key bindings.
        parser.vars().set_one(L"fish_key_bindings", ENV_UNEXPORT, L"fish_default_key_bindings");
        if (function_exists(L"fish_default_key_bindings", parser)) {
            run_command_list(parser, {"fish_default_key_bindings"}, {});
        }
    }

    // Re-read the terminal modes after config, it might have changed them.
    term_copy_modes();

    // Stomp the exit status of any initialization commands (issue #635).
    parser.set_last_statuses(statuses_t::just(STATUS_CMD_OK));

    // If we're profiling startup to a separate file, write it now.
    if (!opts.profile_startup_output.empty() &&
        opts.profile_startup_output != opts.profile_output) {
        parser.emit_profiling(opts.profile_startup_output.c_str());

        // If we are profiling both, ensure the startup data only
        // ends up in the startup file.
        parser.clear_profiling();
    }

    g_profiling_active = !opts.profile_output.empty();

    // Run post-config commands specified as arguments, if any.
    if (!opts.postconfig_cmds.empty()) {
        res = run_command_list(parser, opts.postconfig_cmds, {});
    }

    // Clear signals in case we were interrupted (#9024).
    signal_clear_cancel();

    if (!opts.batch_cmds.empty()) {
        // Run the commands specified as arguments, if any.
        if (get_login()) {
            // Do something nasty to support OpenSUSE assuming we're bash. This may modify cmds.
            fish_xdm_login_hack_hack_hack_hack(&opts.batch_cmds, argc - my_optind,
                                               argv + my_optind);
        }

        // Pass additional args as $argv.
        // Note that we *don't* support setting argv[0]/$0, unlike e.g. bash.
        std::vector<wcstring> list;
        for (char **ptr = argv + my_optind; *ptr; ptr++) {
            list.push_back(str2wcstring(*ptr));
        }
        parser.vars().set(L"argv", ENV_DEFAULT, std::move(list));
        res = run_command_list(parser, opts.batch_cmds, {});
        parser.libdata().exit_current_script = false;
    } else if (my_optind == argc) {
        // Implicitly interactive mode.
        if (opts.no_exec && isatty(STDIN_FILENO)) {
            FLOGF(error, L"no-execute mode enabled and no script given. Exiting");
            return EXIT_FAILURE;  // above line should always exit
        }
        res = reader_read(parser, STDIN_FILENO, {});
    } else {
        const char *file = *(argv + (my_optind++));
        autoclose_fd_t fd(open_cloexec(file, O_RDONLY));
        if (!fd.valid()) {
            FLOGF(error, _(L"Error reading script file '%s':"), file);
            perror("error");
        } else {
            std::vector<wcstring> list;
            for (char **ptr = argv + my_optind; *ptr; ptr++) {
                list.push_back(str2wcstring(*ptr));
            }
            parser.vars().set(L"argv", ENV_DEFAULT, std::move(list));

            auto &ld = parser.libdata();
            filename_ref_t rel_filename = std::make_shared<wcstring>(str2wcstring(file));
            scoped_push<filename_ref_t> filename_push{&ld.current_filename, rel_filename};
            res = reader_read(parser, fd.fd(), {});
            if (res) {
                FLOGF(warning, _(L"Error while reading file %ls\n"), rel_filename->c_str());
            }
        }
    }

    int exit_status = res ? STATUS_CMD_UNKNOWN : parser.get_last_status();
    event_fire(parser, *new_event_process_exit(getpid(), exit_status));

    // Trigger any exit handlers.
    event_fire_generic(parser, L"fish_exit", {to_string(exit_status)});

    restore_term_mode();
    restore_term_foreground_process_group_for_exit();

    if (!opts.profile_output.empty()) {
        parser.emit_profiling(opts.profile_output.c_str());
    }

    history_save_all();
    if (opts.print_rusage_self) {
        print_rusage_self(stderr);
    }
    if (debug_output) {
        fclose(debug_output);
    }
    asan_maybe_exit(exit_status);
    exit_without_destructors(exit_status);
    return EXIT_FAILURE;  // above line should always exit
}
