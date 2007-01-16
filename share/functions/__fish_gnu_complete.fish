function __fish_gnu_complete -d "Wrapper for the complete builtin. Skips the long completions on non-GNU systems"
	set is_gnu 0

	set -l argv_out

	# Check if we are using a gnu system
	for i in $argv
		switch  $i
			
			case -g --is-gnu
				set is_gnu 1

			case '*'
				 set argv_out $argv_out $i
		end
	end

	set argv $argv_out
	set argv_out

	# Remove long option if not on a gnu system
	if test $is_gnu = 0
		for i in $argv

			if set -q __fish_gnu_complete_skip_next
				set -e __fish_gnu_complete_skip_next
				continue
			end
	
			switch $i
				case -l --long
					set __fish_gnu_complete_skip_next 1
					continue
			end

			set argv_out $argv_out $i
		end
		set argv $argv_out
	end
	set -e __fish_gnu_complete_skip_next
	
	complete $argv

end

