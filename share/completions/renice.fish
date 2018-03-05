
# magic completion safety check (do not remove this comment)
if not type -q renice
    exit
end

complete -c renice -s p -d "Force following parameters to be process ID's (The default)"
complete -c renice -s g -d "Force following parameters to be interpreted as process group ID's"
complete -c renice -s u -d "Force following parameters to be interpreted as user names"
