# Helper function for contextual autocompletion of gpg user ids

function __fish_complete_gpg_user_id -d "Complete using gpg user ids" -a __fish_complete_gpg_command
    # gpg doesn't seem to like it when you use the whole key name as a
    # completion, so we skip the <EMAIL> part and use it as a
    # description.
    # It also replaces colons with \x3a
    $__fish_complete_gpg_command --list-keys --with-colon | cut -d : -f 10 | sed -ne 's/\\\x3a/:/g' -e 's/\(.*\) <\(.*\)>/\1'\t'\2/p'
end
