complete -y mount

complete -c df -s a -l all -d (_ "Include empty filesystems")
complete -c df -s B -l block-size -r -d (_ "Block size")
complete -c df -s h -l human-readable -d (_ "Human readable sizes")
complete -c df -s H -l si -d (_ "Human readable sizes, powers of 1000")
complete -c df -s i -l inodes -d (_ "List inode information")
complete -c df -s k -d (_ "Use 1kB block size")
complete -c df -s l -l local -d (_ "List only local filesystems")
complete -c df -l no-sync -d (_ "Do not sync before getting usage info")
complete -c df -s P -l portability -d (_ "Use Posix format")
complete -c df -l sync -d (_ "Sync before getting usage info")
complete -c df -s t -l type -r -d (_ "Filesystem type") -x -a $__fish_filesystems
complete -c df -s T -l print-type -d (_ "Print filesystem type")
complete -c df -s x -l exclude-type -d (_ "Excluded filesystem type") -r -x -a $__fish_filesystems
complete -c df -l help -d (_ "Display help and exit")
complete -c df -l version -d (_ "Display version and exit")

