#
# Completions for the ls command and its aliases
#

# Shared ls switches
complete -c ls -s C --description "List by columns"
complete -c ls -s S --description "Sort by size"
complete -c ls -s c --description "Show and sort by ctime"
complete -c ls -s f --description "Don't sort"
complete -c ls -s g --description "Long format without owner"
complete -c ls -s k --description "Set blocksize to 1kB"
complete -c ls -s l --description "Long format"
complete -c ls -s m --description "Comma separated format"
complete -c ls -s t --description "Sort by modification time"
complete -c ls -s u --description "Show access time"
complete -c ls -s x --description "List entries by lines"
complete -c ls -s 1 --description "List one file per line"

# Test if we are using GNU ls
if command ls --version >/dev/null ^/dev/null
    complete -c ls -s a -l all --description "Show hidden"
    complete -c ls -s A -l almost-all --description "Show hidden except . and .."
    complete -c ls -s F -l classify --description "Append filetype indicator"
    complete -c ls -s H -l dereference-command-line --description "Follow symlinks"
    complete -c ls -s L -l dereference --description "Follow symlinks"
    complete -c ls -s R -l recursive --description "List subdirectory recursively"
    complete -c ls -s b -l escape --description "Octal escapes for non graphic characters"
    complete -c ls -s d -l directory --description "List directories, not their content"
    complete -c ls -s h -l human-readable --description "Human readable sizes"
    complete -c ls -s i -l inode --description "Print inode number of files"
    complete -c ls -s n -l numeric-uid-gid --description "Long format, numeric IDs"
    complete -c ls -s p -l file-type --description "Append filetype indicator"
    complete -c ls -s q -l hide-control-chars --description "Replace non-graphic characters with '?'"
    complete -c ls -s r -l reverse --description "Reverse sort order"
    complete -c ls -s s -l size --description "Print size of files"

    # GNU specific ls switches
    complete -c ls -l group-directories-first --description "Group directories before files" -r
    complete -c ls -l hide --description "Do not list implied entries matching specified shell pattern" -r
    complete -c ls -l lcontext --description "Display security context"
    complete -c ls -l context -s Z --description "Display  security  context  so  it fits on most displays"
    complete -c ls -l scontext --description "Display only security context and file name"
    complete -c ls -l author --description "Print author"
    complete -c ls -l block-size -x --description "Set block size"
    complete -c ls -s B -l ignore-backups --description "Ignore files ending with ~"
    complete -c ls -l color -f -a "never always auto" --description "Use colors"
    complete -c ls -s D -l dired --description "Generate dired output"
    complete -c ls -l format -x -a "across commas horizontal long single-column verbose vertical" --description "List format"
    complete -c ls -l full-time --description "Long format, full-iso time"
    complete -c ls -s G -l no-group --description "Don't print group information"
    complete -c ls -l si --description "Human readable sizes, powers of 1000"
    complete -c ls -l dereference-command-line-symlink-to-dir #--description "Follow directory symlinks from command line"
    complete -c ls -l indicator-style -x -a "none classify file-type" --description "Append filetype indicator"
    complete -c ls -s I -l ignore -r --description "Skip entries matching pattern"
    complete -c ls -s N -l literal --description "Print raw entry names"
    complete -c ls -s o --description "Long format without groups"
    complete -c ls -l show-control-chars --description "Non graphic as-is"
    complete -c ls -s Q -l quote-name --description "Enclose entry in quotes"
    complete -c ls -l quoting-style -x -a "literal locale shell shell-always c escape" --description "Select quoting style"
    complete -c ls -l sort -x --description "Sort criteria" -a "
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
    complete -c ls -l time -x --description "Show time type" -a "
			time\t'Sort by modification time'
			access\t'Sort by access time'
			use\t'Sort by use time'
			ctime\t'Sort by file status modification time'
			status\t'Sort by status time'
		"
    complete -c ls -l time-style -x -a "full-iso long-iso iso locale" --description "Select time style"
    complete -c ls -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" --description "Assume tab stops at each COLS"
    complete -c ls -s U --description "Do not sort"
    complete -c ls -s v --description "Sort by version"
    complete -c ls -s w -l width -x --description "Assume screen width"
    complete -c ls -s X --description "Sort by extension"
    complete -c ls -l help --description "Display help and exit"
    complete -c ls -l version --description "Display version and exit"
else
    # If not a GNU system, assume we have standard BSD ls features instead
    complete -c ls -s B --description "Octal escapes for non graphic characters"
    complete -c ls -s G --description "Use colors"
    complete -c ls -s I --description "Prevent -A from being automatically set for root"
    complete -c ls -s P --description "Don't follow symlinks"
    complete -c ls -s T --description "Show modification time"
    complete -c ls -s W --description "Show whiteouts when scanning directories"
    complete -c ls -s Z --description "Display each file's MAC label"
    complete -c ls -s o --description "Include the file flags in a long (-l) output"
    complete -c ls -s w --description "Print raw entry names"

    complete -c ls -s a --description "Show hidden"
    complete -c ls -s A --description "Show hidden except . and .."
    complete -c ls -s F --description "Append filetype indicator"
    complete -c ls -s H --description "Follow symlinks"
    complete -c ls -s L --description "Follow symlinks"
    complete -c ls -s R --description "List subdirectory recursively"
    complete -c ls -s b --description "Octal escapes for non graphic characters"
    complete -c ls -s d --description "List directories, not their content"
    complete -c ls -s h --description "Human readable sizes"
    complete -c ls -s i --description "Print inode number of files"
    complete -c ls -s n --description "Long format, numeric IDs"
    complete -c ls -s p --description "Append filetype indicator"
    complete -c ls -s q --description "Replace non-graphic characters with '?'"
    complete -c ls -s r --description "Reverse sort order"
    complete -c ls -s s --description "Print size of files"
end
