set command termux-wallpaper

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s f \
    -d 'Specify the [f]ile of a wallpaper' \
    -F -r

complete -c $command \
    -s u \
    -d 'Specify the [u]RL of a wallpaper' \
    -F -r

complete -c $command \
    -s l \
    -d 'Set a wallpaper for the [l]ockscreen'
