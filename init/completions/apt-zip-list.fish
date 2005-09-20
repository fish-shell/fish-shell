#apt-zip-list
complete -c apt-zip-list -s h -l help -d "apt-zip-list command help"
complete -f -c apt-zip-list -s V -l version -d "show version"
complete -c apt-zip-list -s m -l medium -d "removable medium"
complete -f -c apt-zip-list -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d "select an action"
complete -c apt-zip-list -s p -l packages -d "list of pkgs to install"
complete -f -c apt-zip-list -s f -l fix-broken -d "fix broken option"
complete -c apt-zip-list -l skip-mount -d "specify a non-mountpoint dir"
complete -c apt-zip-list -s M -l method -d "select a method"
complete -c apt-zip-list -s o -l options -a "tar restart" -d "specify options"
complete -c apt-zip-list -s A -l accept -a "http ftp" -d "accept protocols"
complete -c apt-zip-list -s R -l reject -a "http ftp" -d "reject protocols"

