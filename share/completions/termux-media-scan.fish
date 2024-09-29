set command termux-media-scan

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -s r \
    -d 'Scan directories recursively'

complete -c $command \
    -s v \
    -d 'Show verbose messages'
