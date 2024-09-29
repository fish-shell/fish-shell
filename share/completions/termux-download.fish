set command termux-download

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -s d \
    -d 'Specify the description for a download notification' \
    -x

complete -c $command \
    -s t \
    -d 'Specify the title for a download notification' \
    -x

complete -c $command \
    -s p \
    -d 'Specify the path for a download' \
    -F -r
