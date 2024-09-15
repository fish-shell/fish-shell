set command termux-download

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s d \
    -d 'Specify a [d]escription for a download notification' \
    -x

complete -c $command \
    -s t \
    -d 'Specify a [t]itle for a download notification' \
    -x

complete -c $command \
    -s p \
    -d 'Specify a [p]ath for a download' \
    -F -r
