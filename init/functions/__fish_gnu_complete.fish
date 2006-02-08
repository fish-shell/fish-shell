function __fish_gnu_complete -d "Wrapper for the complete builtin. Skips the long completions on non-GNU systems"
	set is_gnu 0

	# Check if we are using a gnu system
	for i in (seq (count $argv))
		switch  $argv[$i]
			
			case -g --is-gnu
				set -e argv[$i]
				set is_gnu 1
				break
		end
	end

	# Remove long option if not on a gnu system
	if test $is_gnu = 0
		for i in (seq (count $argv))
			if test $argv[$i] = -l
				set -e argv[$i]
				set -e argv[$i]
				break
			end
		end
	end
	
	complete $argv

end

