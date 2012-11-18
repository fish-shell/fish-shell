# Completions for the 'find' command

# There are quite a few of them...

# Switches for how to handle symlinks

complete -c find -s P --description "Never follow symlinks"
complete -c find -s L -o follow --description "Follow symlinks"
complete -c find -s H --description "Do  not  follow symbolic links, except while processing the command line arguments"


# General options

complete -c find -o daystart --description "Measure from the beginning of today rather than  from  24  hours ago"
complete -c find -s d -o depth --description "Process subdirectories before the directory itself"
complete -c find -o help -l help --description "Display help and exit"
complete -c find -o ignore_readdir_race --description "Do not print error messages for files that are deleted while running find"
complete -c find -o maxdepth --description "Maximum recursion depth" -a "1 2 3 4 5 6 7 8 9"
complete -c find -o mindepth --description "Do not apply any tests or actions at levels less than specified level" -a "1 2 3 4 5 6 7 8 9"
complete -c find -o mount -o xdev --description "Don't descend directories on other  filesystems"
complete -c find -o noignore_readdir_race --description "Print error messages for files that are deleted while running find"
complete -c find -o noleaf --description "Do not optimize by assuming that  directories  contain  2  fewer subdirectories  than  their  hard  link  count"
complete -c find -o regextype --description "Specify regular expression type" -a "emacs posix-awk posix-basic posiz-egrep posix-extended"
complete -c find -o version -l version --description "Display version and exit"
complete -c find -o warn --description "Turn warnings on"
complete -c find -o nowarn --description "Turn warnings off"


# Tests

complete -c find -o amin --description "File last accessed specified number of minutes ago" -r
complete -c find -o anewer --description "File last accessed more recently than file was modified" -r
complete -c find -o atime --description "File last accessed specified number of days ago" -r

complete -c find -o cmin --description "File status last changed specified number of minutes ago" -r
complete -c find -o cnewer --description "File status last changed more recently than file was modified" -r
complete -c find -o ctime --description "File status last changed specified number of days ago" -r

complete -c find -o empty --description "File is empty and is either a regular file or a directory"
complete -c find -o executable --description "File is executable"
complete -c find -o false --description "Always false"
complete -c find -o fstype --description "File is on filesystem of specified type" -a "(__fish_print_filesystems)" -r
complete -c find -o gid --description "Numeric group id of file" -r
complete -c find -o group --description "Group name of file" -a "(__fish_complete_groups)"

complete -c find -o ilname --description "File is symlink matching specified case insensitive pattern" -r
complete -c find -o iname --description "File name matches case insensitive pattern" -r
complete -c find -o inum --description "File has specified inode number" -r
complete -c find -o ipath -o iwholename --description "File path matches case insensitive pattern" -r
complete -c find -o iregex --description "File name matches case insensetive regex" -r

complete -c find -o links --description "File has specified number of links" -r -a "1 2 3 4 5 6 7 8 9"
complete -c find -o lname --description "File is symlink matching specified pattern" -r

complete -c find -o mmin --description "File last modified specified number of minutes ago" -r
complete -c find -o newer --description "File last modified more recently than file was modified" -r
complete -c find -o mtime --description "File last modified specified number of days ago" -r

complete -c find -o name --description "File name matches pattern" -r

complete -c find -o nouser --description "No user corresponds to file's numeric user ID"
complete -c find -o nogroup --description "No group corresponds to file's numeric group ID"

complete -c find -o path -o wholename --description "File path matches pattern" -r

complete -c find -o perm --description "Files has specified permissions set" -r

complete -c find -o regex --description "File name matches regex" -r

complete -c find -o samefile --description "File refers to the same inode as specified file" -r
complete -c find -o size --description "File uses specified units of space" -r
complete -c find -o true --description "Always true"

set -l type_comp 'b\t"Block device" c\t"Character device" d\t"Directory" p\t"Named pipe" f\t"File" l\t"Symbolic link" s\t"Socket"'

complete -c find -o type --description "File is of specified type" -x -a  $type_comp

complete -c find -o uid --description "File's owner has specified numeric user ID" -r
complete -c find -o used --description "File was last accessed specified number of days after its status was last changed" -r
complete -c find -o user --description "File's owner" -a "(__fish_complete_users)" -r
complete -c find -o xtype --description "Check type of file - in case of symlink, check the file that is not checked by -type" -x -a $type_comp
complete -c find -o context --description "File's security context matches specified pattern" -r


# Actions

complete -c find -o delete --description "Delete selected files"
complete -c find -o exec --description "Execute specified command for each located file" -r
complete -c find -o execdir --description "Execute specified command for each located file, in the files directory" -r
complete -c find -o fls --description "List file in ls -dils format, write to specified file" -r
complete -c find -o fprint --description "Print full file names into specified file" -r
complete -c find -o fprint0 --description "Print null separated full file names into specified file" -r
complete -c find -o fprintf --description "Print formated data into specified file" -r
complete -c find -o ok --description "Execute specified command for each located file after asking user" -r
complete -c find -o print --description "Print full file names"
complete -c find -o okdir --description "Execute specified command for each located file, in the files directory, after asking user" -r
complete -c find -o print0 --description "Print null separated full file names"
complete -c find -o printf --description "Print formated data" -r
complete -c find -o prune --description "Do not recurse unless -depth is specified"
complete -c find -o quit --description "Exit at once"
complete -c find -o ls --description "List file in ls -dils format" -r

# Grouping

complete -c find -o not --description "Negate result of action"
complete -c find -o and -s a --description "Result is only true if both previous and next action are true"
complete -c find -o or -s o --description "Result is true if either previous or next action are true"





