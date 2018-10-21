# Main file for fish command completions. This file contains various
# common helper functions for the command completions. All actual
# completions are located in the completions subdirectory.
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
    printf "fish: Unknown command '%s'\n" (string escape -- $argv) >&2
end

if status --is-interactive
    # The user has seemingly explicitly launched an old fish with too-new scripts installed.
    if not contains -- "string" (builtin -n)
        set -g __is_launched_without_string 1
        # XXX nostring - fix old fish binaries with no `string' builtin.
        # When executed on fish 2.2.0, the `else' block after this would
        # force on 24-bit mode due to changes to in test behavior.
        # These "XXX nostring" hacks were added for 2.3.1
        set_color --bold
        echo "You appear to be trying to launch an old fish binary with newer scripts "
        echo "installed into" (set_color --underline)"$__fish_data_dir"
        set_color normal
        echo -e "\nThis is an unsupported configuration.\n"
        set_color yellow
        echo "You may need to uninstall and reinstall fish!"
        set_color normal
        # Remove this code when we've made it safer to upgrade fish.
    else
        # Enable truecolor/24-bit support for select terminals
        # Ignore Screen and emacs' ansi-term as they swallow the sequences, rendering the text white.
        if not set -q STY
            and not string match -q -- 'eterm*' $TERM
            and begin
                set -q KONSOLE_PROFILE_NAME # KDE's konsole
                or string match -q -- "*:*" $ITERM_SESSION_ID # Supporting versions of iTerm2 will include a colon here
                or string match -q -- "st-*" $TERM # suckless' st
                or test -n "$VTE_VERSION" -a "$VTE_VERSION" -ge 3600 # Should be all gtk3-vte-based terms after version 3.6.0.0
                or test "$COLORTERM" = truecolor -o "$COLORTERM" = 24bit # slang expects this
            end
            # Only set it if it isn't to allow override by setting to 0
            set -q fish_term24bit
            or set -g fish_term24bit 1
        end
    end
else
    # Hook up the default as the principal command_not_found handler
    # in case we are not interactive
    function __fish_command_not_found_handler --on-event fish_command_not_found
        __fish_default_command_not_found_handler $argv
    end
end

#
# Set default search paths for completions and shellscript functions
# unless they already exist
#

set -g __fish_config_dir ~/.config/fish

if set -q XDG_CONFIG_HOME
    set __fish_config_dir $XDG_CONFIG_HOME/fish
end

set -l userdatadir ~/.local/share

if set -q XDG_DATA_HOME
    set userdatadir $XDG_DATA_HOME
end

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

# Set up function and completion paths. Make sure that the fish
# default functions/completions are included in the respective path.

if not set -q fish_function_path
    set fish_function_path $__fish_config_dir/functions $__fish_sysconf_dir/functions $__extra_functionsdir $__fish_data_dir/functions
end

if not contains -- $__fish_data_dir/functions $fish_function_path
    set fish_function_path $fish_function_path $__fish_data_dir/functions
end

if not set -q fish_complete_path
    set fish_complete_path $__fish_config_dir/completions $__fish_sysconf_dir/completions $__extra_completionsdir $__fish_data_dir/completions $userdatadir/fish/generated_completions
end

if not contains -- $__fish_data_dir/completions $fish_complete_path
    set fish_complete_path $fish_complete_path $__fish_data_dir/completions
end

# This cannot be in an autoload-file because `:.fish` is an invalid filename on windows.
function :
    # no-op function for compatibility with sh, bash, and others.
    # Often used to insert a comment into a chain of commands without having
	# it eat up the remainder of the line, handy in Makefiles.
end

#
# This is a Solaris-specific test to modify the PATH so that
# Posix-conformant tools are used by default. It is separate from the
# other PATH code because this directory needs to be prepended, not
# appended, since it contains POSIX-compliant replacements for various
# system utilities.
#

if test -d /usr/xpg4/bin
    if not contains -- /usr/xpg4/bin $PATH
        set PATH /usr/xpg4/bin $PATH
    end
end

# Add a handler for when fish_user_path changes, so we can apply the same changes to PATH
function __fish_reconstruct_path -d "Update PATH when fish_user_paths changes" --on-variable fish_user_paths
    set -l local_path $PATH

    for x in $__fish_added_user_paths
        set -l idx (contains --index -- $x $local_path)
        and set -e local_path[$idx]
    end

    set -g __fish_added_user_paths
    if set -q fish_user_paths
        for x in $fish_user_paths[-1..1]
            if set -l idx (contains --index -- $x $local_path)
                set -e local_path[$idx]
            else
                set -g __fish_added_user_paths $__fish_added_user_paths $x
            end
            set local_path $x $local_path
        end
    end

    set -xg PATH $local_path
