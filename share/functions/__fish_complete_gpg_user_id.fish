# Helper function for contextual autocompletion of gpg user ids

function __fish_complete_gpg_user_id -d "Complete using gpg user ids" -a __fish_complete_gpg_command
    # gpg doesn't seem to like it when you use the whole key name as a
    # completion, so we skip the <EMAIL> part and use it as a description.
    # It also replaces \x3a from gpg's output with colons.
    #
    # TODO: I tried with <EMAIL> and it worked, this was possibly fixed in gpg.
    # Regardless, it's probably nicer as a description.
    $__fish_complete_gpg_command --list-keys --with-colon | string split -a -f 10 : | string replace '\x3a' : | string replace -r '(.*) <(.*)>' '$1\t$2'
end
