
# magic completion safety check (do not remove this comment)
if not type -q return
    exit
end

complete -c return -s h -l help -d "Display help and exit"
complete -c return -x -a 0 -d "Return from function with normal exit status"
complete -c return -x -a 1 -d "Return from function with abnormal exit status"
