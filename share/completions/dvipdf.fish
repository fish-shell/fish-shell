
# magic completion safety check (do not remove this comment)
if not type -q dvipdf
    exit
end
complete -c dvipdf -x -a "(
           __fish_complete_suffix .dvi
   )"
