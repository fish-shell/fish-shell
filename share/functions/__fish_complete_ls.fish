#
# Completions for the ls command and its aliases
#

# Test if we are using GNU ls

function __fish_complete_ls -d "Compleletions for ls and it's aliases"
	
	set -l is_gnu 
	command ls --version >/dev/null ^/dev/null; and set is_gnu --is-gnu
	
	set -l cmds -c $argv
	
	# Shared ls switches
	
	__fish_gnu_complete $cmds -s a -l all -d (N_ "Show hidden") $is_gnu
	__fish_gnu_complete $cmds -s A -l almost-all -d (N_ "Show hidden except . and ..") $is_gnu
	__fish_gnu_complete $cmds -s F -l classify -d (N_ "Append filetype indicator") $is_gnu
	__fish_gnu_complete $cmds -s H -l dereference-command-line -d (N_ "Follow symlinks") $is_gnu
	__fish_gnu_complete $cmds -s L -l dereference -d (N_ "Follow symlinks") $is_gnu
	__fish_gnu_complete $cmds -s R -l recursive -d (N_ "List subdirectory recursively") $is_gnu
	__fish_gnu_complete $cmds -s b -l escape -d (N_ "Octal escapes for non graphic characters") $is_gnu
	__fish_gnu_complete $cmds -s d -l directory -d (N_ "List directories, not their content") $is_gnu
	__fish_gnu_complete $cmds -s h -l human-readable -d (N_ "Human readable sizes") $is_gnu
	__fish_gnu_complete $cmds -s i -l inode -d (N_ "Print inode number of files") $is_gnu
	__fish_gnu_complete $cmds -s n -l numeric-uid-gid -d (N_ "Long format, numeric IDs") $is_gnu
	__fish_gnu_complete $cmds -s p -l file-type -d (N_ "Append filetype indicator") $is_gnu
	__fish_gnu_complete $cmds -s q -l hide-control-chars -d (N_ "Replace non-graphic characters with '?'") $is_gnu
	__fish_gnu_complete $cmds -s r -l reverse -d (N_ "Reverse sort order") $is_gnu
	__fish_gnu_complete $cmds -s s -l size -d (N_ "Print size of files") $is_gnu
	
	complete $cmds -s C -d (N_ "List by columns")
	complete $cmds -s S -d (N_ "Sort by size")
	complete $cmds -s c -d (N_ "Show and sort by ctime")
	complete $cmds -s f -d (N_ "Don't sort")
	complete $cmds -s g -d (N_ "Long format without owner")
	complete $cmds -s k -d (N_ "Set blocksize to 1kB")
	complete $cmds -s l -d (N_ "Long format")
	complete $cmds -s m -d (N_ "Comma separated format")
	complete $cmds -s t -d (N_ "Sort by modification time")
	complete $cmds -s u -d (N_ "Show access time")
	complete $cmds -s x -d (N_ "List entries by lines")
	complete $cmds -s 1 -d (N_ "List one file per line")
	
	if test -n "$is_gnu"
	   
	   	# GNU specific features
	
		complete $cmds -l author -d (N_ "Print author")
		complete $cmds -l blocksize -x -d (N_ "Set block size")
		complete $cmds -s B -l ignore-backups -d (N_ "Ignore files ending with ~")
		complete $cmds -l color -f -a "never always auto" -d (N_ "Use colors")
		complete $cmds -s D -l dired -d (N_ "Generate dired output")
		complete $cmds -l format -x -a "across commas horizontal long single-column verbose vertical" -d (N_ "List format")
		complete $cmds -l full-time -d (N_ "Long format, full-iso time")
		complete $cmds -s G -l no-group -d (N_ "Don't print group information")
		complete $cmds -l si -d (N_ "Human readable sizes, powers of 1000")
		complete $cmds -l dereference-command-line-symlink-to-dir #-d (N_ "Follow directory symlinks from command line")
		complete $cmds -l indicator-style -x -a "none classify file-type" -d (N_ "Append filetype indicator")
		complete $cmds -s I -l ignore -r -d (N_ "Skip entries matching pattern")
		complete $cmds -s N -l literal -d (N_ "Print raw entry names")
		complete $cmds -s o -d (N_ "Long format without groups")
		complete $cmds -l show-control-chars -d (N_ "Non graphic as-is")
		complete $cmds -s Q -l quote-name -d (N_ "Enclose entry in quotes")
		complete $cmds -l quoting-style -x -a "literal locale shell shell-always c escape" -d (N_ "Select quoting style")
		complete $cmds -l sort -x -d (N_ "Sort criteria") -a "
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
		complete $cmds -l time -x -d (N_ "Show time type") -a "
			time\t'Sort by modification time'
			access\t'Sort by access time'
			use\t'Sort by use time'
			ctime\t'Sort by file status modification time'
			status\t'Sort by status time'
		"
		complete $cmds -l time-style -x -a "full-iso long-iso iso locale" -d (N_ "Select time style")
		complete $cmds -s T -l tabsize -x -a "1 2 3 4 5 6 7 8 9 10 11 12" -d (N_ "Assume tab stops at each COLS")
		complete $cmds -s U -d (N_ "Do not sort")
		complete $cmds -s v -d (N_ "Sort by version")
		complete $cmds -s w -l width -x -d (N_ "Assume screen width")
		complete $cmds -s X -d (N_ "Sort by extension")
		complete $cmds -l help -d (N_ "Display help and exit")
		complete $cmds -l version -d (N_ "Display version and exit")
	
	else
	
		# If not a GNU system, assume we have standard BSD ls features instead
	
		complete $cmds -s B -d (N_ "Octal escapes for non graphic characters")
		complete $cmds -s G -d (N_ "Use colors")
		complete $cmds -s I -d (N_ "Prevent -A from being automatically set for root")
		complete $cmds -s P -d (N_ "Don't follow symlinks")
		complete $cmds -s T -d (N_ "Show modification time")
		complete $cmds -s W -d (N_ "Show whiteouts when scanning directories")
		complete $cmds -s Z -d (N_ "Display each file's MAC label")
		complete $cmds -s o -d (N_ "Include the file flags in a long (-l) output")
		complete $cmds -s w -d (N_ "Print raw entry names")
	
	end
	
end
