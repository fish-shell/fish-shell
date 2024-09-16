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
    -d 'Specify the length [l]imit of a recording' \
    -x

complete -c $command \
    -a 'aac amr_wb amr_nb' \
    -s e \
    -d 'Specify the [e]ncoder of a recording' \
    -x

complete -c $command \
    -s b \
    -d 'Specify the [b]itrate of a recording' \
    -x

complete -c $command \
    -s r \
    -d 'Specify the sampling [r]ate of a recording' \
    -x

complete -c $command \
    -s c \
    -d 'Specify the [c]hannel count of a recording' \
    -x

complete -c $command \
    -s i \
    -d 'Show [i]nformation about the current recording'

complete -c $command \
    -s q \
    -d '[q]uit the current recording'
