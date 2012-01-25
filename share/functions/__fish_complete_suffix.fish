#
# Find files that complete $argv[1], has the suffix $argv[2], and
# output them as completions with the description $argv[3] Both
# $argv[1] and $argv[3] are optional, if only one is specified, it is
# assumed to be the argument to complete.
#

function __fish_complete_suffix -d "Complete using files"

	# Variable declarations

	set -l comp
	set -l suff
	set -l desc
	set -l files

	switch (count $argv)

		case 1
		set comp (commandline -ct)
		set suff $argv
		set desc (mimedb -d $suff)

		case 2
		set comp $argv[1]
		set suff $argv[2]
		set desc (mimedb -d $suff)

		case 3
		set comp $argv[1]
		set suff $argv[2]
		set desc $argv[3]

	end

	# Perform the completion

	set base (echo $comp |sed -e 's/\.[a-zA-Z0-9]*$//')
	eval "set files $base*$suff"

	if test $files[1]
		printf "%s\t$desc\n" $files
	end

	#
	# Also do directory completion, since there might be files
	# with the correct suffix in a subdirectory
	#

	__fish_complete_directories $comp

end
