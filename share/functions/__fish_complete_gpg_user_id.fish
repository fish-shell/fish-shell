# Helper function for contextual autocompletion of gpg user ids

function __fish_complete_gpg_user_id -d "Complete using gpg user ids" -a __fish_complete_gpg_command
    # gpg doesn't seem to like it when you use the whole key name as a
    # completion, so we skip the <EMAIL> part and use it as a description.
    # It also replaces \x3a from gpg's output with colons.
    $__fish_complete_gpg_command --list-keys --with-colon | string split -a -f 10 : | string replace '\x3a' : | string replace -rf '(.*) <(.*)>' '$1\t$2'
end
