
# magic completion safety check (do not remove this comment)
if not type -q ll
    exit
end
complete -c ll -w ls
