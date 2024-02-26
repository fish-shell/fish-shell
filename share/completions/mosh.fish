complete -x -c mosh -d Hostname -a "(__fish_complete_user_at_hosts)"

complete -x -c mosh -d User -a "
(__fish_print_users)@
"
complete -c mosh -n 'test (__fish_number_of_cmd_args_wo_opts) -ge 2' -d "Command to run" -x -a '(__fish_complete_subcommand --fcs-skip=2)'

complete -c mosh -l client -d 'Path to client helper on local machine (default: "mosh-client")'
complete -c mosh -l server -d 'Command to run server helper on remote machine (default: "mosh-server")'
complete -c mosh -l ssh -d 'SSH command to run when setting up session (example: "ssh -p 2222") (default: "ssh")'
complete -c mosh -f -l predict -d 'Controls use of speculative local echo' -a 'adaptive always never experimental'
complete -c mosh -s a -d 'Synonym for --predict=always'
complete -c mosh -s n -d 'Synonym for --predict=never'
complete -c mosh -s p -l port -d 'Use a particular server-side UDP port or port range'
