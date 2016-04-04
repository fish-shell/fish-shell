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
# Create the default command_not_found handler
#
function __fish_default_command_not_found_handler
	echo "fish: Unknown command '$argv'" >&2
end

#
# Hook up the default as the principal command_not_found handler
# in case we are not interactive
#
status -i; or function __fish_command_not_found_handler --on-event fish_command_not_found
	__fish_default_command_not_found_handler $argv
end

#
# Set default search paths for completions and shellscript functions
# unless they already exist
#

set -l configdir ~/.config

if set -q XDG_CONFIG_HOME
	set configdir $XDG_CONFIG_HOME
end

set -l userdatadir ~/.local/share

if set -q XDG_DATA_HOME
	set userdatadir $XDG_DATA_HOME
end

# __fish_datadir, __fish_sysconfdir, __fish_help_dir, __fish_bin_dir
# are expected to have been set up by read_init from fish.cpp

# Grab extra directories (as specified by the build process, usually for
# third-party packages to ship completions &c.
set -l __extra_completionsdir
set -l __extra_functionsdir
set -l __extra_snippetsdir
if test -f $__fish_datadir/__fish_build_paths.fish
    source $__fish_datadir/__fish_build_paths.fish
end

# Set up function and completion paths. Make sure that the fish
# default functions/completions are included in the respective path.

if not set -q fish_function_path
	set fish_function_path $configdir/fish/functions $__fish_sysconfdir/functions $__extra_functionsdir $__fish_datadir/functions
end

if not contains $__fish_datadir/functions $fish_function_path
	set fish_function_path $fish_function_path $__fish_datadir/functions
end

if not set -q fish_complete_path
	set fish_complete_path $configdir/fish/completions $__fish_sysconfdir/completions $__extra_completionsdir $__fish_datadir/completions $userdatadir/fish/generated_completions
end

if not contains $__fish_datadir/completions $fish_complete_path
	set fish_complete_path $fish_complete_path $__fish_datadir/completions
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

# OS X-ism: Load the path files out of /etc/paths and /etc/paths.d/*
set -g __fish_tmp_path $PATH
function __fish_load_path_helper_paths
	# We want to rearrange the path to reflect this order. Delete that path component if it exists and then prepend it.
	# Since we are prepending but want to preserve the order of the input file, we reverse the array, append, and then reverse it again
	set __fish_tmp_path $__fish_tmp_path[-1..1] 
	while read -l new_path_comp
		if test -d $new_path_comp
			set -l where (contains -i $new_path_comp $__fish_tmp_path)
			and set -e __fish_tmp_path[$where]
			set __fish_tmp_path $new_path_comp $__fish_tmp_path
		end
	end
	set __fish_tmp_path $__fish_tmp_path[-1..1]
end
test -r /etc/paths ; and __fish_load_path_helper_paths < /etc/paths 
for pathfile in /etc/paths.d/* ; __fish_load_path_helper_paths < $pathfile ; end
set -xg PATH $__fish_tmp_path
set -e __fish_tmp_path
functions -e __fish_load_path_helper_paths


# Add a handler for when fish_user_path changes, so we can apply the same changes to PATH
# Invoke it immediately to apply the current value of fish_user_path
function __fish_reconstruct_path -d "Update PATH when fish_user_paths changes" --on-variable fish_user_paths
	set -l local_path $PATH
	set -l x
	for x in $__fish_added_user_paths
		set -l idx (contains --index $x $local_path)
		and set -e local_path[$idx]
	end

	set -e __fish_added_user_paths
	for x in $fish_user_paths[-1..1]
		if set -l idx (contains --index $x $local_path)
			set -e local_path[$idx]
		else
			set -g __fish_added_user_paths $__fish_added_user_paths $x
		end
		set local_path $x $local_path
	end
	set -xg PATH $local_path
end
__fish_reconstruct_path

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

# As last part of initialization, source the conf directories
# Implement precedence (User > Admin > Extra (e.g. vendors) > Fish) by basically doing "basename"
set -l sourcelist
for file in $configdir/fish/conf.d/*.fish $__fish_sysconfdir/conf.d/*.fish $__extra_snippetsdir/*.fish
	set -l basename (string replace -r '^.*/' '' -- $file)
	contains -- $basename $sourcelist; and continue
	set sourcelist $sourcelist $basename
	# Also skip non-files or unreadable files
	# This allows one to use e.g. symlinks to /dev/null to "mask" something (like in systemd)
	[ -f $file -a -r $file ]; and source $file
end

# Upgrade pre-existing abbreviations from the old "key=value" to the new "key value" syntax
# This needs to be in share/config.fish because __fish_config_interactive is called after sourcing config.fish, which might contain abbr calls
if not set -q __fish_init_2_3_0
	set -l fab
	for abb in $fish_user_abbreviations
		set fab $fab (string replace -r '^([^ =]+)=(.*)$' '$1 $2' -- $abb)
	end
	set fish_user_abbreviations $fab
	set -U __fish_init_2_3_0
end

#
# Some things should only be done for login terminals
# This used to be in etc/config.fish - keep it here to keep the semantics
#

if status --is-login

	# Check for i18n information in
	# /etc/sysconfig/i18n

	if test -f /etc/sysconfig/i18n
		string match -r '^[a-zA-Z]*=.*' < /etc/sysconfig/i18n | while read -l line
			set -gx (string split '=' -m 1 -- $line | string replace -ra '"([^"]+)"' '$1' | string replace -ra "'([^']+)'" '$1')
		end
	end

	#
	# Put linux consoles in unicode mode.
	#

	if test "$TERM" = linux
		if string match -qir '\.UTF' -- $LANG
			if command -s unicode_start >/dev/null
				unicode_start
			end
		end
	end
end
