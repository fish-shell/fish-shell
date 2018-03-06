
# magic completion safety check (do not remove this comment)
if not type -q nextd
    exit
end
complete -c nextd -s l -d "Also print directory history"
