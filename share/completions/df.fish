#
# Completions for df
#

#
# Test if we are using GNU df
#

set -l is_gnu
df --version >/dev/null 2>/dev/null; and set is_gnu --is-gnu

__fish_gnu_complete -c df -s h -l human-readable -d "Human readable sizes" $is_gnu
__fish_gnu_complete -c df -s i -l inodes -d "List inode information" $is_gnu
__fish_gnu_complete -c df -s k -d "Use 1kB block size" $is_gnu
__fish_gnu_complete -c df -s l -l local -d "List only local file systems" $is_gnu
__fish_gnu_complete -c df -s P -l portability -d "Use POSIX format" $is_gnu
__fish_gnu_complete -c df -s t -l type -r -d "Show file systems of specified type" $is_gnu -x -a "(__fish_print_filesystems)"

if test -n "$is_gnu"

    complete -c df -s a -l all -d "Include empty file systems"
    complete -c df -s B -l block-size -r -d "Block size"
    complete -c df -s H -l si -d "Human readable sizes, powers of 1000"
    complete -c df -l no-sync -d "Do not sync before getting usage info"
    complete -c df -l sync -d "Sync before getting usage info"
    complete -c df -s T -l print-type -d "Print file system type"
    complete -c df -s x -l exclude-type -d "Excluded file system type" -r -x -a "(__fish_print_filesystems)"
    complete -c df -l help -d "Display help and exit"
    complete -c df -l version -d "Display version and exit"

else

    complete -c df -s a -d "Show all file systems"
    complete -c df -s g -d "Show sizes in gigabytes"
    complete -c df -s m -d "Show sizes in megabytes"
    complete -c df -s n -d "Use cached statistics"

end
