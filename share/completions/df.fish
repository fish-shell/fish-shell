#
# Completions for df
#

#
# Test if we are using GNU sed
#

set -l is_gnu 
df --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

__fish_gnu_complete -c df -s h -l human-readable -d (N_ "Human readable sizes") $is_gnu
__fish_gnu_complete -c df -s i -l inodes -d (N_ "List inode information") $is_gnu
__fish_gnu_complete -c df -s k -d (N_ "Use 1kB block size") $is_gnu
__fish_gnu_complete -c df -s l -l local -d (N_ "List only local filesystems") $is_gnu
__fish_gnu_complete -c df -s P -l portability -d (N_ "Use Posix format") $is_gnu
__fish_gnu_complete -c df -s t -l type -r -d (N_ "Show filesystems of specified type") $is_gnu -x -a "(__fish_print_filesystems)"

if test -n "$is_gnu"

	complete -c df -s a -l all -d (N_ "Include empty filesystems")
	complete -c df -s B -l block-size -r -d (N_ "Block size")
	complete -c df -s H -l si -d (N_ "Human readable sizes, powers of 1000")
	complete -c df -l no-sync -d (N_ "Do not sync before getting usage info")
	complete -c df -l sync -d (N_ "Sync before getting usage info")
	complete -c df -s T -l print-type -d (N_ "Print filesystem type")
	complete -c df -s x -l exclude-type -d (N_ "Excluded filesystem type") -r -x -a "(__fish_print_filesystems)"
	complete -c df -l help -d (N_ "Display help and exit")
	complete -c df -l version -d (N_ "Display version and exit")

else

	complete -c df -s a -d (N_ "Show all filesystems")
	complete -c df -s g -d (N_ "Show sizes in gigabytes")
	complete -c df -s m -d (N_ "Show sizes in megabytes")
	complete -c df -s n -d (N_ "Print out the previously obtained statistics from the file systems")

end
