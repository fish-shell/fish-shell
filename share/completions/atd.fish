#atd

# magic completion safety check (do not remove this comment)
if not type -q atd
    exit
end
complete -f -c atd -s l -d "Limiting load factor"
complete -f -c atd -s b -d "Minimum interval in seconds"
complete -f -c atd -s d -d "Debug mode"
complete -f -c atd -s s -d "Process at queue only once"

