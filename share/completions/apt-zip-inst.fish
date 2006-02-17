#apt-zip-inst
complete -c apt-zip-inst -s h -l help -d (_ "Display help and exit")
complete -f -c apt-zip-inst -s V -l version -d (_ "Display version and exit")
complete -c apt-zip-inst -s m -l medium -d (_ "Removable medium")
complete -f -c apt-zip-inst -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d (_ "Select an action")
complete -c apt-zip-inst -s p -l packages -d (_ "List of packages to install")
complete -f -c apt-zip-inst -s f -l fix-broken -d (_ "Fix broken option")
complete -c apt-zip-inst -l skip-mount -d (_ "Specify a non-mountpoint dir")


