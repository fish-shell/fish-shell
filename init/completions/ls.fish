#
# Completions for the ls command and its aliases
#

# Test if we are using GNU ls

set -l is_gnu 
ls --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

set -l cmds    -c ls -c ll -c la

# Shared ls switches

__fish_gnu_complete $cmds -s a -l all -d (_ "Show hidden") $is_gnu
__fish_gnu_complete $cmds -s A -l almost-all -d (_ "Show hidden except . and ..") $is_gnu
__fish_gnu_complete $cmds -s F -l classify -d (_ "Append filetype indicator") $is_gnu
__fish_gnu_complete $cmds -s H -l dereference-command-line -d (_ "Follow symlinks") $is_gnu
__fish_gnu_complete $cmds -s L -l dereference -d (_ "Follow symlinks") $is_gnu
__fish_gnu_complete $cmds -s R -l recursive -d (_ "List subdirectory recursively") $is_gnu
__fish_gnu_complete $cmds -s b -l escape -d (_ "Octal escapes for non graphic characters") $is_gnu
__fish_gnu_complete $cmds -s d -l directory -d (_ "List directories, not their content") $is_gnu
__fish_gnu_complete $cmds -s h -l human-readable -d (_ "Human readable sizes") $is_gnu
__fish_gnu_complete $cmds -s i -l inode -d (_ "Print inode number of files") $is_gnu
__fish_gnu_complete $cmds -s n -l numeric-uid-gid -d (_ "Long format, numeric IDs") $is_gnu
__fish_gnu_complete $cmds -s p -l file-type -d (_ "Append filetype indicator") $is_gnu
__fish_gnu_complete $cmds -s q -l hide-control-chars -d (_ "Replace non-graphic characters with '?'") $is_gnu
__fish_gnu_complete $cmds -s r -l reverse -d (_ "Reverse sort order") $is_gnu
__fish_gnu_complete $cmds -s s -l size -d (_ "Print size of files") $is_gnu

complete $cmds -s C -d (_ "List by columns")
complete $cmds -s S -d (_ "Sort by size")
complete $cmds -s c -d (_ "Show and sort by ctime")
complete $cmds -s f -d (_ "Don't sort")
complete $cmds -s g -d (_ "Long format without owner")
complete $cmds -s k -d (_ "Set blocksize to 1kB")
complete $cmds -s l -d (_ "Long format")
complete $cmds -s m -d (_ "Comma separated format")
complete $cmds -s t -d (_ "Sort by modification time")
complete $cmds -s u -d (_ "Show access time")
complete $cmds -s x -d (_ "List entries by lines")
complete $cmds -s 1 -d (_ "List one file per line")

if test -n "$is_gnu"

	# GNU specific features

	complete $cmds -l author -d (_ "Print author")
	complete $cmds -l blocksize -x -d (_ "Set block size")
	complete $cmds -s B -l ignore-backups -d (_ "Ignore files ending with ~")
	complete $cmds -l color -f -a "never always auto" -d (_ "Use colors")
	complete $cmds -s D -l dired -d (_ "Generate dired output")
	complete $cmds -l format -x -a "across commas horizontal long single-column verbose vertical" -d (_ "List format")
	complete $cmds -l full-time -d (_ "Long format, full-iso time")
	complete $cmds -s G -l no-group -d (_ "Don't print group information")
	complete $cmds -l si -d (_ "Human readable sizes, powers of 1000")
	complete $cmds -l dereference-command-line-symlink-to-dir #-d (_ "Follow directory symlinks from command line")
	complete $cmds -l indicator-style -x -a "none classify file-type" -d (_ "Append filetype indicator")
	complete $cmds -s I -l ignore -r -d (_ "Skip entries matching pattern")
	complete $cmds -s N -l literal -d (_ "Print raw entry names")
	complete $cmds -s o -d (_ "Long format without groups")
	complete $cmds -l show-control-chars -d (_ "Non graphic as-is")
	complete $cmds -s Q -l quote-name -d (_ "Enclose entry in quotes")
	complete $cmds -l quoting-style -x -a "literal locale shell shell-always c escape" -d (_ "Select quoting style")
	complete $cmds -l sort -x -d (_ "Sort criteria") -a "
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
	complete $cmds -l time -x -d (_ "Show time type") -a "
		time\t'Sort by modification time'
		access\t'Sort by access time'
		use\t'Sort by use time'
		ctime\t'Sort by file status modification time'
		status\t'Sort by status time'
	"
	complete $cmds -l time-style -x -a "full-iso long-iso iso locale" -d (_ "Select time style")
	complete $cmds -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d (_ "Assume tab stops at each COLS")
	complete $cmds -s U -d (_ "Do not sort")
	complete $cmds -s v -d (_ "Sort by version")
	complete $cmds -s w -l width -x -d (_ "Assume screen width")
	complete $cmds -s X -d (_ "Sort by extension")
	complete $cmds -l help -d (_ "Display help and exit")
	complete $cmds -l version -d (_ "Display version and exit")

else

	# If not a GNU system, assume we have standard BSD ls features instead

	complete $cmds -s B -d (_ "Octal escapes for non graphic characters")
	complete $cmds -s G -d (_ "Use colors")
	complete $cmds -s I -d (_ "Prevent -A from being automatically set for root")
	complete $cmds -s P -d (_ "Don't follow symlinks")
	complete $cmds -s T -d (_ "Show modification time")
	complete $cmds -s W -d (_ "Show whiteouts when scanning directories")
	complete $cmds -s Z -d (_ "Display each file's MAC label")
	complete $cmds -s o -d (_ "Include the file flags in a long (-l) output")
	complete $cmds -s w -d (_ "Print raw entry names")

end
