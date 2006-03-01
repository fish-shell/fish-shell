#apt-key
complete -r -c apt-key -a add -d (N_ "Add a new key")
complete -f -c apt-key -a del -d (N_ "Remove a key")
complete -f -c apt-key -a list -d (N_ "List trusted keys")

