set command termux-microphone-record

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s d \
    -d Record

complete -c $command \
    -s f \
    -d 'Specify a [f]ile to save recording to' \
    -F -r

complete -c $command \
    -a '0\tdefault' \
    -s l \
    -d 'Specify a recording length [l]imit' \
    -x

complete -c $command \
    -a 'aac amr_wb amr_nb' \
    -s e \
    -d 'Specify a recording [e]ncoder' \
    -x

complete -c $command \
    -s b \
    -d 'Specify a recording [b]itrate' \
    -x

complete -c $command \
    -s r \
    -d 'Specify a recording sampling [r]ate' \
    -x

complete -c $command \
    -s c \
    -d 'Specify a recording [c]hannel count' \
    -x

complete -c $command \
    -s i \
    -d 'Show [i]nformation about the current recording'

complete -c $command \
    -s q \
    -d '[q]uit the current recording'
