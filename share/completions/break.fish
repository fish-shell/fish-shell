
# magic completion safety check (do not remove this comment)
if not type -q break
    exit
end

complete -c break -s h -l help -d 'Display help and exit'
