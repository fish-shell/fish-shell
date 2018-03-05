
# magic completion safety check (do not remove this comment)
if not type -q prevd
    exit
end
complete -c prevd -s l -d "Also print directory history"
