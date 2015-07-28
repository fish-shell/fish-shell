#
# Load common ssh options
#

__fish_complete_ssh ssh

complete -x -c ssh -d Hostname -a "

(__fish_print_hostnames)

(
	#Prepend any username specified in the completion to the hostname
	echo (commandline -ct)|sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames)
"

complete -x -c ssh -d User -a "
(__fish_print_users | __fish_sgrep -v '^_')@
"
complete -c ssh --description "Command to run" -x -a '(__fish_complete_subcommand --fcs-skip=2)'

complete -c ssh -s a --description "Disables forwarding of the authentication agent"
complete -c ssh -s A --description "Enables forwarding of the authentication agent"
complete -x -c ssh -s b --description "Interface to transmit from" -a "
(
	cat /proc/net/arp ^/dev/null| __fish_sgrep -v '^IP'|cut -d ' ' -f 1 ^/dev/null
)
"

complete -x -c ssh -s e --description "Escape character" -a "\^ none"
complete -c ssh -s f --description "Go to background"
complete -c ssh -s g --description "Allow remote host to connect to local forwarded ports"
complete -c ssh -s I --description "Smartcard device"
complete -c ssh -s k --description "Disable forwarding of Kerberos tickets"
complete -c ssh -s l -x -a "(__fish_complete_users)" --description "User"
complete -c ssh -s m --description "MAC algorithm"
complete -c ssh -s n --description "Prevent reading from stdin"
complete -c ssh -s N --description "Do not execute remote command"
complete -c ssh -s p -x --description "Port"
complete -c ssh -s q --description "Quiet mode"
complete -c ssh -s s --description "Subsystem"
complete -c ssh -s t --description "Force pseudo-tty allocation"
complete -c ssh -s T --description "Disable pseudo-tty allocation"
complete -c ssh -s x --description "Disable X11 forwarding"
complete -c ssh -s X --description "Enable X11 forwarding"
complete -c ssh -s L --description "Locally forwarded ports"
complete -c ssh -s R --description "Remotely forwarded ports"
complete -c ssh -s D --description "Dynamic port forwarding"

# Since ssh runs subcommands, it can accept any switches
complete -c ssh -u
