
# magic completion safety check (do not remove this comment)
if not type -q sha384sum
    exit
end
complete -c sha384sum -w md5sum
