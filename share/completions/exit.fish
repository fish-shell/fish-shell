
# magic completion safety check (do not remove this comment)
if not type -q exit
    exit
end

complete -c exit -s h -l help -d 'Display help and exit'
complete -c exit -x -a 0 -d "Quit with normal exit status"
complete -c exit -x -a 1 -d "Quit with abnormal exit status"
