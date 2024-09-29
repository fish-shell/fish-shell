set -l command termux-infrared-transmit

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s f -x -d 'Specify the IR carrier frequency'
