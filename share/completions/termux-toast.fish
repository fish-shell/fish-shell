set command termux-toast

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -a '(__fish_termux_api__complete_colors)' \
    -s b \
    -d 'Specify the background color of a message' \
    -x

complete -c $command \
    -a '(__fish_termux_api__complete_colors)' \
    -s c \
    -d 'Specify the text color of a message' \
    -x

complete -c $command \
    -a 'middle\tdefault top bottom' \
    -s g \
    -d 'Specify the position of a message' \
    -x

complete -c $command \
    -s s \
    -d 'Show a message for the short period of time'
