
# magic completion safety check (do not remove this comment)
if not type -q and
    exit
end
complete -c and -s h -l help -d 'Display help and exit'
complete -c and -xa '( __fish_complete_subcommand )'

