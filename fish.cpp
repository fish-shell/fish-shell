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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


/** \file fish.c
	The main loop of <tt>fish</tt>.
*/

#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/param.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <locale.h>
#include <signal.h>

#include "fallback.h"
#include "util.h"

#include "common.h"
#include "reader.h"
#include "builtin.h"
#include "function.h"
#include "complete.h"
#include "wutil.h"
#include "env.h"
#include "sanity.h"
#include "proc.h"
#include "parser.h"
#include "expand.h"
#include "intern.h"
#include "exec.h"
#include "event.h"
#include "output.h"
#include "history.h"
#include "path.h"

/* PATH_MAX may not exist */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/**
   The string describing the single-character options accepted by the main fish binary
*/
#define GETOPT_STRING "+hilnvc:p:d:"

static bool has_suffix(const std::string &path, const char *suffix, bool ignore_case)
{
    size_t pathlen = path.size(), suffixlen = strlen(suffix);
    return pathlen >= suffixlen && !(ignore_case ? strcasecmp : strcmp)(path.c_str() + pathlen - suffixlen, suffix);
}

/* Modifies the given path by calling realpath. Returns true if realpath succeeded, false otherwise */
static bool get_realpath(std::string &path)
{
    char buff[PATH_MAX], *ptr;
    if ((ptr = realpath(path.c_str(), buff)))
    {
        path = ptr;
    }
    return ptr != NULL;
}

/* OS X function for getting the executable path */
extern "C" {
    int _NSGetExecutablePath(char* buf, uint32_t* bufsize);
}

/* Return the path to the current executable. This needs to be realpath'd. */
static std::string get_executable_path(const char *argv0)
{
    char buff[PATH_MAX];
#if __APPLE__
    {
        /* Returns 0 on success, -1 if the buffer is too small */
        uint32_t buffSize = sizeof buff;
        if (0 == _NSGetExecutablePath(buff, &buffSize))
            return std::string(buff);

        /* Loop until we're big enough */
        char *mbuff = (char *)malloc(buffSize);
        while (0 > _NSGetExecutablePath(mbuff, &buffSize))
            mbuff = (char *)realloc(mbuff, buffSize);

        /* Return the string */
        std::string result = mbuff;
        free(mbuff);
        return result;
    }
#endif
    {
        /* On other Unixes, try /proc directory. This might be worth breaking out into macros. */
        if (0 < readlink("/proc/self/exe", buff, sizeof buff) || // Linux
                0 < readlink("/proc/curproc/file", buff, sizeof buff) || // BSD
                0 < readlink("/proc/self/path/a.out", buff, sizeof buff)) // Solaris
        {
            return std::string(buff);
        }
    }

    /* Just return argv0, which probably won't work (i.e. it's not an absolute path or a path relative to the working directory, but instead something the caller found via $PATH). We'll eventually fall back to the compile time paths. */
    return std::string(argv0 ? argv0 : "");
}


static struct config_paths_t determine_config_directory_paths(const char *argv0)
{
    struct config_paths_t paths;
    bool done = false;
    std::string exec_path = get_executable_path(argv0);
    if (get_realpath(exec_path))
    {
#if __APPLE__

        /* On OS X, maybe we're an app bundle, and should use the bundle's files. Since we don't link CF, use this lame approach to test it: see if the resolved path ends with /Contents/MacOS/fish, case insensitive since HFS+ usually is.
         */
        if (! done)
        {
            const char *suffix = "/Contents/MacOS/fish";
            const size_t suffixlen = strlen(suffix);
            if (has_suffix(exec_path, suffix, true))
            {
                /* Looks like we're a bundle. Cut the string at the / prefixing /Contents... and then the rest */
                wcstring wide_resolved_path = str2wcstring(exec_path);
                wide_resolved_path.resize(exec_path.size() - suffixlen);
                wide_resolved_path.append(L"/Contents/Resources/");

                /* Append share, etc, doc */
                paths.data = wide_resolved_path + L"share/fish";
                paths.sysconf = wide_resolved_path + L"etc/fish";
                paths.doc = wide_resolved_path + L"doc/fish";

                /* But the bin_dir is the resolved_path, minus fish (aka the MacOS directory) */
                paths.bin = str2wcstring(exec_path);
                paths.bin.resize(paths.bin.size() - strlen("/fish"));

                done = true;
            }
        }
#endif

        if (! done)
        {
            /* The next check is that we are in a reloctable directory tree like this:
                 bin/fish
                 etc/fish
                 share/fish

                 Check it!
            */
            const char *suffix = "/bin/fish";
            if (has_suffix(exec_path, suffix, false))
            {
                wcstring base_path = str2wcstring(exec_path);
                base_path.resize(base_path.size() - strlen(suffix));

                paths.data = base_path + L"/share/fish";
                paths.sysconf = base_path + L"/etc/fish";
                paths.doc = base_path + L"/share/doc/fish";
                paths.bin = base_path + L"/bin";

                struct stat buf;
                if (0 == wstat(paths.data, &buf) && 0 == wstat(paths.sysconf, &buf))
                {
                    done = true;
                }
            }
        }
    }

