#
# Completions for the modprobe command
#

complete -c modprobe -n "__fish_contains_opt -s r remove" --no-files -d Module -a "(lsmod | cut -d' ' -f1)"
complete -c modprobe -n "not __fish_contains_opt -s r remove" --no-files -d Module -a "(find /lib/modules/(uname -r)/{kernel,misc} -type f 2>/dev/null | sed -e 's/\/.*\/\([^\/.]*\).*/\1/')"
complete -c modprobe -s v -l verbose --description "Print messages about what the program is doing"
complete -c modprobe -s C -l config --description "Configuration file" -r
complete -c modprobe -s c -l showconfig --description "Dump configuration file"
complete -c modprobe -s n -l dry-run --description "Do not actually insert/remove module"
complete -c modprobe -s i -l ignore-install --description "Ignore install and remove commands in configuration file"
complete -c modprobe -l ignore-remove --description "Ignore install and remove commands in configuration file"
complete -c modprobe -s q -l quiet --description "Ignore bogus module names"
complete -c modprobe -s r -l remove --description "Remove modules"
complete -c modprobe -s V -l version --description "Display version and exit"
complete -c modprobe -s f -l force --description "Ignore all version information"
complete -c modprobe -l force-vermagic --description "Ignore version magic information"
complete -c modprobe -l force-modversion --description "Ignore module interface version"
complete -c modprobe -s l -l list --description "List all modules matching the given wildcard"
complete -c modprobe -s a -l all --description "Insert modules matching the given wildcard"
complete -c modprobe -s t -l type --description "Restrict wildcards to specified directory"
complete -c modprobe -s s -l syslog --description "Send error messages through syslog"
complete -c modprobe -l set-version --description "Specify kernel version"
complete -c modprobe -l show-depends --description "List dependencies of module"
complete -c modprobe -s o -l name --description "Rename module"
complete -c modprobe -l first-time --description "Fail if inserting already loaded module"

