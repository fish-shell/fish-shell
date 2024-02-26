# Completions for the 'find' command

# Switches for how to handle symlinks

complete -c find -s P -d "Never follow symlinks"
complete -c find -s L -o follow -d "Follow symlinks"
complete -c find -s H -d "Don't follow symlinks (except for command line arguments)"

# General options

complete -c find -o daystart -d "Measure from the beginning of today rather than from 24 hours ago"
complete -c find -s d -o depth -d "Process subdirectories before the directory itself"
complete -c find -o help -l help -d "Display help and exit"
complete -c find -o ignore_readdir_race -d "Do not print error messages for files that are deleted while running find"
complete -c find -o maxdepth -d "Maximum recursion depth" -a "1 2 3 4 5 6 7 8 9"
complete -c find -o mindepth -d "Do not apply any tests or actions at levels less than specified level" -a "1 2 3 4 5 6 7 8 9"
complete -c find -o mount -o xdev -d "Don't descend directories on other filesystems"
complete -c find -o noignore_readdir_race -d "Print error messages for files that are deleted while running find"
complete -c find -o noleaf -d "Don't assume directories contain 2 fewer subdirs than their hard link count"
complete -c find -o regextype -d "Specify regular expression type" -a "emacs posix-awk posix-basic posix-egrep posix-extended"
complete -c find -o version -l version -d "Display version and exit"
complete -c find -o warn -d "Turn warnings on"
complete -c find -o nowarn -d "Turn warnings off"
complete -c find -o O0 -d "Equivalent to optimisation level 1."
complete -c find -o O1 -d "Default optimisation level and corresponds to the traditional behaviour."
complete -c find -o O2 -d "Any -type or -xtype tests are performed after any tests based only on the names of files."
complete -c find -o O3 -d "The full cost-based query optimiser is enabled."
complete -c find -s D -d "Print diagnostic information." -x -a "exec opt rates search stat time tree all help"

# Tests

complete -c find -o amin -d "File last accessed specified number of minutes ago" -x
complete -c find -o anewer -d "File last accessed more recently than file was modified" -F
complete -c find -o atime -d "File last accessed specified number of days ago" -x

complete -c find -o cmin -d "File status last changed specified number of minutes ago" -x
complete -c find -o cnewer -d "File status last changed more recently than file was modified" -F
complete -c find -o ctime -d "File status last changed specified number of days ago" -x

complete -c find -o empty -d "File is empty and is either a regular file or a directory"
complete -c find -o executable -d "File is executable"
complete -c find -o false -d "Always false"
complete -c find -o fstype -d "File is on filesystem of specified type" -a "(__fish_print_filesystems)" -x
complete -c find -o gid -d "Numeric group id of file" -x -a "(__fish_complete_group_ids)"
complete -c find -o group -d "Group name of file" -x -a "(__fish_complete_groups)"

complete -c find -o ilname -d "File is symlink matching specified case insensitive pattern" -x
complete -c find -o iname -d "File name matches case insensitive pattern" -x
complete -c find -o inum -d "File has specified inode number" -x
complete -c find -o ipath -o iwholename -d "File path matches case insensitive pattern" -x
complete -c find -o iregex -d "File name matches case insensitive regex" -x

complete -c find -o links -d "File has specified number of links" -x -a "1 2 3 4 5 6 7 8 9"
complete -c find -o lname -d "File is symlink matching specified pattern" -x

complete -c find -o mmin -d "File last modified specified number of minutes ago" -x
complete -c find -o newer -d "File last modified more recently than file was modified" -F
complete -c find -o mtime -d "File last modified specified number of days ago" -x

complete -c find -o name -d "File name matches pattern" -x

complete -c find -o nouser -d "No user corresponds to file's numeric user ID"
complete -c find -o nogroup -d "No group corresponds to file's numeric group ID"

complete -c find -o path -o wholename -d "File path matches pattern" -x

complete -c find -o perm -d "Files has specified permissions set" -x

complete -c find -o regex -d "File name matches regex" -x

complete -c find -o samefile -d "File refers to the same inode as specified file" -F
complete -c find -o size -d "File uses specified units of space" -x
complete -c find -o true -d "Always true"

set -l type_comp 'b\t"Block device" c\t"Character device" d\t"Directory" p\t"Named pipe" f\t"File" l\t"Symbolic link" s\t"Socket"'

complete -c find -o type -d "File is of specified type" -x -a $type_comp

complete -c find -o uid -d "File's owner has specified numeric user ID" -x -a "(__fish_complete_user_ids)"
complete -c find -o used -d "File was last used X number of days after its status was last changed" -x
complete -c find -o user -d "File's owner" -x -a "(__fish_complete_users)"
complete -c find -o xtype -d "Check type of file - in case of symlink, check the file that is not checked by -type" -x -a $type_comp
complete -c find -o context -d "File's security context matches specified pattern" -x

# Actions

complete -c find -o delete -d "Delete selected files"
complete -c find -o exec -d "Execute specified command for each located file" -r
complete -c find -o execdir -d "Execute specified command for each located file, in the files directory" -r
complete -c find -o fls -d "List file in ls -dils format, write to specified file" -F
complete -c find -o fprint -d "Print full file names into specified file" -F
complete -c find -o fprint0 -d "Print null separated full file names into specified file" -F
complete -c find -o fprintf -d "Print formated data into specified file" -F
complete -c find -o ok -d "Execute specified command for each located file after asking user" -r
complete -c find -o print -d "Print full file names"
complete -c find -o okdir -d "Execute a command for each located file, in its directory, after asking" -r
complete -c find -o print0 -d "Print null separated full file names"
complete -c find -o printf -d "Print formated data" -x
complete -c find -o prune -d "Do not recurse unless -depth is specified"
complete -c find -o quit -d "Exit at once"
complete -c find -o ls -d "List file in ls -dils format" -r

# Grouping

complete -c find -o not -d "Negate result of action"
complete -c find -o and -s a -d "Result is only true if both previous and next action are true"
complete -c find -o or -s o -d "Result is true if either previous or next action are true"
