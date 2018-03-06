
# magic completion safety check (do not remove this comment)
if not type -q mupdf
    exit
end
complete -c mupdf -x -a "(__fish_complete_suffix .pdf)"

