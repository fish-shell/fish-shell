#
# Completions for the modprobe command
#

complete -c modprobe -d Module -a "(/sbin/modprobe -l | sed -e 's/\/.*\/\([^\/.]*\).*/\1/')"
complete -c modprobe -s v -l verbose -d (_ "Print messages about what the program is doing")
complete -c modprobe -s C -l config -d (_ "Configuration file") -r
complete -c modprobe -s c -l showconfig -d (_ "Dump configuration file")
complete -c modprobe -s n -l dry-run -d (_ "Do not actually insert/remove module")
complete -c modprobe -s i -l ingnore-install -d (_ "Ignore install and remove commands in configuration file")
complete -c modprobe -l ingnore-remove -d (_ "Ignore install and remove commands in configuration file")
complete -c modprobe -s q -l quiet -d (_ "Ignore bogus module names")
complete -c modprobe -s r -l remove -d (_ "Remove modules")
complete -c modprobe -s V -l version -d (_ "Display version and exit")
complete -c modprobe -s f -l force -d (_ "Ignore all version information")
complete -c modprobe -l force-vermagic -d (_ "Ignore version magic information")
complete -c modprobe -l force-modversion -d (_ "Ignore module interface version")
complete -c modprobe -s l -l list -d (_ "List all modules matching the given wildcard") 
complete -c modprobe -s a -l all -d (_ "Insert modules matching the given wildcard")
complete -c modprobe -s t -l type -d (_ "Restrict wildcards to specified directory")
complete -c modprobe -s s -l syslog -d (_ "Send error messages through syslog")
complete -c modprobe -l set-version -d (_ "Specify kernel version")
complete -c modprobe -l show-depends -d (_ "List dependencies of module")
complete -c modprobe -s o -l name -d (_ "Rename module")
complete -c modprobe -l first-time -d (_ "Fail if inserting already loaded module")

