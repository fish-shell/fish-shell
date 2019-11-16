# Helper function for contextual autocompletion of GPG key ids

function __fish_complete_gpg_key_id -d 'Complete using gpg key ids' -a __fish_complete_gpg_command
    # Use user id as description
    set -l keyid
    $__fish_complete_gpg_command --list-keys --with-colons | while read garbage
        switch $garbage
            # Extract user ids
            case "uid*"
                echo $garbage | cut -d ":" -f 10 | sed -e "s/\\\x3a/:/g" | read uid
                printf "%s\t%s\n" $keyid $uid
                # Extract key fingerprints (no subkeys)
            case "pub*"
                echo $garbage | cut -d ":" -f 5 | read keyid
        end
    end
end