end

#
# Launch debugger on SIGTRAP
#
function fish_sigtrap_handler --on-signal TRAP --no-scope-shadowing --description "Signal handler for the TRAP signal. Launches a debug prompt."
    breakpoint
end

#
# Whenever a prompt is displayed, make sure that interactive
# mode-specific initializations have been performed.
# This handler removes itself after it is first called.
#
function __fish_on_interactive --on-event fish_prompt
    __fish_config_interactive
    functions -e __fish_on_interactive
end

# Set the locale if it isn't explicitly set. Allowing the lack of locale env vars to imply the
# C/POSIX locale causes too many problems. Do this before reading the snippets because they might be
# in UTF-8 (with non-ASCII characters).
__fish_set_locale

# "." command for compatibility with old fish versions.
function . --description 'Evaluate contents of file (deprecated, see "source")' --no-scope-shadowing
    if test (count $argv) -eq 0
        # Uses tty directly, as isatty depends on "."
        and tty 0>&0 >/dev/null
        echo "source: '.' command is deprecated, and doesn't work with STDIN anymore. Did you mean 'source' or './'?" >&2
        return 1
    else
        source $argv
    end
end

# Upgrade pre-existing abbreviations from the old "key=value" to the new "key value" syntax.
# This needs to be in share/config.fish because __fish_config_interactive is called after sourcing
# config.fish, which might contain abbr calls.
if not set -q __fish_init_2_3_0
    if set -q fish_user_abbreviations
        set -l fab
        for abbr in $fish_user_abbreviations
            set fab $fab (string replace -r '^([^ =]+)=(.*)$' '$1 $2' -- $abbr)
        end
        set fish_user_abbreviations $fab
    end
    set -U __fish_init_2_3_0
end

# macOS-ism: Emulate calling path_helper.
if command -sq /usr/libexec/path_helper
    # Adapt construct_path from the macOS /usr/libexec/path_helper
    # executable for fish; see
    # https://opensource.apple.com/source/shell_cmds/shell_cmds-203/path_helper/path_helper.c.auto.html .
    function __fish_macos_set_env -d "set an environment variable like path_helper does (macOS only)"
        set -l result

        for path_file in $argv[2] $argv[3]/*
            if test -f $path_file
                while read -la entry
                    if not contains $entry $result
                        set result $result $entry
                    end
                end <$path_file
            end
        end

        for entry in $$argv[1]
            if not contains $entry $result
                set result $result $entry
            end
        end

        set -xg $argv[1] $result
    end

    __fish_macos_set_env 'PATH' '/etc/paths' '/etc/paths.d'
    if [ -n "$MANPATH" ]
        __fish_macos_set_env 'MANPATH' '/etc/manpaths' '/etc/manpaths.d'
    end
    functions -e __fish_macos_set_env
end


#
# Some things should only be done for login terminals
# This used to be in etc/config.fish - keep it here to keep the semantics
#

if status --is-login
    #
    # Put linux consoles in unicode mode.
    #
    if test "$TERM" = linux
        if string match -qir '\.UTF' -- $LANG
            if command -sq unicode_start
                unicode_start
            end
        end
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
            # set newargv $newargv (jobs -p $arg)
            jobs -p $arg
            if not test $status -eq 0
                return 1
            end
        else
            printf "%s\n" $arg
        end
    end
end

function bg --wraps bg
    builtin bg (__fish_expand_pid_args $argv)
end

function fg --wraps fg
    builtin fg (__fish_expand_pid_args $argv)
end

function kill --wraps kill
    command kill (__fish_expand_pid_args $argv)
end

function wait --wraps wait
    builtin wait (__fish_expand_pid_args $argv)
end

function disown --wraps disown
    builtin disown (__fish_expand_pid_args $argv)
end

# As last part of initialization, source the conf directories.
# Implement precedence (User > Admin > Extra (e.g. vendors) > Fish) by basically doing "basename".
set -l sourcelist
for file in $__fish_config_dir/conf.d/*.fish $__fish_sysconf_dir/conf.d/*.fish $__extra_confdir/*.fish
    set -l basename (string replace -r '^.*/' '' -- $file)
    contains -- $basename $sourcelist
    and continue
    set sourcelist $sourcelist $basename
    # Also skip non-files or unreadable files.
    # This allows one to use e.g. symlinks to /dev/null to "mask" something (like in systemd).
    [ -f $file -a -r $file ]
    and source $file
end
