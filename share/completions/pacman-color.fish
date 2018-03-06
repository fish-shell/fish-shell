
# magic completion safety check (do not remove this comment)
if not type -q pacman-color
    exit
end
complete -c pacman-color -w pacman
