#apt-zip-list
complete -c apt-zip-list -s h -l help -d (_ "Display help and exit")
complete -f -c apt-zip-list -s V -l version -d (_ "Display version and exit")
complete -c apt-zip-list -s m -l medium -d (_ "Removable medium")
complete -f -c apt-zip-list -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d (_ "Select an action")
complete -c apt-zip-list -s p -l packages -d (_ "List of packages to install")
complete -f -c apt-zip-list -s f -l fix-broken -d (_ "Fix broken option")
complete -c apt-zip-list -l skip-mount -d (_ "Specify a non-mountpoint dir")
complete -c apt-zip-list -s M -l method -d (_ "Select a method")
complete -c apt-zip-list -s o -l options -a "tar restart" -d (_ "Specify options")
complete -c apt-zip-list -s A -l accept -a "http ftp" -d (_ "Accept protocols")
complete -c apt-zip-list -s R -l reject -a "http ftp" -d (_ "Reject protocols")

