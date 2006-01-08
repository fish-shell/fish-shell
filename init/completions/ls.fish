
for i in ls ll la

	complete -c $i -s a -l all -d (_ "Show hidden")
	complete -c $i -s A -l almost-all -d (_ "Show hidden except . and ..")
	complete -c $i -l author -d (_ "Print author")
	complete -c $i -s b -l escape -d (_ "Octal escapes for non graphic")
	complete -c $i -l blocksize -x -d (_ "Use SIZE-byte blocks")
	complete -c $i -s B -l ignore-backups -d (_ "Ignore files ending with ~")
	complete -c $i -s c -d (_ "Show and sort by ctime")
	complete -c $i -s C -d (_ "List by columns")
	complete -c $i -l color -f -a "never always auto" -d (_ "Show colors")
	complete -c $i -s d -l directory -d (_ "List directories")
	complete -c $i -s D -l dired -d (_ "Generate dired output")
	complete -c $i -s f -d (_ "Do not sort")
	complete -c $i -s F -l classify -d (_ "Append filetype indicator")
	complete -c $i -l format -x -a "across commas horizontal long single-column verbose vertical" -d (_ "List format")
	complete -c $i -l full-time -d (_ "Long forma, full-iso time")
	complete -c $i -s g -d (_ "Long format without owner")
	complete -c $i -s G -l no-group -d (_ "Do not print group information")
	complete -c $i -s h -l human-readable -d (_ "Human readable size")
	complete -c $i -l si -d (_ "Human readable size, base 1000")
	complete -c $i -s H -l dereference-command-line -d (_ "Follow symlinks")
	complete -c $i -l dereference-command-line-symlink-to-dir #-d (_ "Follow directory symlinks from command line")
	complete -c $i -l indicator-style -x -a "none classify file-type" -d (_ "Append indicator to entry")
	complete -c $i -s i -l inode -d (_ "Print index number of files")
	complete -c $i -s I -l ignore -r -d (_ "Skip entries matching pattern")
	complete -c $i -s k -d (_ "Like --block-size=1K")
	complete -c $i -s l -d (_ "Long format")
	complete -c $i -s L -l dereference -d (_ "Follow symlinks")
	complete -c $i -s m -d (_ "Comma separated format")
	complete -c $i -s n -l numeric-uid-gid -d (_ "Long format, numeric IDs")
	complete -c $i -s N -l literal -d (_ "Print raw entry names")
	complete -c $i -s o -d (_ "Long format without groups")
	complete -c $i -s p -l file-type -d (_ "Append indicator")
	complete -c $i -s q -l hide-control-chars -d (_ "? fo non graphic")
	complete -c $i -l show-control-chars -d (_ "Non graphic as-is")
	complete -c $i -s Q -l quote-name -d (_ "Enclose entry in quotes")
	complete -c $i -l quoting-style -x -a "literal locale shell shell-always c escape" -d (_ "Select quoting style")
	complete -c $i -s r -l reverse -d (_ "Reverse sort order")
	complete -c $i -s R -l recursive -d (_ "List subdirectory recursively")
	complete -c $i -s s -l size -d (_ "Print size of files")
	complete -c $i -s S -d (_ "Sort by size")
	complete -c $i -l sort -x -d (_ "Sort criteria") -a "
	extension\t'Sort by file extension'
	none\t'Do not sort'
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
	complete -c $i -s t -d (_ "Sort by modification time")
	complete -c $i -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d (_ "Assume tab stops at each COLS")
	complete -c $i -s u -d (_ "Show access time")
	complete -c $i -s U -d (_ "Do not sort")
	complete -c $i -s v -d (_ "Sort by version")
	complete -c $i -s w -l width -x -d (_ "Assume screen width")
	complete -c $i -s x -d (_ "List entries by lines")
	complete -c $i -s X -d (_ "Sort by extension")
	complete -c $i -s 1 -d (_ "List one file per line")
	complete -c $i -l help -d (_ "Display help and exit")
	complete -c $i -l version -d (_ "Display version and exit")

end
