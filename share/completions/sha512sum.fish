
# magic completion safety check (do not remove this comment)
if not type -q sha512sum
    exit
end
complete -c sha512sum -w md5sum
