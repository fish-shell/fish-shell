
# magic completion safety check (do not remove this comment)
if not type -q pacmatic
    exit
end
complete -c pacmatic -w pacman
