#completion for sylpheed

complete -c sylpheed -l exit -d 'Exit sylpheed'
complete -c sylpheed -l debug -d 'Debug mode'
complete -c sylpheed -l help -d 'Display help and exit'
complete -c sylpheed -l version -d 'Output version information and exit'

complete -r -c sylpheed -l compose -d 'Open composition window with address'
complete -r -c sylpheed -l attach -d 'Open composition window with attached files'
complete -r -c sylpheed -l receive -d 'Receive new messages'
complete -r -c sylpheed -l receive-all -d 'Receive new messages of all accounts'
complete -r -c sylpheed -l send -d 'Send all queued messages'
complete -r -c sylpheed -l status -d 'Show the total number of messages for folder'
complete -r -c sylpheed -l status-full -d 'Show the total number of messages for each folder'
complete -r -c sylpheed -l configdir -d 'Specify directory with configuration files'
