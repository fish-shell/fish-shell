#apt-key
complete -r -c apt-key -a add -d (_ "Add a new key")
complete -f -c apt-key -a del -d (_ "Remove a key")
complete -f -c apt-key -a list -d (_ "List trusted keys")

