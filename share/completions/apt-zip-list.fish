#apt-zip-list

# magic completion safety check (do not remove this comment)
if not type -q apt-zip-list
    exit
end
complete -c apt-zip-list -s h -l help -d "Display help and exit"
complete -f -c apt-zip-list -s V -l version -d "Display version and exit"
complete -c apt-zip-list -s m -l medium -d "Removable medium"
complete -f -c apt-zip-list -s a -l aptgetaction -a "dselect-upgrade upgrade dist-upgrade" -d "Select an action"
complete -c apt-zip-list -s p -l packages -d "List of packages to install"
complete -f -c apt-zip-list -s f -l fix-broken -d "Fix broken option"
complete -c apt-zip-list -l skip-mount -d "Specify a non-mountpoint dir"
complete -c apt-zip-list -s M -l method -d "Select a method"
complete -c apt-zip-list -s o -l options -a "tar restart" -d "Specify options"
complete -c apt-zip-list -s A -l accept -a "http ftp" -d "Accept protocols"
complete -c apt-zip-list -s R -l reject -a "http ftp" -d "Reject protocols"

