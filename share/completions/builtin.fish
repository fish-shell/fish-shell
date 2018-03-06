
# magic completion safety check (do not remove this comment)
if not type -q builtin
    exit
end

complete -c builtin -s h -l help -d 'Display help and exit'
complete -c builtin -s n -l names -d 'Print names of all existing builtins'
complete -c builtin -xa '(builtin -n)'
complete -c builtin -n '__fish_use_subcommand' -xa '__fish_complete_subcommand'
