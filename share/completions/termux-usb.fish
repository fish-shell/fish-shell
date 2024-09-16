set command termux-usb

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s l \
    -d '[l]ist all devices'

complete -c $command \
    -s r \
    -d 'Show the permission [r]equest dialog for a device'

complete -c $command \
    -s e \
    -d '[e]xecute the specified command for a device' \
    -x
    