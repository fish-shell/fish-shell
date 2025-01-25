# This file does some internal fish setup.
# It is not recommended to remove or edit it.
#
# Set default field separators
#
set -g IFS \n\ \t
set -qg __fish_added_user_paths
or set -g __fish_added_user_paths

#
# Create the default command_not_found handler
#
function __fish_default_command_not_found_handler
    printf (_ "fish: Unknown command: %s\n") (string escape -- $argv[1]) >&2
end

if not status --is-interactive
    # Hook up the default as the command_not_found handler
    # if we are not interactive to avoid custom handlers.
    function fish_command_not_found --on-event fish_command_not_found
        __fish_default_command_not_found_handler $argv
    end
end

#
# Set default search paths for completions and shellscript functions
# unless they already exist
#

# __fish_data_dir, __fish_sysconf_dir, __fish_help_dir, __fish_bin_dir
# are expected to have been set up by read_init from fish.cpp

# Grab extra directories (as specified by the build process, usually for
# third-party packages to ship completions &c.
set -l __extra_completionsdir
set -l __extra_functionsdir
set -l __extra_confdir
if test -f $__fish_data_dir/__fish_build_paths.fish
    source $__fish_data_dir/__fish_build_paths.fish
end

# Compute the directories for vendor configuration.  We want to include
# all of XDG_DATA_DIRS, as well as the __extra_* dirs defined above.
set -l xdg_data_dirs
if set -q XDG_DATA_DIRS
    set --path xdg_data_dirs $XDG_DATA_DIRS
    set xdg_data_dirs (string replace -r '([^/])/$' '$1' -- $xdg_data_dirs)/fish
else
    set xdg_data_dirs $__fish_data_dir
end

set -g __fish_vendor_completionsdirs
set -g __fish_vendor_functionsdirs
set -g __fish_vendor_confdirs
# Don't load vendor directories when running unit tests
if not set -q FISH_UNIT_TESTS_RUNNING
    set __fish_vendor_completionsdirs $__fish_user_data_dir/vendor_completions.d $xdg_data_dirs/vendor_completions.d
    set __fish_vendor_functionsdirs $__fish_user_data_dir/vendor_functions.d $xdg_data_dirs/vendor_functions.d
    set __fish_vendor_confdirs $__fish_user_data_dir/vendor_conf.d $xdg_data_dirs/vendor_conf.d

    # Ensure that extra directories are always included.
    if not contains -- $__extra_completionsdir $__fish_vendor_completionsdirs
        set -a __fish_vendor_completionsdirs $__extra_completionsdir
    end
    if not contains -- $__extra_functionsdir $__fish_vendor_functionsdirs
        set -a __fish_vendor_functionsdirs $__extra_functionsdir
    end
    if not contains -- $__extra_confdir $__fish_vendor_confdirs
        set -a __fish_vendor_confdirs $__extra_confdir
    end
end

# Set up function and completion paths. Make sure that the fish
# default functions/completions are included in the respective path.

if not set -q fish_function_path
    set fish_function_path $__fish_config_dir/functions $__fish_sysconf_dir/functions $__fish_vendor_functionsdirs $__fish_data_dir/functions
else if not contains -- $__fish_data_dir/functions $fish_function_path
    set -a fish_function_path $__fish_data_dir/functions
end

if not set -q fish_complete_path
    set fish_complete_path $__fish_config_dir/completions $__fish_sysconf_dir/completions $__fish_vendor_completionsdirs $__fish_data_dir/completions $__fish_cache_dir/generated_completions
else if not contains -- $__fish_data_dir/completions $fish_complete_path
    set -a fish_complete_path $__fish_data_dir/completions
end

# Add a handler for when fish_user_path changes, so we can apply the same changes to PATH
function __fish_reconstruct_path -d "Update PATH when fish_user_paths changes" --on-variable fish_user_paths
    # Deduplicate $fish_user_paths
    # This should help with people appending to it in config.fish
    set -l new_user_path
    for path in (string split : -- $fish_user_paths)
        if not contains -- $path $new_user_path
            set -a new_user_path $path
        end
    end

    if test (count $new_user_path) -lt (count $fish_user_paths)
        # This will end up calling us again, so we return
        set fish_user_paths $new_user_path
        return
    end

    set -l local_path $PATH

    for x in $__fish_added_user_paths
        set -l idx (contains --index -- $x $local_path)
        and set -e local_path[$idx]
    end

    set -g __fish_added_user_paths
    if set -q fish_user_paths
        # Explicitly split on ":" because $fish_user_paths might not be a path variable,
        # but $PATH definitely is.
        for x in (string split ":" -- $fish_user_paths[-1..1])
            if set -l idx (contains --index -- $x $local_path)
                set -e local_path[$idx]
            else
                set -ga __fish_added_user_paths $x
            end
            set -p local_path $x
        end
    end

    set -xg PATH $local_path
