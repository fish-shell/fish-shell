
# magic completion safety check (do not remove this comment)
if not type -q djview
    exit
end

complete -c djview -x -a "(__fish_complete_suffix .djvu)"

