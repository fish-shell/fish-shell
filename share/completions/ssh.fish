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

(__fish_print_users)@
"

complete -c ssh -s a -d (N_ "Disables forwarding of the authentication agent")
complete -c ssh -s A -d (N_ "Enables forwarding of the authentication agent")
complete -x -c ssh -s b -d (N_ "Interface to transmit from") -a "
(
	cat /proc/net/arp ^/dev/null| grep -v '^IP'|cut -d ' ' -f 1 ^/dev/null
)
"

complete -x -c ssh -s e -d (N_ "Escape character") -a "^ none"
complete -c ssh -s f -d (N_ "Go to background")
complete -c ssh -s g -d (N_ "Allow remote host to connect to local forwarded ports")
complete -c ssh -s I -d (N_ "Smartcard device")
complete -c ssh -s k -d (N_ "Disable forwarding of Kerberos tickets")
complete -c ssh -s l -x -a "(__fish_complete_users)" -d (N_ "User")
complete -c ssh -s m -d (N_ "MAC algorithm")
complete -c ssh -s n -d (N_ "Prevent reading from stdin")
complete -c ssh -s N -d (N_ "Do not execute remote command")
complete -c ssh -s p -x -d (N_ "Port")
complete -c ssh -s q -d (N_ "Quiet mode")
complete -c ssh -s s -d (N_ "Subsystem")
complete -c ssh -s t -d (N_ "Force pseudo-tty allocation")
complete -c ssh -s T -d (N_ "Disable pseudo-tty allocation")
complete -c ssh -s x -d (N_ "Disable X11 forwarding")
complete -c ssh -s X -d (N_ "Enable X11 forwarding")
complete -c ssh -s L -d (N_ "Locally forwarded ports")
complete -c ssh -s R -d (N_ "Remotely forwarded ports")
complete -c ssh -s D -d (N_ "Dynamic port forwarding")
