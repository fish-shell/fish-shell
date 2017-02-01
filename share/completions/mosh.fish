
complete -x -c mosh -d Hostname -a "

(__fish_print_hostnames)

(
        #Prepend any username specified in the completion to the hostname
        echo (commandline -ct)|sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames)
"

complete -x -c mosh -d User -a "
(__fish_print_users)@
"
complete -c mosh --description "Command to run" -x -a '(__fish_complete_subcommand --fcs-skip=2)'

complete -c mosh -l client --description 'Path to client helper on local machine (default: "mosh-client")'
complete -c mosh -l server --description 'Command to run server helper on remote machine (default: "mosh-server")'
complete -c mosh -l ssh --description 'SSH command to run when setting up session (example: "ssh -p 2222") (default: "ssh")'
complete -c mosh -f -l predict --description 'Controls use of speculative local echo' -a 'adaptive always never experimental'
complete -c mosh -s a --description 'Synonym for --predict=always'
complete -c mosh -s n --description 'Synonym for --predict=never'
complete -c mosh -s p -l port --description 'Use a particular server-side UDP port or port range'
