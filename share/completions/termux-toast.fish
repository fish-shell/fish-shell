set command termux-toast

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '(__fish_termux_api__complete_colors)' \
    -s b \
    -d 'Specify the [b]ackground color of a message' \
    -x

complete -c $command \
    -a '(__fish_termux_api__complete_colors)' \
    -s c \
    -d 'Specify the text [c]olor of a message' \
    -x

complete -c $command \
    -a 'middle\tdefault top bottom' \
    -s g \
    -d 'Specify the position of a message' \
    -x

complete -c $command \
    -s s \
    -d 'Show a message for the [s]hort period of time'
