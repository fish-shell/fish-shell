#apt-key

# magic completion safety check (do not remove this comment)
if not type -q apt-key
    exit
end
complete -r -c apt-key -a add -d "Add a new key"
complete -f -c apt-key -a del -d "Remove a key"
complete -f -c apt-key -a list -d "List trusted keys"

