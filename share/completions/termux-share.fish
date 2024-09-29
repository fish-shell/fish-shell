set -l command termux-share

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'

complete -c $command -s a -x \
    -a 'view\tdefault edit send' \
    -d 'Specify the action to perform on a content'

complete -c $command -s c \
    -a 'text/plain\tdefault' \
    -d 'Specify the type of a content'

complete -c $command -s d -d 'Specify the receiver of a content'
complete -c $command -s t -x -d 'Specify the title of a content'
    
