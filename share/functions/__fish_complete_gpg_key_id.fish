# Helper function for contextual autocompletion of GPG key ids

function __fish_complete_gpg_key_id -d 'Complete using gpg key ids' -a __fish_complete_gpg_command list_arg
    # Use user id as description
    set -l keyid
    set -q list_arg[1]; or set list_arg --list-keys
    $__fish_complete_gpg_command $list_arg --with-colons | while read -l garbage
        switch $garbage
            case "uid*"
                # Extract user ids (note: gpg escapes colons as '\x3a')
                set -l __uid (string split ":" -- $garbage)
                set -l uid (string replace -a '\x3a' ':' -- $__uid[10])
                printf "%s\t%s\n" $keyid $uid
                # NOTE key is preceded by "sec" instead of "pub" when listing secret keys
            case "pub*" "sec*"
                # Extract key fingerprints (no subkeys)
                set -l __pub (string split ":" -- $garbage)
                set keyid $__pub[5]
        end
    end
end
