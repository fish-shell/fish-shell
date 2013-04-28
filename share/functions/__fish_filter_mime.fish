#
# $argv[1] is a mimetype. The mimetype may contain wildcards. All
# following arguments are filenames. Filenames matching the mimetype
# are returned.
#

function __fish_filter_mime -d "Select files with a specific mimetype"

	set -l mime_search $argv[1]
	set -e argv[1]

	if not set -l mime (mimedb -f $argv)
		return 1
	end
	
	if count $mime > /dev/null
		set -l res
		for i in (seq (count $mime))
			switch $mime[$i]
				case $mime_search
					set res $res $argv[$i]
			end
		end
		printf "%s\n" $res
	end
end
