#apt-zip-list
complete -c apt-zip-list -s h -l help -d (N_ "Display help and exit")
complete -f -c apt-zip-list -s V -l version -d (N_ "Display version and exit")
complete -c apt-zip-list -s m -l medium -d (N_ "Removable medium")
complete -f -c apt-zip-list -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d (N_ "Select an action")
complete -c apt-zip-list -s p -l packages -d (N_ "List of packages to install")
complete -f -c apt-zip-list -s f -l fix-broken -d (N_ "Fix broken option")
complete -c apt-zip-list -l skip-mount -d (N_ "Specify a non-mountpoint dir")
complete -c apt-zip-list -s M -l method -d (N_ "Select a method")
complete -c apt-zip-list -s o -l options -a "tar restart" -d (N_ "Specify options")
complete -c apt-zip-list -s A -l accept -a "http ftp" -d (N_ "Accept protocols")
complete -c apt-zip-list -s R -l reject -a "http ftp" -d (N_ "Reject protocols")

