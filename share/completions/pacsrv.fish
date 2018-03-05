
# magic completion safety check (do not remove this comment)
if not type -q pacsrv
    exit
end
complete -c pacsrc -w pacman
