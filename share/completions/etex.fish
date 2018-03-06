
# magic completion safety check (do not remove this comment)
if not type -q etex
    exit
end
complete -c tex -w etex
