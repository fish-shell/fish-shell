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
	set fish_complete_path $configdir/fish/completions  $__fish_sysconfdir/completions  $__fish_datadir/completions
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

#
# Add a few common directories to path, if they exists. Note that pure
# console programs like makedep sometimes live in /usr/X11R6/bin, so we
# want this even for text-only terminals.
#

set -l path_list /bin /usr/bin /usr/X11R6/bin /usr/local/bin $__fish_bin_dir

# Root should also have the sbin directories in the path
switch $USER
	case root
	set path_list $path_list /sbin /usr/sbin /usr/local/sbin
end

for i in $path_list
	if not contains $i $PATH
		if test -d $i
			set PATH $PATH $i
		end
	end
end

#
# Launch debugger on SIGTRAP
#
function fish_sigtrap_handler --on-signal TRAP --no-scope-shadowing --description "Signal handler for the TRAP signal. Lanches a debug prompt."
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