    if (! done)
    {
        /* Fall back to what got compiled in. */
        paths.data = L"" DATADIR "/fish";
        paths.sysconf = L"" SYSCONFDIR "/fish";
        paths.doc = L"" DATADIR "/doc/fish";
        paths.bin = L"" BINDIR;

        done = true;
    }

    return paths;
}

/**
   Parse init files. exec_path is the path of fish executable as determined by argv[0].
*/
static int read_init(const struct config_paths_t &paths)
{
    parser_t &parser = parser_t::principal_parser();
    const io_chain_t empty_ios;
    parser.eval(L"builtin source " + paths.data + L"/config.fish 2>/dev/null", empty_ios, TOP);
    parser.eval(L"builtin source " + paths.sysconf + L"/config.fish 2>/dev/null", empty_ios, TOP);


    /*
      We need to get the configuration directory before we can source the user configuration file
    */
    wcstring config_dir;

    /*
      If path_get_config returns false then we have no configuration directory
      and no custom config to load.
    */
    if (path_get_config(config_dir))
    {
        wcstring config_dir_escaped = escape_string(config_dir, 1);
        wcstring eval_buff = format_string(L"builtin source %ls/config.fish 2>/dev/null", config_dir_escaped.c_str());
        parser.eval(eval_buff, empty_ios, TOP);
    }

    return 1;
}


/**
  Parse the argument list, return the index of the first non-switch
  arguments.
 */
static int fish_parse_opt(int argc, char **argv, std::vector<std::string> *out_cmds)
{
    int my_optind;
    int force_interactive=0;
    bool has_cmd = false;

    while (1)
    {
        static struct option
                long_options[] =
        {
            { "command", required_argument, 0, 'c' },
            { "debug-level", required_argument, 0, 'd' },
            { "interactive", no_argument, 0, 'i' } ,
            { "login", no_argument, 0, 'l' },
            { "no-execute", no_argument, 0, 'n' },
            { "profile", required_argument, 0, 'p' },
            { "help", no_argument, 0, 'h' },
            { "version", no_argument, 0, 'v' },
            { 0, 0, 0, 0 }
        }
        ;

        int opt_index = 0;

        int opt = getopt_long(argc,
                              argv,
                              GETOPT_STRING,
                              long_options,
                              &opt_index);

        if (opt == -1)
            break;

        switch (opt)
        {
            case 0:
            {
                break;
            }

            case 'c':
            {
                out_cmds->push_back(optarg ? optarg : "");
                has_cmd = true;
                is_interactive_session = 0;
                break;
            }

            case 'd':
            {
                char *end;
                long tmp;

                errno = 0;
                tmp = strtol(optarg, &end, 10);

                if (tmp >= 0 && tmp <=10 && !*end && !errno)
                {
                    debug_level = (int)tmp;
                }
                else
                {
                    debug(0, _(L"Invalid value '%s' for debug level switch"), optarg);
                    exit_without_destructors(1);
                }
                break;
            }

            case 'h':
            {
                out_cmds->push_back("__fish_print_help fish");
                has_cmd = true;
                break;
            }

            case 'i':
            {
                force_interactive = 1;
                break;
            }

            case 'l':
            {
                is_login=1;
                break;
            }

            case 'n':
            {
                no_exec=1;
                break;
            }

            case 'p':
            {
                profile = optarg;
                break;
            }

            case 'v':
            {
                fwprintf(stderr,
                         _(L"%s, version %s\n"),
                         PACKAGE_NAME,
                         FISH_BUILD_VERSION);
                exit_without_destructors(0);
            }

            case '?':
            {
                exit_without_destructors(1);
            }

        }
    }

    my_optind = optind;

    is_login |= (strcmp(argv[0], "-fish") == 0);

    /*
      We are an interactive session if we have not been given an
      explicit command to execute, _and_ stdin is a tty.
     */
    is_interactive_session &= ! has_cmd;
    is_interactive_session &= (my_optind == argc);
    is_interactive_session &= isatty(STDIN_FILENO);

    /*
      We are also an interactive session if we have are forced-
     */
    is_interactive_session |= force_interactive;

    return my_optind;
}

