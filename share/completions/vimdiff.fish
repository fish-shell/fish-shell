
# magic completion safety check (do not remove this comment)
if not type -q vimdiff
    exit
end
complete -c vimdiff -w vim
