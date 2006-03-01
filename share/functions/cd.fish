#
# The following functions add support for a directory history
#

function cd -d (N_ "Change directory")

	# Skip history in subshells
	if status --is-command-substitution
		builtin cd $argv
		return $status
	end

	# Avoid set completions
	set previous (command pwd)

	if test $argv[1] = - ^/dev/null
		if test $__fish_cd_direction = next ^/dev/null
			nextd
		else
			prevd
		end
		return $status
	end
				
	builtin cd $argv[1]

	if test $status = 0 -a (command pwd) != $previous
		set -g dirprev $dirprev $previous
		set -e dirnext
		set -g __fish_cd_direction prev
	end

	return $status
end

