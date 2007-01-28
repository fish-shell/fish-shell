#completion for sylpheed

complete -c sylpheed -l exit --description 'Exit sylpheed'
complete -c sylpheed -l debug --description 'Debug mode'
complete -c sylpheed -l help --description 'Display help and exit'
complete -c sylpheed -l version --description 'Output version information and exit'

complete -r -c sylpheed -l compose --description 'Open composition window with address'
complete -r -c sylpheed -l attach --description 'Open composition window with attached files'
complete -r -c sylpheed -l receive --description 'Receive new messages'
complete -r -c sylpheed -l receive-all --description 'Receive new messages of all accounts'
complete -r -c sylpheed -l send --description 'Send all queued messages'
complete -r -c sylpheed -l status --description 'Show the total number of messages for folder'
complete -r -c sylpheed -l status-full --description 'Show the total number of messages for each folder'
complete -r -c sylpheed -l configdir --description 'Specify directory with configuration files'

