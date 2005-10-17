# Main file for fish command completions. This file contains various
# common helper functions for the command completions. All actual
# completions are located in the completions subdirectory.

#
# Don't need completions in non-interactive mode
#

if not status --is-interactive
	exit
end

set -g fish_complete_path /etc/fish.d/completions ~/.fish.d/completions

# Knowing the location of the whatis database speeds up command
# description lookup.

for i in /var/cache/man/{whatis,windex} /usr{,/local}{,/share}/man/{whatis,windex}
	if test -f $i
		set -g __fish_whatis_path $i
		break
	end
end

#
# Convenience functions
#
# The naming heuristic is that __fish_complete_* prints completions
# and descriptions, while __fish_print_* only prints the completion,
# without the description
#

#
# Find files that complete $argv[1], has the suffix $argv[2], and
# output them as completions with description $argv[3]
#

function __fish_complete_suffix -d "Complete using files"

	set -- comp $argv[1]
	set -- suff $argv[2]
	set -- desc $argv[3]

	set -- base (echo $comp |sed -e 's/\.[a-zA-Z0-9]*$//')
	eval "set -- files "$base"*"$suff

	if test $files[1]
		printf "%s\t"$desc"\n" $files
	end

	#
	# Also do directory completion, since there might be files
	# with the correct suffix in a subdirectory
	#

	__fish_complete_directory $comp

end

#
# Find directories that complete $argv[1], output them as completions
# with description $argv[2] if defined, otherwise use 'Directory'
#

function __fish_complete_directory -d "Complete using directories"

	set -- comp $argv[1]
	set -- desc Directory

	if test (count $argv) -gt 1
		set desc $argv[2]
	end

	eval "set -- dirs "$comp"*/"

	if test $dirs[1]
		printf "%s\t"$desc"\n" $dirs
	end

end

function __fish_complete_users -d "Print a list of local users, with the real user name as a description"
	cat /etc/passwd | sed -e "s/^\([^:]*\):[^:]*:[^:]*:[^:]*:\([^:]*\):.*/\1\t\2/"
end

function __fish_complete_groups -d "Print a list of local groups, with group members as the description"
	cat /etc/group | sed -e "s/^\([^:]*\):[^:]*:[^:]*:\(.*\)/\1\tMembers: \2/"
end

function __fish_complete_pids -d "Print a list of process identifiers along with brief descriptions"
	# This may be a bit slower, but it's nice - having the tty displayed is really handy
	ps --no-heading -o pid,comm,tty --ppid %self -N | sed -r 's/ *([0-9]+) +([^ ].*[^ ]|[^ ]) +([^ ]+)$/\1\t\2 [\3]/' ^/dev/null

	# If the above is too slow, this is faster but a little less useful
	#	pgrep -l -v -P %self | sed 's/ /\t/'
end

function __fish_print_hostnames -d "Print a list of known hostnames"

	# Print all hosts from /etc/hosts
	cat /etc/hosts|sed -e 's/[0-9.]*\( \|\t\)*\(.*\)/\2/'|sed -e 's/\#.*//'|sed -e 'y/\t/\n/'|grep -v '^$'

	# Print nfs servers from /etc/fstab
	cat /etc/fstab| grep "^\([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*\|[a-zA-Z.]*\):"|cut -d : -f 1

	# Print hosts with known ssh keys
	cat ~/.ssh/known_hosts{,2} ^/dev/null|cut -d ' ' -f 1| cut -d , -f 1
end

function __fish_print_interfaces -d "Print a list of known network interfaces"
	netstat -i -n -a | awk 'NR>2'|awk '{print $1}'
end

function __fish_print_addresses -d "Print a list of known network addresses"
	/sbin/ifconfig |grep 'inet addr'|cut -d : -f 2|cut -d ' ' -f 1
end

function __fish_print_users -d "Print a list of local users"
	cat /etc/passwd | cut -d : -f 1
end


function __fish_contains_opt -d "Checks if a specific option has been given in the current commandline"
	set -l next_short 

	set -l short_opt
	set -l long_opt 

	for i in $argv
		if test $next_short 
			set next_short 
			set -- short_opt $short_opt $i
		else
			switch $i
				case -s
					set next_short 1
				case '-*'
					echo __fish_contains_opt: Unknown option $i
					return 1

				case '**'
					set -- long_opt $long_opt $i
			end
		end
	end

	for i in $short_opt

		if test -z $i
			continue
		end

		if commandline -cpo | grep -- "^-"$i"\|^-[^-]*"$i >/dev/null
			return 0
		end
		
		if commandline -ct | grep -- "^-"$i"\|^-[^-]*"$i >/dev/null
			return 0
		end
	end

	for i in $long_opt
		if test -z $i
			continue
		end

		if contains -- --$i (commandline -cpo)
			return 0
		end
	end

	return 1
end

#
# Completions for the shell and it's builtin commands and functions
#

for i in (builtin -n|grep -vE '(while|for|if|function|switch)' )
	complete -c $i -s h -l help -d "Display help and exit"
end


function __fish_print_packages

	# apt-cache is much, much faster than rpm, and can do this in real
    # time. We use it if available.

	switch (commandline -tc)
		case '-**'
			return
	end

	if which apt-cache >/dev/null ^/dev/null
		# Apply the following filters to output of apt-cache:
		# 1) Remove package names with parentesis in them, since these seem to not correspond to actual packages as reported by rpm
		# 2) Remove package names that are .so files, since these seem to not correspond to actual packages as reported by rpm
		# 3) Remove path information such as /usr/bin/, as rpm packages do not have paths

		apt-cache --no-generate pkgnames (commandline -tc)|grep -v \( |grep -v '\.so\(\.[0-9]\)*$'|sed -e 's/\/.*\///'|sed -e 's/$/\tPackage/'
		return
	end

	# Rpm is too slow for this job, so we set it up to do completions
    # as a background job and cache the results.

	if which rpm >/dev/null ^/dev/null

		# If the cache is less than five minutes old, we do not recalculate it

		set cache_file /tmp/.rpm-cache.$USER
			if test -f $cache_file 
			cat $cache_file
			set age (echo (date +%s) - (stat -c '%Y' $cache_file) | bc)
			set max_age 250
			if test $age -lt $max_age
				return
			end
		end

		# Remove package version information from output and pipe into cache file
		rpm -qa >$cache_file |sed -e 's/-[^-]*-[^-]*$//' | sed -e 's/$/\tPackage/' &
	end
end


function __fish_append -d "Internal completion function for appending string to the commandline"
	set separator $argv[1]
	set -e argv[1]
	set str (commandline -tc| sed -re 's/(.*'$separator')[^'$separator']*/\1/')
	printf "%s\n" $str$argv $str$argv,
end

#
# Completions for SysV startup scripts
#

set -g __fish_service_commands '
	start\t"Start service"
	stop\t"Stop service"
	status\t"Print service status"
	restart\t"Stop and then start service"
	reload\t"Reload service configuration"
'

complete -x -p "/etc/init.d/*" -a '$__fish_service_commands'
