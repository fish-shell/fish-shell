
# magic completion safety check (do not remove this comment)
if not type -q mkfs.vfat
    exit
end
complete -c mkfs.vfat -w mkdosfs
