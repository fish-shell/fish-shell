
# magic completion safety check (do not remove this comment)
if not type -q eval
    exit
end

complete -c eval -s h -l help -d 'Display help and exit'