extern int g_fork_count;
int main(int argc, char **argv)
{
    int res=1;
    int my_optind=0;

    set_main_thread();
    setup_fork_guards();
    save_term_foreground_process_group();

    wsetlocale(LC_ALL, L"");
    is_interactive_session=1;
    program_name=L"fish";

    //struct stat tmp;
    //stat("----------FISH_HIT_MAIN----------", &tmp);

    std::vector<std::string> cmds;
    my_optind = fish_parse_opt(argc, argv, &cmds);

    /*
      No-exec is prohibited when in interactive mode
    */
    if (is_interactive_session && no_exec)
    {
        debug(1, _(L"Can not use the no-execute mode when running an interactive session"));
        no_exec = 0;
    }

    const struct config_paths_t paths = determine_config_directory_paths(argv[0]);

    proc_init();
    event_init();
    wutil_init();
    builtin_init();
    function_init();
    env_init(&paths);
    reader_init();
    history_init();

    parser_t &parser = parser_t::principal_parser();

    if (g_log_forks)
        printf("%d: g_fork_count: %d\n", __LINE__, g_fork_count);

    const io_chain_t empty_ios;
    if (read_init(paths))
    {
        /* Stop the exit status of any initialization commands (#635) */
        proc_set_last_status(STATUS_BUILTIN_OK);

        /* Run the commands specified as arguments, if any */
        if (! cmds.empty())
        {
            /* Do something nasty to support OpenSUSE assuming we're bash. This may modify cmds. */
            if (is_login)
            {
                fish_xdm_login_hack_hack_hack_hack(&cmds, argc - my_optind, argv + my_optind);
            }
            for (size_t i=0; i < cmds.size(); i++)
            {
                const wcstring cmd_wcs = str2wcstring(cmds.at(i));
                res = parser.eval(cmd_wcs, empty_ios, TOP);
            }
            reader_exit(0, 0);
        }
        else
        {
            if (my_optind == argc)
            {
                res = reader_read(STDIN_FILENO, empty_ios);
            }
            else
            {
                char **ptr;
                char *file = *(argv+(my_optind++));
                int i;
                int fd;


                if ((fd = open(file, O_RDONLY)) == -1)
                {
                    wperror(L"open");
                    return 1;
                }

                // OK to not do this atomically since we cannot have gone multithreaded yet
                set_cloexec(fd);

                if (*(argv+my_optind))
                {
                    wcstring sb;
                    for (i=1,ptr = argv+my_optind; *ptr; i++, ptr++)
                    {
                        if (i != 1)
                            sb.append(ARRAY_SEP_STR);
                        sb.append(str2wcstring(*ptr));
                    }

                    env_set(L"argv", sb.c_str(), 0);
                }

                const wcstring rel_filename = str2wcstring(file);
                const wchar_t *abs_filename = wrealpath(rel_filename, NULL);

                if (!abs_filename)
                {
                    abs_filename = wcsdup(rel_filename.c_str());
                }

                reader_push_current_filename(intern(abs_filename));
                free((void *)abs_filename);

                res = reader_read(fd, empty_ios);

                if (res)
                {
                    debug(1,
                          _(L"Error while reading file %ls\n"),
                          reader_current_filename()?reader_current_filename(): _(L"Standard input"));
                }
                reader_pop_current_filename();
            }
        }
    }

    proc_fire_event(L"PROCESS_EXIT", EVENT_EXIT, getpid(), res);

    restore_term_foreground_process_group();
    history_destroy();
    proc_destroy();
    builtin_destroy();
    reader_destroy();
    parser.destroy();
    wutil_destroy();
    event_destroy();

    env_destroy();

    if (g_log_forks)
        printf("%d: g_fork_count: %d\n", __LINE__, g_fork_count);

    exit_without_destructors(res ? STATUS_UNKNOWN_COMMAND : proc_get_last_status());
    return EXIT_FAILURE; //above line should always exit
}
