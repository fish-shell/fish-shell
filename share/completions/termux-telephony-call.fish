set command termux-telephony-call

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -a '(__fish_termux_api__complete_phone_numbers)'
