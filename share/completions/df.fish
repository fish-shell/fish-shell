#
# Completions for df
#

#
# Test if we are using GNU sed
#

set -l is_gnu
df --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

__fish_gnu_complete -c df -s h -l human-readable --description "Human readable sizes" $is_gnu
__fish_gnu_complete -c df -s i -l inodes --description "List inode information" $is_gnu
__fish_gnu_complete -c df -s k --description "Use 1kB block size" $is_gnu
__fish_gnu_complete -c df -s l -l local --description "List only local file systems" $is_gnu
__fish_gnu_complete -c df -s P -l portability --description "Use POSIX format" $is_gnu
__fish_gnu_complete -c df -s t -l type -r --description "Show file systems of specified type" $is_gnu -x -a "(__fish_print_filesystems)"

if test -n "$is_gnu"

        complete -c df -s a -l all --description "Include empty file systems"
	complete -c df -s B -l block-size -r --description "Block size"
	complete -c df -s H -l si --description "Human readable sizes, powers of 1000"
	complete -c df -l no-sync --description "Do not sync before getting usage info"
	complete -c df -l sync --description "Sync before getting usage info"
        complete -c df -s T -l print-type --description "Print file system type"
        complete -c df -s x -l exclude-type --description "Excluded file system type" -r -x -a "(__fish_print_filesystems)"
	complete -c df -l help --description "Display help and exit"
	complete -c df -l version --description "Display version and exit"

else

        complete -c df -s a --description "Show all file systems"
	complete -c df -s g --description "Show sizes in gigabytes"
	complete -c df -s m --description "Show sizes in megabytes"
	complete -c df -s n --description "Print out the previously obtained statistics from the file systems"

end
