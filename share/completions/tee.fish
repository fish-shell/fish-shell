
# magic completion safety check (do not remove this comment)
if not type -q tee
    exit
end
complete -c tee -s a -l append              -d 'append to the given FILEs, do not overwrite'
complete -c tee -s i -l ignore-interrupts   -d 'ignore interrupt signals'
complete -c tee -l help     -d 'display this help and exit'
complete -c tee -l version  -d 'output version information and exit'
