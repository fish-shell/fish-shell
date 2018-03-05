
# magic completion safety check (do not remove this comment)
if not type -q djview4
    exit
end

complete -c djview4 -x -a "(__fish_complete_suffix .djvu)"

