#
# Completions for the ls command and its aliases
#

# Test if we are using GNU ls

set -l is_gnu 
ls --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

for i in ls ll la

	# Shared ls switches

	__fish_gnu_complete -c $i -s a -l all -d (_ "Show hidden") $is_gnu
	__fish_gnu_complete -c $i -s A -l almost-all -d (_ "Show hidden except . and ..") $is_gnu
	__fish_gnu_complete -c $i -s F -l classify -d (_ "Append filetype indicator") $is_gnu
	__fish_gnu_complete -c $i -s H -l dereference-command-line -d (_ "Follow symlinks") $is_gnu
	__fish_gnu_complete -c $i -s L -l dereference -d (_ "Follow symlinks") $is_gnu
	__fish_gnu_complete -c $i -s R -l recursive -d (_ "List subdirectory recursively") $is_gnu
	__fish_gnu_complete -c $i -s b -l escape -d (_ "Octal escapes for non graphic characters") $is_gnu
	__fish_gnu_complete -c $i -s d -l directory -d (_ "List directories, not their content") $is_gnu
	__fish_gnu_complete -c $i -s h -l human-readable -d (_ "Human readable sizes") $is_gnu
	__fish_gnu_complete -c $i -s i -l inode -d (_ "Print inode number of files") $is_gnu
	__fish_gnu_complete -c $i -s n -l numeric-uid-gid -d (_ "Long format, numeric IDs") $is_gnu
	__fish_gnu_complete -c $i -s p -l file-type -d (_ "Append filetype indicator") $is_gnu
	__fish_gnu_complete -c $i -s q -l hide-control-chars -d (_ "Replace non-graphic characters with '?'") $is_gnu
	__fish_gnu_complete -c $i -s r -l reverse -d (_ "Reverse sort order") $is_gnu
	__fish_gnu_complete -c $i -s s -l size -d (_ "Print size of files") $is_gnu

	complete -c $i -s C -d (_ "List by columns")
	complete -c $i -s S -d (_ "Sort by size")
	complete -c $i -s c -d (_ "Show and sort by ctime")
	complete -c $i -s f -d (_ "Don't sort")
	complete -c $i -s g -d (_ "Long format without owner")
	complete -c $i -s k -d (_ "Set blocksize to 1kB")
	complete -c $i -s l -d (_ "Long format")
	complete -c $i -s m -d (_ "Comma separated format")
	complete -c $i -s t -d (_ "Sort by modification time")
	complete -c $i -s u -d (_ "Show access time")
	complete -c $i -s x -d (_ "List entries by lines")
	complete -c $i -s 1 -d (_ "List one file per line")

	if test -n "$is_gnu"

		# GNU specific features

		complete -c $i -l author -d (_ "Print author")
		complete -c $i -l blocksize -x -d (_ "Set block size")
		complete -c $i -s B -l ignore-backups -d (_ "Ignore files ending with ~")
		complete -c $i -l color -f -a "never always auto" -d (_ "Use colors")
		complete -c $i -s D -l dired -d (_ "Generate dired output")
		complete -c $i -l format -x -a "across commas horizontal long single-column verbose vertical" -d (_ "List format")
		complete -c $i -l full-time -d (_ "Long format, full-iso time")
		complete -c $i -s G -l no-group -d (_ "Don't print group information")
		complete -c $i -l si -d (_ "Human readable sizes, powers of 1000")
		complete -c $i -l dereference-command-line-symlink-to-dir #-d (_ "Follow directory symlinks from command line")
		complete -c $i -l indicator-style -x -a "none classify file-type" -d (_ "Append filetype indicator")
		complete -c $i -s I -l ignore -r -d (_ "Skip entries matching pattern")
		complete -c $i -s N -l literal -d (_ "Print raw entry names")
		complete -c $i -s o -d (_ "Long format without groups")
		complete -c $i -l show-control-chars -d (_ "Non graphic as-is")
		complete -c $i -s Q -l quote-name -d (_ "Enclose entry in quotes")
		complete -c $i -l quoting-style -x -a "literal locale shell shell-always c escape" -d (_ "Select quoting style")
		complete -c $i -l sort -x -d (_ "Sort criteria") -a "
			extension\t'Sort by file extension'
			none\tDon't\ sort
			size\t'Sort by size'
			time\t'Sort by modification time'
			version\t'Sort by version'
			status\t'Sort by file status modification time'
			atime\t'Sort by access time'
			access\t'Sort by access time'
			use\t'Sort by access time'
		"
		complete -c $i -l time -x -d (_ "Show time type") -a "
			time\t'Sort by modification time'
			access\t'Sort by access time'
			use\t'Sort by use time'
			ctime\t'Sort by file status modification time'
			status\t'Sort by status time'
		"
		complete -c $i -l time-style -x -a "full-iso long-iso iso locale" -d (_ "Select time style")
		complete -c $i -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d (_ "Assume tab stops at each COLS")
		complete -c $i -s U -d (_ "Do not sort")
		complete -c $i -s v -d (_ "Sort by version")
		complete -c $i -s w -l width -x -d (_ "Assume screen width")
		complete -c $i -s X -d (_ "Sort by extension")
		complete -c $i -l help -d (_ "Display help and exit")
		complete -c $i -l version -d (_ "Display version and exit")

	else

		# If not a GNU system, assume we have standard BSD ls features instead

		complete -c $i -s B -d (_ "Octal escapes for non graphic characters")
		complete -c $i -s G -d (_ "Use colors")
		complete -c $i -s I -d (_ "Prevent -A from being automatically set for root")
		complete -c $i -s P -d (_ "Don't follow symlinks")
		complete -c $i -s T -d (_ "Show modification time")
		complete -c $i -s W -d (_ "Show whiteouts when scanning directories")
		complete -c $i -s Z -d (_ "Display each file's MAC label")
		complete -c $i -s o -d (_ "Include the file flags in a long (-l) output")
		complete -c $i -s w -d (_ "Print raw entry names")

	end
end
