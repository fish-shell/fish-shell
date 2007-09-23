#
# Find directories that complete $argv[1], output them as completions
# with description $argv[2] if defined, otherwise use 'Directory'
#

function __fish_complete_directories -d "Complete using directories" --argument comp

	set desc (_ Directory)

	if test (count $argv) -gt 1
		set desc $argv[2]
	end

	eval "set dirs "$comp"*/"

	if test $dirs[1]
		printf "%s\t$desc\n" $dirs
	end

end

