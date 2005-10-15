
for i in ls ll la

	complete -c $i -s a -l all -d "Show hidden"
	complete -c $i -s A -l almost-all -d "Show hidden except . and .."
	complete -c $i -l author -d "Print author"
	complete -c $i -s b -l escape -d "Octal escapes for non graphic"
	complete -c $i -l blocksize -x -d "Use SIZE-byte blocks"
	complete -c $i -s B -l ignore-backups -d "Ignore files ending with ~"
	complete -c $i -s c -d "Show and sort by ctime"
	complete -c $i -s C -d "List by columns"
	complete -c $i -l color -f -a "never always auto" -d "Show colors"
	complete -c $i -s d -l directory -d "List directories"
	complete -c $i -s D -l dired -d "Generate dired output"
	complete -c $i -s f -d "Do not sort"
	complete -c $i -s F -l classify -d "Append filetype indicator"
	complete -c $i -l format -x -a "across commas horizontal long single-column verbose vertical" -d "List format"
	complete -c $i -l full-time -d "Long forma, full-iso time"
	complete -c $i -s g -d "Long format without owner"
	complete -c $i -s G -l no-group -d "Do not print group information"
	complete -c $i -s h -l human-readable -d "Human readable size"
	complete -c $i -l si -d "Human readable size, base 1000"
	complete -c $i -s H -l dereference-command-line -d "Follow symlinks"
	complete -c $i -l dereference-command-line-symlink-to-dir #-d "Follow directory symlinks from command line"
	complete -c $i -l indicator-style -x -a "none classify file-type" -d "Append indicator to entry"
	complete -c $i -s i -l inode -d "Print index number of files"
	complete -c $i -s I -l ignore -r -d "Skip entries matching pattern"
	complete -c $i -s k -d "like --block-size=1K"
	complete -c $i -s l -d "Long format"
	complete -c $i -s L -l dereference -d "Follow symlinks"
	complete -c $i -s m -d "Comma separated format"
	complete -c $i -s n -l numeric-uid-gid -d "Long format, numeric IDs"
	complete -c $i -s N -l literal -d "Print raw entry names"
	complete -c $i -s o -d "Long format without groups"
	complete -c $i -s p -l file-type -d "Append indicator"
	complete -c $i -s q -l hide-control-chars -d "? fo non graphic"
	complete -c $i -l show-control-chars -d "Non graphic as-is"
	complete -c $i -s Q -l quote-name -d "Enclose entry in quotes"
	complete -c $i -l quoting-style -x -a "literal locale shell shell-always c escape" -d "Select quoting style"
	complete -c $i -s r -l reverse -d "Reverse sort order"
	complete -c $i -s R -l recursive -d "List subdirectory recursively"
	complete -c $i -s s -l size -d "Print size of files"
	complete -c $i -s S -d "Sort by size"
	complete -c $i -l sort -x -d "Sort criteria" -a "
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
	complete -c $i -l time -x -d "Show time type" -a "
		time\t'Sort by modification time'
		access\t'Sort by access time'
		use\t'Sort by use time'
		ctime\t'Sort by file status modification time'
		status\t'Sort by status time'
	"
	complete -c $i -l time-style -x -a "full-iso long-iso iso locale" -d "Select time style"
	complete -c $i -s t -d "Sort by modification time"
	complete -c $i -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d "Assume tab stops at each COLS"
	complete -c $i -s u -d "Show access time"
	complete -c $i -s U -d "Do not sort"
	complete -c $i -s v -d "Sort by version"
	complete -c $i -s w -l width -x -d "Assume screen width"
	complete -c $i -s x -d "List entries by lines"
	complete -c $i -s X -d "sort by extension"
	complete -c $i -s 1 -d "List one file per line"
	complete -c $i -l help -d "Display help and exit"
	complete -c $i -l version -d "Output version and exit"

end
