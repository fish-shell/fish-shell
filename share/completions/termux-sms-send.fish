set -l command termux-sms-list

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s n -x \
    -a '(__fish_complete_list , __fish_termux_api__complete_phone_numbers)' \
    -d 'Specify the recipient numbers of a message'

complete -c $command -s s -x -d 'Specify the sim slot to use for a message'
