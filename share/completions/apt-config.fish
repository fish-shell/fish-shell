#apt-config
complete -c apt-config -s h -l help -d (N_ "Display help and exit")
complete -c apt-config -a shell -d (N_ "Access config file from shell")
complete -f -c apt-config -a dump -d (N_ "Dump contents of config file")
complete -f -c apt-config -s v -l version -d (N_ "Display version and exit")
complete -r -c apt-config -s c -l config-file -d (N_ "Specify config file")
complete -x -c apt-config -s o -l option -d (N_ "Specify options")
