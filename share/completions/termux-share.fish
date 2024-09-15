set command termux-share

complete -c $command -f

complete -c $command \
    -s h \
    -l help \
    -d 'Show [h]elp'

complete -c $command \
    -a 'view\tdefault edit send' \
    -s a \
    -d 'Specify the [a]ction to perform on a content' \
    -x

complete -c $command \
    -a 'text/plain\tdefault' \
    -s c \
    -d 'Specify the type of a [c]ontent'

complete -c $command \
    -s d \
    -d 'Specify the receiver of a content'

complete -c $command \
    -s t \
    -d 'Specify the [t]itle of a content' \
    -x
