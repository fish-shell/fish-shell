
# magic completion safety check (do not remove this comment)
if not type -q sha1sum
    exit
end
complete -c sha1sum -w md5sum
