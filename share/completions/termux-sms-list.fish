set -l command termux-sms-list

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s d -d 'Show the creation dates of listed messages'

complete -c $command -s l -x \
    -a '10\tdefault' \
    -d 'Limit the amount of listed messages'

complete -c $command -s n -d 'Show the phone numbers of listed messages'

complete -c $command -s o -x \
    -a '0\tdefault' \
    -d 'Start listing calls with the specificed one'

complete -c $command -s t -x \
    -a 'inbox\tdefault all sent draft outbox' \
    -d 'Filter listed messages by the type'
