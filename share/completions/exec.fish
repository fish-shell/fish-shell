
# magic completion safety check (do not remove this comment)
if not type -q exec
    exit
end

complete -c exec -s h -l help -d 'Display help and exit'
