#apt-key
complete -r -c apt-key -a add --description "Add a new key"
complete -f -c apt-key -a del --description "Remove a key"
complete -f -c apt-key -a list --description "List trusted keys"

