
# magic completion safety check (do not remove this comment)
if not type -q dvipdfm
    exit
end
complete -c dvipdfm -x -a "
(
           __fish_complete_suffix .dvi
)"
