set command termux-wallpaper

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -s f \
    -d 'Specify the file of a wallpaper' \
    -F -r

complete -c $command \
    -s u \
    -d 'Specify the uRL of a wallpaper' \
    -F -r

complete -c $command \
    -s l \
    -d 'Set a wallpaper for the lockscreen'
