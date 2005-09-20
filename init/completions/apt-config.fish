#apt-config
complete -c apt-config -s h -l help -d "apt-config command help"
complete -c apt-config -a shell -d "access config file from shell"
complete -f -c apt-config -a dump -d "dump contents of config file"
complete -f -c apt-config -s v -l version -d "show version"
complete -r -c apt-config -s c -l config-file -d "specify config file"
complete -x -c apt-config -s o -l option -d "specify options"
