set command termux-microphone-record

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -s d \
    -d Record

complete -c $command \
    -s f \
    -d 'Specify a file to save recording to' \
    -F -r

complete -c $command \
    -a '0\tdefault' \
    -s l \
    -d 'Specify the length limit of a recording' \
    -x

complete -c $command \
    -a 'aac amr_wb amr_nb' \
    -s e \
    -d 'Specify the encoder of a recording' \
    -x

complete -c $command \
    -s b \
    -d 'Specify the bitrate of a recording' \
    -x

complete -c $command \
    -s r \
    -d 'Specify the sampling rate of a recording' \
    -x

complete -c $command \
    -s c \
    -d 'Specify the channel count of a recording' \
    -x

complete -c $command \
    -s i \
    -d 'Show information about the current recording'

complete -c $command \
    -s q \
    -d 'Quit the current recording'
