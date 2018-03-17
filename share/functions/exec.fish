# A wrapper for the `exec` builtin to mimic the warn-before-terminate
# behavior of the `exit` builtin, but written entirely in fish.

if not set -q __fish_exec_path
	set -g __fish_exec_path (status -f)
end

if not set -gq __fish_exec_count
	set -g __fish_exec_count 0
end

function __fish_really_exec
	# echo "actually `exec`ing"
	# Actually exec, or at least, try to
	functions --erase exec
	exec /proc/self/exe -c $argv

	# We have to reset it because we might be `exec`ing a function, in which case
	# it's equivalent to not having called `exec` at all. Also, the exec might fail
	# (e.g. if the command isn't found)
	set -g $__fish_exec_count 0
	source $__fish_exec_path
end

function exec --wraps exec
	# We need to stop the fish_postexec handler from counting our
	set -g __fish_exec_ignore 1
	# echo "exec wrapper called"

	if jobs -q
		# echo was: $__fish_exec_count
		set __fish_exec_count (math $__fish_exec_count + 1)
		# echo now: $__fish_exec_count

		if test $__fish_exec_count -eq 1
			echo -en "Warning: the following jobs are running in the background"
			echo -en "and will be terminated if `exec` is used:\n"

			jobs

			echo -e "\nUse 'disown PID' to remove jobs from the list without terminating them."
			echo -e "A second call to `exec` will abandon these jobs and continue."

			return 1
		end 1>&2
	end

	__fish_really_exec $argv
end
