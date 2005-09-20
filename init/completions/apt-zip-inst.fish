#apt-zip-inst
complete -c apt-zip-inst -s h -l help -d "apt-zip-inst command help"
complete -f -c apt-zip-inst -s V -l version -d "show version"
complete -c apt-zip-inst -s m -l medium -d "removable medium"
complete -f -c apt-zip-inst -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d "select an action"
complete -c apt-zip-inst -s p -l packages -d "list of pkgs to install"
complete -f -c apt-zip-inst -s f -l fix-broken -d "fix broken option"
complete -c apt-zip-inst -l skip-mount -d "specify a non-mountpoint dir"


