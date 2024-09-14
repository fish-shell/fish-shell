set command termux-media-scan

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s r \
    -d 'Scan directories [r]ecursively'

complete -c $command \
    -s v \
    -d 'Show [v]erbose messages'
