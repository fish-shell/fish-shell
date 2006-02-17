#apt-config
complete -c apt-config -s h -l help -d (_ "Display help and exit")
complete -c apt-config -a shell -d (_ "Access config file from shell")
complete -f -c apt-config -a dump -d (_ "Dump contents of config file")
complete -f -c apt-config -s v -l version -d (_ "Display version and exit")
complete -r -c apt-config -s c -l config-file -d (_ "Specify config file")
complete -x -c apt-config -s o -l option -d (_ "Specify options")
