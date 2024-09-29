set command termux-share

complete -c $command -f

complete -c $command \
    -s h \
    -l help \
    -d 'Show help'

complete -c $command \
    -a 'view\tdefault edit send' \
    -s a \
    -d 'Specify the action to perform on a content' \
    -x

complete -c $command \
    -a 'text/plain\tdefault' \
    -s c \
    -d 'Specify the type of a content'

complete -c $command \
    -s d \
    -d 'Specify the receiver of a content'

complete -c $command \
    -s t \
    -d 'Specify the title of a content' \
    -x
