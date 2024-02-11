# Load completions shared by various ssh tools like ssh, scp and sftp.
__fish_complete_ssh ssh

#
# ssh specific completions
#

complete -c ssh -d Remote -xa "(__fish_complete_user_at_hosts)"
complete -c ssh -d Remote -k -fa '(__ssh_history_completions)'

complete -c ssh -n 'test (__fish_number_of_cmd_args_wo_opts) -ge 2' -d "Command to run" -xa '(__fish_complete_subcommand --fcs-skip=2)'

complete -c ssh -s a -d "Disables forwarding of the authentication agent"
complete -c ssh -s B -d "Bind to the address of that interface" -xa '(__fish_print_interfaces)'
complete -c ssh -s b -d "Local address to bind to" -xa '(__fish_print_addresses)'
complete -c ssh -s D -d "Specify dynamic port forwarding" -x
complete -c ssh -s E -d "Append debug logs to log_file" -rF
complete -c ssh -s e -d "Escape character" -xa "\^ none"
complete -c ssh -s f -d "Go to background"
complete -c ssh -s G -d "Print configuration after evaluating Host"
complete -c ssh -s g -d "Allow remote host to connect to local forwarded ports"
complete -c ssh -s I -d "Specify the PKCS#11 library" -r
complete -c ssh -s K -d "Enables GSSAPI-based authentication"
complete -c ssh -s k -d "Disables forwarding of GSSAPI credentials"
complete -c ssh -s L -d "Specify local port forwarding" -x
complete -c ssh -s l -x -a "(__fish_complete_users)" -d User
complete -c ssh -s M -d "Places the ssh client into master mode"
complete -c ssh -s m -d "MAC algorithm" -xa "(__fish_complete_list , __fish_ssh_macs)"
complete -c ssh -s N -d "Do not execute remote command"
complete -c ssh -s n -d "Prevent reading from stdin"
complete -c ssh -s O -d "Control an active connection multiplexing master process" -x
complete -c ssh -s p -d Port -x
complete -c ssh -s Q -d "List supported algorithms" -xa "(ssh -Q help)"
complete -c ssh -s R -d "Specify remote/reverse port forwarding" -x
complete -c ssh -s S -d "Location of a control socket for connection sharing" -r
complete -c ssh -s s -d Subsystem
complete -c ssh -s T -d "Disable pseudo-tty allocation"
complete -c ssh -s t -d "Force pseudo-tty allocation"
complete -c ssh -s V -d "Display version number"
complete -c ssh -s W -d "Forward stdin/stdout to host:port over secure channel" -x
complete -c ssh -s w -d "Requests tunnel device forwarding" -x
complete -c ssh -s X -d "Enable X11 forwarding"
complete -c ssh -s x -d "Disable X11 forwarding"
complete -c ssh -s Y -d "Enables trusted X11 forwarding"
complete -c ssh -s y -d "Send log information using syslog"
