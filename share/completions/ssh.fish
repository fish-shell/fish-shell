#
# Load completions shared by ssh and scp.
#
__fish_complete_ssh ssh

#
# ssh specific completions
#
complete -x -c ssh -d Hostname -a "
# First print an empty line to get just the hostname
(echo;
# Then prepend any username specified in the completion to the hostname
commandline -ct | string replace -rf '(.*@).*' '$1'
)(__fish_print_hostnames)
"

# Disable as username completion is not very useful.
# complete -x -c ssh -d User -a "
# (__fish_print_users | string match -r -v '^_')@
# "

complete -c ssh -d "Command to run" -x -a '(__fish_complete_subcommand --fcs-skip=2)'

complete -c ssh -s a -d "Disables forwarding of the authentication agent"
complete -c ssh -s A -d "Enables forwarding of the authentication agent"
# TODO: Improve this since /proc/net/arp is not POSIX compliant.
complete -x -c ssh -s b -d "Interface to transmit from" -a "
(cut -d ' ' -f 1 /proc/net/arp ^/dev/null | string match -r -v '^IP')
"

complete -x -c ssh -s e -d "Escape character" -a "\^ none"
complete -c ssh -s f -d "Go to background"
complete -c ssh -s g -d "Allow remote host to connect to local forwarded ports"
complete -c ssh -s I -d "Smartcard device"
complete -c ssh -s k -d "Disable forwarding of Kerberos tickets"
complete -c ssh -s l -x -a "(__fish_complete_users)" -d "User"
complete -c ssh -s m -d "MAC algorithm"
complete -c ssh -s n -d "Prevent reading from stdin"
complete -c ssh -s N -d "Do not execute remote command"
complete -c ssh -s p -x -d "Port"
complete -c ssh -s q -d "Quiet mode"
complete -c ssh -s s -d "Subsystem"
complete -c ssh -s t -d "Force pseudo-tty allocation"
complete -c ssh -s T -d "Disable pseudo-tty allocation"
complete -c ssh -s x -d "Disable X11 forwarding"
complete -c ssh -s X -d "Enable X11 forwarding"
complete -c ssh -s L -d "Locally forwarded ports"
complete -c ssh -s R -d "Remotely forwarded ports"
complete -c ssh -s D -d "Dynamic port forwarding"
