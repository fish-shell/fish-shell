
# magic completion safety check (do not remove this comment)
if not type -q sha256sum
    exit
end
complete -c sha256sum -w md5sum
