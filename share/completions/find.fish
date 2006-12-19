# Completions for the 'find' command

# There are quite a few of them...

# Switches for how to handle symlinks

complete -c find -s P -d (N_ "Never follow symlinks")
complete -c find -s L -o follow -d (N_ "Follow symlinks")
complete -c find -s H -d (N_ "Do  not  follow symbolic links, except while processing the command line arguments")


# General options 

complete -c find -o daystart -d (N_ "Measure from the beginning of today rather than  from  24  hours ago")
complete -c find -s d -o depth -d (N_ "Process subdirectories before the directory itself")
complete -c find -o help -l help -d (N_ "Display help and exit")
complete -c find -o ignore_readdir_race -d (N_ "Do not print error messages for files that are deleted while running find")
complete -c find -o maxdepth -d (N_ "Maximum recursion depth") -a "1 2 3 4 5 6 7 8 9"
complete -c find -o mindepth -d (N_ "Do not apply any tests or actions at levels less than specified level") -a "1 2 3 4 5 6 7 8 9"
complete -c find -o mount -o xdev -d (N_ "Don't descend directories on other  filesystems")
complete -c find -o noignore_readdir_race -d (N_ "Print error messages for files that are deleted while running find")
complete -c find -o noleaf -d (N_ "Do not optimize by assuming that  directories  contain  2  fewer subdirectories  than  their  hard  link  count")
complete -c find -o regextype -d (N_ "Specify regular expression type") -a "emacs posix-awk posix-basic posiz-egrep posix-extended"
complete -c find -o version -l version -d (N_ "Display version and exit")
complete -c find -o warn -d (N_ "Turn warnings on")
complete -c find -o nowarn -d (N_ "Turn warnings off")


# Tests

complete -c find -o amin -d (N_ "File last accessed specified number of minutes ago") -r
complete -c find -o anewer -d (N_ "File last accessed more recently than file was modified") -r
complete -c find -o atime -d (N_ "File last accessed specified number of days ago") -r

complete -c find -o cmin -d (N_ "File status last changed specified number of minutes ago") -r
complete -c find -o cnewer -d (N_ "File status last changed more recently than file was modified") -r
complete -c find -o ctime -d (N_ "File status last changed specified number of days ago") -r

complete -c find -o empty -d (N_ "File is empty and is either a regular file or a directory") 
complete -c find -o false -d (N_ "Always false") 
complete -c find -o fstype -d (N_ "File is on filesystem of specified type") -a "(__fish_print_filesystems)" -r
complete -c find -o gid -d (N_ "Numeric group id of file") -r
complete -c find -o group -d (N_ "Group name of file") -a "(__fish_complete_groups)"

complete -c find -o ilname -d (N_ "File is symlink matching specified case insensitive pattern") -r
complete -c find -o iname -d (N_ "File name matches case insensitive pattern") -r
complete -c find -o inum -d (N_ "File has specified inode number") -r
complete -c find -o ipath -o iwholename -d (N_ "File path matches case insensitive pattern") -r
complete -c find -o iregex -d (N_ "File name matches case insensetive regex") -r

complete -c find -o links -d (N_ "File has specified number of links") -r -a "1 2 3 4 5 6 7 8 9"
complete -c find -o lname -d (N_ "File is symlink matching specified pattern") -r

complete -c find -o mmin -d (N_ "File last modified specified number of minutes ago") -r
complete -c find -o newer -d (N_ "File last modified more recently than file was modified") -r
complete -c find -o mtime -d (N_ "File last modified specified number of days ago") -r

complete -c find -o name -d (N_ "File name matches pattern") -r

complete -c find -o nouser -d (N_ "No user corresponds to file's numeric user ID")
complete -c find -o nogroup -d (N_ "No group corresponds to file's numeric group ID")

complete -c find -o path -o wholename -d (N_ "File path matches pattern") -r

complete -c find -o perm -d (N_ "Files has specified permissions set") -r

complete -c find -o iregex -d (N_ "File name matches regex") -r

complete -c find -o samefile -d (N_ "File refers to the same inode as specified file") -r
complete -c find -o size -d (N_ "File uses specified units of space") -r
complete -c find -o true -d (N_ "Always true")

set -l type_comp 'b\t"Block device" c\t"Character device" d\t"Directory" p\t"Named pipe" f\t"File" l\t"Symbolic link" s\t"Socket"'

complete -c find -o type -d (N_ "File is of specified type") -x -a  $type_comp

complete -c find -o uid -d (N_ "File's owner has specified numeric user ID") -r
complete -c find -o used -d (N_ "File was last accessed specified number of days after its status was last changed") -r
complete -c find -o user -d (N_ "File's owner") -a "(__fish_complete_users)" -r
complete -c find -o xtype -d (N_ "Check type of file - in case of symlink, check the file that is not checked by -type") -x -a $type_comp
complete -c find -o context -d (N_ "File's security context matches specified pattern") -r


# Actions

complete -c find -o delete -d (N_ "Delete selected files")
complete -c find -o exec -d (N_ "Execute specified command for each located file") -r
complete -c find -o execdir -d (N_ "Execute specified command for each located file, in the files directory") -r
complete -c find -o fls -d (N_ "List file in ls -dils format, write to specified file") -r
complete -c find -o fprint -d (N_ "Print full file names into specified file") -r
complete -c find -o fprint0 -d (N_ "Print null separated full file names into specified file") -r
complete -c find -o fprintf -d (N_ "Print formated data into specified file") -r
complete -c find -o ok -d (N_ "Execute specified command for each located file after asking user") -r
complete -c find -o print -d (N_ "Print full file names")
complete -c find -o okdir -d (N_ "Execute specified command for each located file, in the files directory, after asking user") -r
complete -c find -o print0 -d (N_ "Print null separated full file names")
complete -c find -o printf -d (N_ "Print formated data") -r
complete -c find -o prune -d (N_ "Do not recurse unless -depth is specified")
complete -c find -o quit -d (N_ "Exit at once")
complete -c find -o ls -d (N_ "List file in ls -dils format") -r

# Grouping 

complete -c find -o not -d (N_ "Negate result of action")
complete -c find -o and -s a -d (N_ "Result is only true if both previous and next action are true")
complete -c find -o or -s o -d (N_ "Result is true if either previous or next action are true")





