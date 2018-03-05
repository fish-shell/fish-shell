
# magic completion safety check (do not remove this comment)
if not type -q ps2pdf
    exit
end
complete -c ps2pdf -x -a "(
           __fish_complete_suffix .ps
   )"
