set command termux-sms-list

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '(__fish_complete_list , __fish_termux_api__complete_phone_numbers)' \
    -s n \
    -d 'Specify the recipient [n]umbers of a message' \
    -x

complete -c $command \
    -s s \
    -d 'Specify the sim [s]lot to use for a message' \
    -x
