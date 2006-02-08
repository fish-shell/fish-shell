#
# Find files that complete $argv[1], has the suffix $argv[2], and
# output them as completions with description $argv[3]
#

function __fish_complete_suffix -d "Complete using files"

	set -- comp $argv[1]
	set -- suff $argv[2]
	set -- desc $argv[3]

	set -- base (echo $comp |sed -e 's/\.[a-zA-Z0-9]*$//')
	eval "set -- files $base*$suff"

	if test $files[1]
		printf "%s\t$desc\n" $files
	end

	#
	# Also do directory completion, since there might be files
	# with the correct suffix in a subdirectory
	#

	__fish_complete_directory $comp

end
