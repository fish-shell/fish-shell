
# magic completion safety check (do not remove this comment)
if not type -q gvimdiff
    exit
end
complete -c gvimdiff -w vim
