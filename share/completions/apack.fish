
# magic completion safety check (do not remove this comment)
if not type -q apack
    exit
end
complete -c apack -w atool
