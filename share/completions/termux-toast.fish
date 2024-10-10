set -l command termux-toast

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s b -x \
    -a '(__fish_termux_api__complete_colors)' \
    -d 'Specify the background color of a message'

complete -c $command -s c -x \
    -a '(__fish_termux_api__complete_colors)' \
    -d 'Specify the text color of a message'

complete -c $command -s g -x \
    -a 'middle\tdefault top bottom' \
    -d 'Specify the position of a message'

complete -c $command -s s -d 'Show a message for the short period of time'
