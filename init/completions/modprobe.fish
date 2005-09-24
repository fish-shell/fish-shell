#
# Completions for the modprobe command
#

complete -c modprobe -d Module -a "(/sbin/modprobe -l | sed -re 's/\/.*\/([^\/.]*).*/\1/')"
complete -c modprobe -s v -l verbose -d "Print messages about what the program is doing"
complete -c modprobe -s C -l config -d "Configuration file" -r
complete -c modprobe -s c -l showconfig -d "Dump configuration file"
complete -c modprobe -s n -l dry-run -d "Do not actually insert/remove module"
complete -c modprobe -s i -l ingnore-install -d "Ignore install and remove commands in configuration file"
complete -c modprobe -l ingnore-remove -d "Ignore install and remove commands in configuration file"
complete -c modprobe -s q -l quiet -d "Ignore bogus module names"
complete -c modprobe -s r -l remove -d "Remove modules"
complete -c modprobe -s V -l version -d "Display version and exit"
complete -c modprobe -s f -l force -d "Ignore all version information"
complete -c modprobe -l force-vermagic -d "Ignore version magic information"
complete -c modprobe -l force-modversion -d "Ignore module interface version"
complete -c modprobe -s l -l list -d "List all modules matching the given wildcard" 
complete -c modprobe -s a -l all -d "Insert modules matching the given wildcard"
complete -c modprobe -s t -l type -d "Restrict wildcards to specified directory"
complete -c modprobe -s s -l syslog -d "Send error messages through syslog"
complete -c modprobe -l set-version -d "Specify kernel version"
complete -c modprobe -l show-depends -d "List dependencies of module"
complete -c modprobe -s o -l name -d "Rename module"
complete -c modprobe -l first-time -d "Fail if inserting already loaded module"

