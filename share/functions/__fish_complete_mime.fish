
function __fish_complete_mime -d "Complete using text files"
	# Find all possible file completions
	set -l all
	set -l comp (commandline -ct)
	set -l base (echo $comp | sed -e 's/\.[a-zA-Z0-9]*$//')
	set -l mimetype $argv[1]
	eval "set all $base*"

	# Select text files only
	set -l files (__fish_filter_mime $mimetype $all)

	# Get descriptions for files
	set desc (mimedb -d $files)

	# Format completions and descriptions
	if count $files > /dev/null
		set -l res
		for i in (seq (count $files))
			set res $res $files[$i]\t$desc[$i]
		end

		if test $res[1]
			printf "%s\n" $res
		end
	end
end
