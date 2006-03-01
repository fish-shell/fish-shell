#
# Completions for the modprobe command
#

complete -c modprobe -d Module -a "(/sbin/modprobe -l | sed -e 's/\/.*\/\([^\/.]*\).*/\1/')"
complete -c modprobe -s v -l verbose -d (N_ "Print messages about what the program is doing")
complete -c modprobe -s C -l config -d (N_ "Configuration file") -r
complete -c modprobe -s c -l showconfig -d (N_ "Dump configuration file")
complete -c modprobe -s n -l dry-run -d (N_ "Do not actually insert/remove module")
complete -c modprobe -s i -l ingnore-install -d (N_ "Ignore install and remove commands in configuration file")
complete -c modprobe -l ingnore-remove -d (N_ "Ignore install and remove commands in configuration file")
complete -c modprobe -s q -l quiet -d (N_ "Ignore bogus module names")
complete -c modprobe -s r -l remove -d (N_ "Remove modules")
complete -c modprobe -s V -l version -d (N_ "Display version and exit")
complete -c modprobe -s f -l force -d (N_ "Ignore all version information")
complete -c modprobe -l force-vermagic -d (N_ "Ignore version magic information")
complete -c modprobe -l force-modversion -d (N_ "Ignore module interface version")
complete -c modprobe -s l -l list -d (N_ "List all modules matching the given wildcard") 
complete -c modprobe -s a -l all -d (N_ "Insert modules matching the given wildcard")
complete -c modprobe -s t -l type -d (N_ "Restrict wildcards to specified directory")
complete -c modprobe -s s -l syslog -d (N_ "Send error messages through syslog")
complete -c modprobe -l set-version -d (N_ "Specify kernel version")
complete -c modprobe -l show-depends -d (N_ "List dependencies of module")
complete -c modprobe -s o -l name -d (N_ "Rename module")
complete -c modprobe -l first-time -d (N_ "Fail if inserting already loaded module")

