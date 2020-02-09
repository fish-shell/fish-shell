# Helper function for contextual autocompletion of GPG key ids

function __fish_complete_gpg_key_id -d 'Complete using gpg key ids' -a __fish_complete_gpg_command
    # Use user id as description
    set -l keyid
    $__fish_complete_gpg_command --list-keys --with-colons | while read garbage
        switch $garbage
            # Extract user ids (note: gpg escapes colons as '\x3a')
            case "uid*"
                set -l __uid (string split ":" -- $garbage)
                set uid (string replace -a '\x3a' ':' -- $__uuid[10])
                printf "%s\t%s\n" $keyid $uid
            # Extract key fingerprints (no subkeys)
            case "pub*"
                set -l __pub (string split ":" -- $garbage)
                set keyid $__pub[5]
        end
    end
end
