#apt-zip-inst
complete -c apt-zip-inst -s h -l help --description "Display help and exit"
complete -f -c apt-zip-inst -s V -l version --description "Display version and exit"
complete -c apt-zip-inst -s m -l medium --description "Removable medium"
complete -f -c apt-zip-inst -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" --description "Select an action"
complete -c apt-zip-inst -s p -l packages --description "List of packages to install"
complete -f -c apt-zip-inst -s f -l fix-broken --description "Fix broken option"
complete -c apt-zip-inst -l skip-mount --description "Specify a non-mountpoint dir"