end

#
# Launch debugger on SIGTRAP
#
function fish_sigtrap_handler --on-signal TRAP --no-scope-shadowing --description "TRAP handler: debug prompt"
    breakpoint
end

#
# When a prompt is first displayed, make sure that interactive
# mode-specific initializations have been performed.
# This includes a `read` prompt, hence the fish_read event.
# This handler removes itself after it is first called.
#
function __fish_on_interactive --on-event fish_prompt --on-event fish_read
    # We erase this *first* so it can't be called again,
    # e.g. if fish_greeting calls "read".
    functions -e __fish_on_interactive
    __fish_config_interactive
end

# Set the locale if it isn't explicitly set. Allowing the lack of locale env vars to imply the
# C/POSIX locale causes too many problems. Do this before reading the snippets because they might be
# in UTF-8 (with non-ASCII characters).
not set -q LANG # (fast path - no need to load the file if we have $LANG)
and __fish_set_locale

#
# Some things should only be done for login terminals
# This used to be in etc/config.fish - keep it here to keep the semantics
#
if status --is-login
    if command -sq /usr/libexec/path_helper
        # Adapt construct_path from the macOS /usr/libexec/path_helper
        # executable for fish; see
        # https://opensource.apple.com/source/shell_cmds/shell_cmds-203/path_helper/path_helper.c.auto.html .
        function __fish_macos_set_env -d "set an environment variable like path_helper does (macOS only)"
            set -l result

            # Populate path according to config files
            for path_file in $argv[2] $argv[3]/*
                for entry in (string split : <? $path_file)
                    if not contains -- $entry $result
                        test -n "$entry"
                        and set -a result $entry
                    end
                end
            end

            # Merge in any existing path elements
            for existing_entry in $$argv[1]
                if not contains -- $existing_entry $result
                    set -a result $existing_entry
                end
            end

            set -xg $argv[1] $result
        end

        __fish_macos_set_env PATH /etc/paths '/etc/paths.d'
        if test -n "$MANPATH"
            __fish_macos_set_env MANPATH /etc/manpaths '/etc/manpaths.d'
        end
        functions -e __fish_macos_set_env
    end

    #
    # Put linux consoles in unicode mode.
    #
    if test "$TERM" = linux
        and string match -qir '\.UTF' -- $LANG
        and command -sq unicode_start
        unicode_start
    end
end

# Invoke this here to apply the current value of fish_user_path after
# PATH is possibly set above.
__fish_reconstruct_path

# Allow %n job expansion to be used with fg/bg/wait
# `jobs` is the only one that natively supports job expansion
function __fish_expand_pid_args
    for arg in $argv
        if string match -qr '^%\d+$' -- $arg
            if not jobs -p $arg
                return 1
            end
        else
            printf "%s\n" $arg
        end
    end
end

for jobbltn in bg wait disown
    function $jobbltn -V jobbltn
        set -l args (__fish_expand_pid_args $argv)
        and builtin $jobbltn $args
    end
end
function fg
    set -l args (__fish_expand_pid_args $argv)
    and builtin fg $args[-1]
end

if command -q kill
    # Only define this if something to wrap exists
    # this allows a nice "commad not found" error to be triggered.
    function kill
        set -l args (__fish_expand_pid_args $argv)
        and command kill $args
    end
end

# As last part of initialization, source the conf directories.
# Implement precedence (User > Admin > Extra (e.g. vendors) > Fish) by basically doing "basename".
set -l sourcelist
for file in $__fish_config_dir/conf.d/*.fish $__fish_sysconf_dir/conf.d/*.fish $__fish_vendor_confdirs/*.fish
    set -l basename (string replace -r '^.*/' '' -- $file)
    contains -- $basename $sourcelist
    and continue
    set sourcelist $sourcelist $basename
    # Also skip non-files or unreadable files.
    # This allows one to use e.g. symlinks to /dev/null to "mask" something (like in systemd).
    test -f $file -a -r $file
    and source $file
end
