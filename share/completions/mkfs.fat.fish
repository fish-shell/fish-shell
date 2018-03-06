
# magic completion safety check (do not remove this comment)
if not type -q mkfs.fat
    exit
end
complete -c mkfs.fat -w mkdosfs
