#
# Completions for df
#

#
# Test if we are using GNU sed
#

set -l is_gnu 
df --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

__fish_gnu_complete -c df -s h -l human-readable -d (_ "Human readable sizes") $is_gnu
__fish_gnu_complete -c df -s i -l inodes -d (_ "List inode information") $is_gnu
__fish_gnu_complete -c df -s k -d (_ "Use 1kB block size") $is_gnu
__fish_gnu_complete -c df -s l -l local -d (_ "List only local filesystems") $is_gnu
__fish_gnu_complete -c df -s P -l portability -d (_ "Use Posix format") $is_gnu
__fish_gnu_complete -c df -s t -l type -r -d (_ "Show filesystems of specified type") $is_gnu -x -a "(__fish_print_filesystems)"

if test -n "$is_gnu"

	complete -c df -s a -l all -d (_ "Include empty filesystems")
	complete -c df -s B -l block-size -r -d (_ "Block size")
	complete -c df -s H -l si -d (_ "Human readable sizes, powers of 1000")
	complete -c df -l no-sync -d (_ "Do not sync before getting usage info")
	complete -c df -l sync -d (_ "Sync before getting usage info")
	complete -c df -s T -l print-type -d (_ "Print filesystem type")
	complete -c df -s x -l exclude-type -d (_ "Excluded filesystem type") -r -x -a "(__fish_print_filesystems)"
	complete -c df -l help -d (_ "Display help and exit")
	complete -c df -l version -d (_ "Display version and exit")

else

	complete -c df -s a -d (_ "Show all filesystems")
	complete -c df -s g -d (_ "Show sizes in gigabytes")
	complete -c df -s m -d (_ "Show sizes in megabytes")
	complete -c df -s n -d (_ "Print out the previously obtained statistics from the file systems")

end
