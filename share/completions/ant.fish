
# magic completion safety check (do not remove this comment)
if not type -q ant
    exit
end

complete -x -c ant -a "(__fish_complete_ant_targets)"

