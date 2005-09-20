complete -c ls -s a -l all -d "Show hidden"
complete -c ls -s A -l almost-all -d "Show hidden except . and .."
complete -c ls -l author -d "Print author"
complete -c ls -s b -l escape -d "Octal escapes for non graphic"
complete -c ls -l blocksize -x -d "Use SIZE-byte blocks"
complete -c ls -s B -l ignore-backups -d "Ignore files ending with ~"
complete -c ls -s c -d "Show and sort by ctime"
complete -c ls -s C -d "List by columns"
complete -c ls -l color -f -a "never always auto" -d "Show colors"
complete -c ls -s d -l directory -d "List directories"
complete -c ls -s D -l dired -d "Generate dired output"
complete -c ls -s f -d "Do not sort"
complete -c ls -s F -l classify -d "Append filetype indicator"
complete -c ls -l format -x -a "across commas horizontal long single-column verbose vertical" -d "List format"
complete -c ls -l full-time -d "Long forma, full-iso time"
complete -c ls -s g -d "Long format without owner"
complete -c ls -s G -l no-group -d "Do not print group information"
complete -c ls -s h -l human-readable -d "Human readable size"
complete -c ls -l si -d "Human readable size, base 1000"
complete -c ls -s H -l dereference-command-line -d "Follow symlinks"
complete -c ls -l dereference-command-line-symlink-to-dir #-d "Follow directory symlinks from command line"
complete -c ls -l indicator-style -x -a "none classify file-type" -d "Append indicator to entry"
complete -c ls -s i -l inode -d "Print index number of files"
complete -c ls -s I -l ignore -r -d "Skip entries matching pattern"
complete -c ls -s k -d "like --block-size=1K"
complete -c ls -s l -d "Long format"
complete -c ls -s L -l dereference -d "Follow symlinks"
complete -c ls -s m -d "Comma separated format"
complete -c ls -s n -l numeric-uid-gid -d "Long format, numeric IDs"
complete -c ls -s N -l literal -d "Print raw entry names"
complete -c ls -s o -d "Long format without groups"
complete -c ls -s p -l file-type -d "Append indicator"
complete -c ls -s q -l hide-control-chars -d "? fo non graphic"
complete -c ls -l show-control-chars -d "Non graphic as-is"
complete -c ls -s Q -l quote-name -d "Enclose entry in quotes"
complete -c ls -l quoting-style -x -a "literal locale shell shell-always c escape" -d "Select quoting style"
complete -c ls -s r -l reverse -d "Reverse sort order"
complete -c ls -s R -l recursive -d "List subdirectory recursively"
complete -c ls -s s -l size -d "Print size of files"
complete -c ls -s S -d "Sort by size"
complete -c ls -l sort -x -d "Sort criteria" -a "
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
complete -c ls -l time -x -d "Show time type" -a "
	time\t'Sort by modification time'
	access\t'Sort by access time'
	use\t'Sort by use time'
	ctime\t'Sort by file status modification time'
	status\t'Sort by status time'
"
complete -c ls -l time-style -x -a "full-iso long-iso iso locale" -d "Select time style"
complete -c ls -s t -d "Sort by modification time"
complete -c ls -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d "Assume tab stops at each COLS"
complete -c ls -s u -d "Show access time"
complete -c ls -s U -d "Do not sort"
complete -c ls -s v -d "Sort by version"
complete -c ls -s w -l width -x -d "Assume screen width"
complete -c ls -s x -d "List entries by lines"
complete -c ls -s X -d "sort by extension"
complete -c ls -s 1 -d "List one file per line"
complete -c ls -l help -d "Display help and exit"
complete -c ls -l version -d "Output version and exit"

