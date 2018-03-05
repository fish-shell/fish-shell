
# magic completion safety check (do not remove this comment)
if not type -q la
    exit
end
complete -c la -w ls
