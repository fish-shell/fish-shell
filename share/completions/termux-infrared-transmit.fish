set command termux-infrared-transmit

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s f \
    -d 'Specify the IR carrier [f]requency' \
    -x
