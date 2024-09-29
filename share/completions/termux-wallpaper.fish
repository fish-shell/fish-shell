set -l command termux-wallpaper

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s f -F -r -d 'Specify the file of a wallpaper'
complete -c $command -s u -F -r -d 'Specify the uRL of a wallpaper'
complete -c $command -s l -d 'Set a wallpaper for the lockscreen'
