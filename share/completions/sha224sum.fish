
# magic completion safety check (do not remove this comment)
if not type -q sha224sum
    exit
end
complete -c sha224sum -w md5sum
