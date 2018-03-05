
# magic completion safety check (do not remove this comment)
if not type -q egrep
    exit
end
complete -c grep -w egrep
