#
# Main file for fish command completions. This file contains various
# common helper functions for the command completions. All actual
# completions are located in the completions subdirectory.
#

#
# Set default field separators
#

set -g IFS \n\ \t

#
# Set default search paths for completions and shellscript functions
# unless they already exist
#

set -l configdir ~/.config

if set -q XDG_CONFIG_HOME
	set configdir $XDG_CONFIG_HOME
end

# __fish_datadir, __fish_sysconfdir, __fish_help_dir, __fish_bin_dir
# are expected to have been set up by read_init from fish.cpp

# Set up function and completion paths. Make sure that the fish
# default functions/completions are included in the respective path.

if not set -q fish_function_path
	set fish_function_path $configdir/fish/functions    $__fish_sysconfdir/functions    $__fish_datadir/functions
end

if not contains $__fish_datadir/functions $fish_function_path
	set fish_function_path[-1] $__fish_datadir/functions
end

if not set -q fish_complete_path
	set fish_complete_path $configdir/fish/completions  $__fish_sysconfdir/completions  $__fish_datadir/completions $configdir/fish/generated_completions
end

if not contains $__fish_datadir/completions $fish_complete_path
	set fish_complete_path[-1] $__fish_datadir/completions
end

#
# This is a Solaris-specific test to modify the PATH so that
# Posix-conformant tools are used by default. It is separate from the
# other PATH code because this directory needs to be prepended, not
# appended, since it contains POSIX-compliant replacements for various
# system utilities.
#

if test -d /usr/xpg4/bin
	if not contains /usr/xpg4/bin $PATH
		set PATH /usr/xpg4/bin $PATH
	end
end

# Add a handler for when fish_user_path changes, so we can apply the same changes to PATH
# Invoke it immediately to apply the current value of fish_user_path
function __fish_reconstruct_path -d "Update PATH when fish_user_paths changes" --on-variable fish_user_paths
        set -l local_path $PATH
        set -l x
        for x in $__fish_added_user_paths
                if set -l idx (contains --index $x $local_path)
                        set -e local_path[$idx]
                end
	end

        set -e __fish_added_user_paths
        for x in $fish_user_paths[-1..1]
                if not contains $x $local_path
                        set local_path $x $local_path
                        set -g __fish_added_user_paths $__fish_added_user_paths $x
                end
        end
        set -xg PATH $local_path
end
__fish_reconstruct_path

# OS X-ism: Load the path files out of /etc/paths and /etc/paths.d/*
function __fish_load_path_helper_paths
    while read -l new_path_comp
        if not contains $new_path_comp $PATH
        	set PATH $PATH $new_path_comp
        end
    end
end
if test -r /etc/paths ; __fish_load_path_helper_paths < /etc/paths ; end
for pathfile in /etc/paths.d/* ; __fish_load_path_helper_paths < $pathfile ; end
functions -e __fish_load_path_helper_paths


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

# "." command for compatibility with old fish versions.
function . --description 'Evaluate contents of file (deprecated, see "source")' --no-scope-shadowing
	if begin
			test (count $argv) -eq 0
			# Uses tty directly, as isatty depends on "."
			and tty 0>&0 >/dev/null
		end
		echo "source: '.' command is deprecated, and doesn't work with STDIN anymore. Did you mean 'source' or './'?" >&2
		return 1
	else
		source $argv
	end
end
