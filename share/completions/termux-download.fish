set -l command termux-download

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s d -x \
    -d 'Specify the description for a download notification'

complete -c $command -s t -x -d 'Specify the title for a download notification'
complete -c $command -s p -F -r -d 'Specify the path for a download'
