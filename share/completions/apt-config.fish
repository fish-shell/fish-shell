#apt-config
complete -c apt-config -s h -l help --description "Display help and exit"
complete -c apt-config -a shell --description "Access config file from shell"
complete -f -c apt-config -a dump --description "Dump contents of config file"
complete -f -c apt-config -s v -l version --description "Display version and exit"
complete -r -c apt-config -s c -l config-file --description "Specify config file"
complete -x -c apt-config -s o -l option --description "Specify options"
