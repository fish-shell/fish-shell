set command termux-sms-list

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -s d \
    -d 'Show the creation dates of listed messages'

complete -c $command \
    -a '10\tdefault' \
    -s l \
    -d 'Limit the amount of listed messages' \
    -x

complete -c $command \
    -s n \
    -d 'Show the phone numbers of listed messages'

complete -c $command \
    -a '0\tdefault' \
    -s o \
    -d 'Start listing calls with the specificed one' \
    -x

complete -c $command \
    -a 'inbox\tdefault all sent draft outbox' \
    -s t \
    -d 'Filter listed messages by the type' \
    -x
