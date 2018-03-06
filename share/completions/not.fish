
# magic completion safety check (do not remove this comment)
if not type -q not
    exit
end

complete -c not -s h -l help -d 'Display help and exit'
complete -c not -xa '__fish_complete_subcommand'

