
# magic completion safety check (do not remove this comment)
if not type -q arepack
    exit
end
complete -c arepack -w atool
