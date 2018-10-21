#
# Completions for the ls command and its aliases
#

# Shared ls switches
complete -c ls -s C -d "List by columns"
complete -c ls -s S -d "Sort by size"
complete -c ls -s c -d "Show and sort by ctime"
complete -c ls -s f -d "Don't sort"
complete -c ls -s g -d "Long format without owner"
complete -c ls -s k -d "Set blocksize to 1kB"
complete -c ls -s l -d "Long format"
complete -c ls -s m -d "Comma separated format"
complete -c ls -s t -d "Sort by modification time"
complete -c ls -s u -d "Show access time"
complete -c ls -s x -d "List entries by lines"
complete -c ls -s 1 -d "List one file per line"

# Test if we are using GNU ls
if command ls --version >/dev/null 2>/dev/null
    complete -c ls -s a -l all -d "Show hidden"
    complete -c ls -s A -l almost-all -d "Show hidden except . and .."
    complete -c ls -s F -l classify -d "Append filetype indicator"
    complete -c ls -s H -l dereference-command-line -d "Follow symlinks"
    complete -c ls -s L -l dereference -d "Follow symlinks"
    complete -c ls -s R -l recursive -d "List subdirectory recursively"
    complete -c ls -s b -l escape -d "Octal escapes for non graphic characters"
    complete -c ls -s d -l directory -d "List directories, not their content"
    complete -c ls -s h -l human-readable -d "Human readable sizes"
    complete -c ls -s i -l inode -d "Print inode number of files"
    complete -c ls -s n -l numeric-uid-gid -d "Long format, numeric IDs"
    complete -c ls -s p -l file-type -d "Append filetype indicator"
    complete -c ls -s q -l hide-control-chars -d "Replace non-graphic characters with '?'"
    complete -c ls -s r -l reverse -d "Reverse sort order"
    complete -c ls -s s -l size -d "Print size of files"

    # GNU specific ls switches
    complete -c ls -l group-directories-first -d "Group directories before files" -r
    complete -c ls -l hide -d "Do not list implied entries matching specified shell pattern" -r
    complete -c ls -l lcontext -d "Display security context"
    complete -c ls -l context -s Z -d "Display  security  context  so  it fits on most displays"
    complete -c ls -l scontext -d "Display only security context and file name"
    complete -c ls -l author -d "Print author"
    complete -c ls -l block-size -x -d "Set block size"
    complete -c ls -s B -l ignore-backups -d "Ignore files ending with ~"
    complete -c ls -l color -f -a "never always auto" -d "Use colors"
    complete -c ls -s D -l dired -d "Generate dired output"
    complete -c ls -l format -x -a "across commas horizontal long single-column verbose vertical" -d "List format"
    complete -c ls -l full-time -d "Long format, full-iso time"
    complete -c ls -s G -l no-group -d "Don't print group information"
    complete -c ls -l si -d "Human readable sizes, powers of 1000"
    complete -c ls -l dereference-command-line-symlink-to-dir #-d "Follow directory symlinks from command line"
    complete -c ls -l indicator-style -x -a "none classify file-type" -d "Append filetype indicator"
    complete -c ls -s I -l ignore -r -d "Skip entries matching pattern"
    complete -c ls -s N -l literal -d "Print raw entry names"
    complete -c ls -s o -d "Long format without groups"
    complete -c ls -l show-control-chars -d "Non graphic as-is"
    complete -c ls -s Q -l quote-name -d "Enclose entry in quotes"
    complete -c ls -l quoting-style -x -a "literal locale shell shell-always c escape" -d "Select quoting style"
    complete -c ls -l sort -x -d "Sort criteria" -a "
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
    complete -c ls -l time -x -d "Show time type" -a "
			time\t'Sort by modification time'
			access\t'Sort by access time'
			use\t'Sort by use time'
			ctime\t'Sort by file status modification time'
			status\t'Sort by status time'
		"
    complete -c ls -l time-style -x -a "full-iso long-iso iso locale" -d "Select time style"
    complete -c ls -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d "Assume tab stops at each COLS"
    complete -c ls -s U -d "Do not sort"
    complete -c ls -s v -d "Sort by version"
    complete -c ls -s w -l width -x -d "Assume screen width"
    complete -c ls -s X -d "Sort by extension"
    complete -c ls -l help -d "Display help and exit"
    complete -c ls -l version -d "Display version and exit"
else
    # If not a GNU system, assume we have standard BSD ls features instead
    complete -c ls -s B -d "Octal escapes for non graphic characters"
    complete -c ls -s G -d "Use colors"
    complete -c ls -s I -d "Prevent -A from being automatically set for root"
    complete -c ls -s P -d "Don't follow symlinks"
    complete -c ls -s T -d "Show modification time"
    complete -c ls -s W -d "Show whiteouts when scanning directories"
    complete -c ls -s Z -d "Display each file's MAC label"
    complete -c ls -s o -d "Include the file flags in a long (-l) output"
    complete -c ls -s w -d "Print raw entry names"

    complete -c ls -s a -d "Show hidden"
    complete -c ls -s A -d "Show hidden except . and .."
    complete -c ls -s F -d "Append filetype indicator"
    complete -c ls -s H -d "Follow symlinks"
    complete -c ls -s L -d "Follow symlinks"
    complete -c ls -s R -d "List subdirectory recursively"
    complete -c ls -s b -d "Octal escapes for non graphic characters"
    complete -c ls -s d -d "List directories, not their content"
    complete -c ls -s h -d "Human readable sizes"
    complete -c ls -s i -d "Print inode number of files"
    complete -c ls -s n -d "Long format, numeric IDs"
    complete -c ls -s p -d "Append filetype indicator"
    complete -c ls -s q -d "Replace non-graphic characters with '?'"
    complete -c ls -s r -d "Reverse sort order"
    complete -c ls -s s -d "Print size of files"
end
