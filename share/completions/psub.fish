
# magic completion safety check (do not remove this comment)
if not type -q psub
    exit
end
complete -c psub -s h -l help -d "Display help and exit"
complete -c psub -s f -l file -d "Communicate using a regular file, not a named pipe"
