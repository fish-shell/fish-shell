
# magic completion safety check (do not remove this comment)
if not type -q or
    exit
end

complete -c or -s h -l help -d 'Display help and exit'
complete -c or -xa '(__fish_complete_subcommand)'
