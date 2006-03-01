#apt-zip-inst
complete -c apt-zip-inst -s h -l help -d (N_ "Display help and exit")
complete -f -c apt-zip-inst -s V -l version -d (N_ "Display version and exit")
complete -c apt-zip-inst -s m -l medium -d (N_ "Removable medium")
complete -f -c apt-zip-inst -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d (N_ "Select an action")
complete -c apt-zip-inst -s p -l packages -d (N_ "List of packages to install")
complete -f -c apt-zip-inst -s f -l fix-broken -d (N_ "Fix broken option")
complete -c apt-zip-inst -l skip-mount -d (N_ "Specify a non-mountpoint dir")


