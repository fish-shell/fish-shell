
# magic completion safety check (do not remove this comment)
if not type -q whatis
    exit
end

complete -xc whatis -a "(__fish_complete_man)"
