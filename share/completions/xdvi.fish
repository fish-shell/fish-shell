
# magic completion safety check (do not remove this comment)
if not type -q xdvi
    exit
end
complete -c xdvi -x -a "(__fish_complete_suffix .dvi)"
