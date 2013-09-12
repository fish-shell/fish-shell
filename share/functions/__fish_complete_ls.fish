#
# Completions for the ls command and its aliases
#

# Test if we are using GNU ls

function __fish_complete_ls -d "Completions for ls and its aliases"

	set -l is_gnu
	command ls --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

	set -l cmds -c $argv

	# Shared ls switches

	__fish_gnu_complete $cmds -s a -l all --description "Show hidden" $is_gnu
	__fish_gnu_complete $cmds -s A -l almost-all --description "Show hidden except . and .." $is_gnu
	__fish_gnu_complete $cmds -s F -l classify --description "Append filetype indicator" $is_gnu
	__fish_gnu_complete $cmds -s H -l dereference-command-line --description "Follow symlinks" $is_gnu
	__fish_gnu_complete $cmds -s L -l dereference --description "Follow symlinks" $is_gnu
	__fish_gnu_complete $cmds -s R -l recursive --description "List subdirectory recursively" $is_gnu
	__fish_gnu_complete $cmds -s b -l escape --description "Octal escapes for non graphic characters" $is_gnu
	__fish_gnu_complete $cmds -s d -l directory --description "List directories, not their content" $is_gnu
	__fish_gnu_complete $cmds -s h -l human-readable --description "Human readable sizes" $is_gnu
	__fish_gnu_complete $cmds -s i -l inode --description "Print inode number of files" $is_gnu
	__fish_gnu_complete $cmds -s n -l numeric-uid-gid --description "Long format, numeric IDs" $is_gnu
	__fish_gnu_complete $cmds -s p -l file-type --description "Append filetype indicator" $is_gnu
	__fish_gnu_complete $cmds -s q -l hide-control-chars --description "Replace non-graphic characters with '?'" $is_gnu
	__fish_gnu_complete $cmds -s r -l reverse --description "Reverse sort order" $is_gnu
	__fish_gnu_complete $cmds -s s -l size --description "Print size of files" $is_gnu

	complete $cmds -s C --description "List by columns"
	complete $cmds -s S --description "Sort by size"
	complete $cmds -s c --description "Show and sort by ctime"
	complete $cmds -s f --description "Don't sort"
	complete $cmds -s g --description "Long format without owner"
	complete $cmds -s k --description "Set blocksize to 1kB"
	complete $cmds -s l --description "Long format"
	complete $cmds -s m --description "Comma separated format"
	complete $cmds -s t --description "Sort by modification time"
	complete $cmds -s u --description "Show access time"
	complete $cmds -s x --description "List entries by lines"
	complete $cmds -s 1 --description "List one file per line"

	if test -n "$is_gnu"

	   	# GNU specific ls switches

		complete $cmds -l hide --description "Do not list implied entries matching specified shell pattern" -r
		complete $cmds -l lcontext --description "Display security context"
		complete $cmds -l context -s Z --description "Display  security  context  so  it fits on most displays"
		complete $cmds -l scontext --description "Display only security context and file name"

		complete $cmds -l author --description "Print author"
		complete $cmds -l blocksize -x --description "Set block size"
		complete $cmds -s B -l ignore-backups --description "Ignore files ending with ~"
		complete $cmds -l color -f -a "never always auto" --description "Use colors"
		complete $cmds -s D -l dired --description "Generate dired output"
		complete $cmds -l format -x -a "across commas horizontal long single-column verbose vertical" --description "List format"
		complete $cmds -l full-time --description "Long format, full-iso time"
		complete $cmds -s G -l no-group --description "Don't print group information"
		complete $cmds -l si --description "Human readable sizes, powers of 1000"
		complete $cmds -l dereference-command-line-symlink-to-dir #--description "Follow directory symlinks from command line"
		complete $cmds -l indicator-style -x -a "none classify file-type" --description "Append filetype indicator"
		complete $cmds -s I -l ignore -r --description "Skip entries matching pattern"
		complete $cmds -s N -l literal --description "Print raw entry names"
		complete $cmds -s o --description "Long format without groups"
		complete $cmds -l show-control-chars --description "Non graphic as-is"
		complete $cmds -s Q -l quote-name --description "Enclose entry in quotes"
		complete $cmds -l quoting-style -x -a "literal locale shell shell-always c escape" --description "Select quoting style"
		complete $cmds -l sort -x --description "Sort criteria" -a "
			extension\t'Sort by file extension'
			none\tDon\'t\ sort
			size\t'Sort by size'
			time\t'Sort by modification time'
			version\t'Sort by version'
			status\t'Sort by file status modification time'
			atime\t'Sort by access time'
			access\t'Sort by access time'
			use\t'Sort by access time'
		"
		complete $cmds -l time -x --description "Show time type" -a "
			time\t'Sort by modification time'
			access\t'Sort by access time'
			use\t'Sort by use time'
			ctime\t'Sort by file status modification time'
			status\t'Sort by status time'
		"
		complete $cmds -l time-style -x -a "full-iso long-iso iso locale" --description "Select time style"
		complete $cmds -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" --description "Assume tab stops at each COLS"
		complete $cmds -s U --description "Do not sort"
		complete $cmds -s v --description "Sort by version"
		complete $cmds -s w -l width -x --description "Assume screen width"
		complete $cmds -s X --description "Sort by extension"
		complete $cmds -l help --description "Display help and exit"
		complete $cmds -l version --description "Display version and exit"

	else

		# If not a GNU system, assume we have standard BSD ls features instead

		complete $cmds -s B --description "Octal escapes for non graphic characters"
		complete $cmds -s G --description "Use colors"
		complete $cmds -s I --description "Prevent -A from being automatically set for root"
		complete $cmds -s P --description "Don't follow symlinks"
		complete $cmds -s T --description "Show modification time"
		complete $cmds -s W --description "Show whiteouts when scanning directories"
		complete $cmds -s Z --description "Display each file's MAC label"
		complete $cmds -s o --description "Include the file flags in a long (-l) output"
		complete $cmds -s w --description "Print raw entry names"

	end

end
