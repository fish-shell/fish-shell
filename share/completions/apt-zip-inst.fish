#apt-zip-inst
complete -c apt-zip-inst -s h -l help -d "Display help and exit"
complete -f -c apt-zip-inst -s V -l version -d "Display version and exit"
complete -c apt-zip-inst -s m -l medium -d "Removable medium"
complete -f -c apt-zip-inst -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d "Select an action"
complete -c apt-zip-inst -s p -l packages -d "List of packages to install"
complete -f -c apt-zip-inst -s f -l fix-broken -d "Fix broken option"
complete -c apt-zip-inst -l skip-mount -d "Specify a non-mountpoint dir"
