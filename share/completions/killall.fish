# For Solaris OS, we don't need to generate completions. Since it can hang the PC.
if test (uname) != 'SunOS'
	__fish_make_completion_signals

	for i in $__kill_signals
		set number (echo $i | cut -d " " -f 1)
		set name (echo $i | cut -d " " -f 2)
		complete -c killall -o $number -d $name
		complete -c killall -o $name
		# Doesn't work in OS X; -s is simulate
		test (uname) != 'Darwin'; and complete -c killall -s s -x -a "$number $name"
	end

	# Finds and completes all users, and their respective UID.
	function __make_users_completions
		set -l users (dscl . list /Users | grep -v '^_.*')
		for user in $users
			set -l uid (id -u $user)
			# GNU doesn't support UID
			killall --version > /dev/null ^ /dev/null; and set -el uid
			complete -c killall -s u -x -a "$user $uid" -d "Match only processes belonging to $user"
		end
	end

	complete -c killall -xa '(__fish_complete_proc)'

	if killall --version > /dev/null ^ /dev/null
		complete -c killall -s e -l exact -d 'Require an exact match for very long names'
		complete -c killall -s I -l ignore-case -d 'Do case insensitive process name match'
		complete -c killall -s g -l process-group -d 'Kill the process group to which the process belongs. The kill signal is only sent once per group, even if multiple processes belonging to the same process group were found'
		complete -c killall -s i -l interactive -d 'Interactively ask for confirmation before killing'
		complete -c killall -s u -l user -d 'Kill only processes the specified user owns. Command names are optional' -x
		__make_users_completions
		complete -c killall -s w -l wait -d 'Wait  for  all  killed processes to die. killall checks once per second if any of the killed processes still exist and only returns if none are left.  Note that killall may wait forever if the signal was ignored, had no effect, or if the process stays in zombie state'
		complete -c killall -s v -l version -d 'Print version'
	else
		complete -c killall -s v -d 'Be more verbose about what will be done'
		complete -c killall -s e -d 'Use effective user ID instead of the real user ID for matching processes specified with the -u option'
		complete -c killall -s help -d 'Print help and exit'
		complete -c killall -s l -d 'List names of available signals and exit'
		complete -c killall -s m -d 'Case sensitive argument match for processed'
		complete -c killall -s s -d 'Simulate, but do not send any signals'
		complete -c killall -s d -d 'Print detailed info. Doesn\'t send signals'
		complete -c killall -s u -x -d 'Only processes for USER'
		# Completions for users 
		__make_users_completions
		complete -c killall -s t -d 'Limit to processes running on specified TTY' 
		complete -c killall -s t -xa "(ps a -o tty | sed 1d | uniq)"
		complete -c killall -s c -x -d 'Limit to processes matching specified PROCNAME'
		complete -c killall -s z -d 'Do not skip zombies'
	end
end
