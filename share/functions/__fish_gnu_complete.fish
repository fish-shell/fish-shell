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
	set -l skip_next 0

	# Remove long option if not on a gnu system
	if test $is_gnu = 0
		for i in $argv

			if test $skip_next = 1
				continue
			end
	
			if test $i = -l
				set skip_next 1
				continue
			end

			set argv_out $argv_out $i
		end
		set argv $argv_out
	end
	
	complete $argv

end

