
function pushd --description 'Push directory to stack'
	if count $argv >/dev/null
		# check for --help
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help pushd
				return 0
		end

		# emulate bash by checking if argument of form +n or -n
		set rot_l (echo $argv[1] | sed -n '/^+\([0-9]\)$/  s//\1/g p')
		set rot_r (echo $argv[1] | sed -n '/^-\([0-9]\)$/  s//\1/g p')
	end

	# emulate bash: an empty pushd should switch the top of dirs
	if test (count $argv) -eq 0
		# check that the stack isn't empty
		if test (count $dirstack) -eq 0
			echo "pushd: no other directory"
			return 1
		end

		# get the top two values of the dirs stack ... the first is pwd
		set -l top_dir (command pwd)
		set -l next_dir $dirstack[1]

		# alter the top of dirstack and move to directory
		set -g dirstack[1] $top_dir
		cd $next_dir
		return
	end

	# emulate bash: check for rotations
	if test -n "$rot_l" -o -n "$rot_r"
		# grab the current stack
		set -l stack (command pwd) $dirstack

		# translate a right rotation to a left rotation
		if test -n "$rot_r"
			# check the rotation in range
			if test $rot_r -gt (math (count $stack) - 1)
				echo "pushd: -$rot_r: directory stack index out of range"
				return 1
			end

			set rot_l (math (count $stack) - 1 - $rot_r)
		end

		# check the rotation in range
		if test $rot_l -gt (math (count $stack) - 1)
			echo "pushd: +$rot_l: directory stack index out of range"
			return 1
		else
			# rotate stack unless rot_l is 0
			if test $rot_l -gt 0
				set stack $stack[(math $rot_l + 1)..(count $stack)] $stack[1..$rot_l]
			end

			# now reconstruct dirstack and change directory
			set -g dirstack $stack[2..(count $stack)]
			cd $stack[1]
		end

		# print the new stack
		dirs
		return
	end

	# Comment to avoid set completions
	set -g dirstack (command pwd) $dirstack
	cd $argv[1]

end
