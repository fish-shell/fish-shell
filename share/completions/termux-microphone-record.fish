set -l command termux-microphone-record

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s d -d Record
complete -c $command -s f -F -r -d 'Specify a file to save recording to'

complete -c $command -s l -x \
    -a '0\tdefault' \
    -d 'Specify the length limit of a recording'

complete -c $command -s e -x \
    -a 'aac amr_wb amr_nb' \
    -d 'Specify the encoder of a recording'

complete -c $command -s b -x -d 'Specify the bitrate of a recording'
complete -c $command -s r -x -d 'Specify the sampling rate of a recording'
complete -c $command -s c -x -d 'Specify the channel count of a recording'
complete -c $command -s i -d 'Show information about the current recording'
complete -c $command -s q -d 'Quit the current recording'
