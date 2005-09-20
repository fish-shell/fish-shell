complete -y mount

complete -c df -s a -l all -d "Include empty filesystems"
complete -c df -s B -l block-size -r -d "Block size"
complete -c df -s h -l human-readable -d "Human readable sizes"
complete -c df -s H -l si -d "Human readable sizes, powers of 1000"
complete -c df -s i -l inodes -d "List inode information"
complete -c df -s k -d "Use 1KB block size"
complete -c df -s l -l local -d "List only local filesystems"
complete -c df -l no-sync -d "Do not sync before getting usage info"
complete -c df -s P -l portability -d "Use Posix format"
complete -c df -l sync -d "Sync before getting usage info"
complete -c df -s t -l type -r -d "Filesystem type" -x -a $__fish_filesystems
complete -c df -s T -l print-type -d "Print filesystem type"
complete -c df -s x -l exclude-type -d "Excluded filesystem type" -r -x -a $__fish_filesystems
complete -c df -l help -d "Display help and exit"
complete -c df -l version -d "Display version and exit"

