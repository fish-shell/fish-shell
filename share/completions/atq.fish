#atq

# magic completion safety check (do not remove this comment)
if not type -q atq
    exit
end
complete -f -c atq -s V -d "Display version and exit"
complete -f -c atq -s q -d "Use specified queue"

