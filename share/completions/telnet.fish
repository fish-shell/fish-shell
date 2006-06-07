#
# Load common telnet options
#
# Written by:  Sean C. Higgins
#

complete -x -c telnet -d Hostname -a "(__fish_print_hostnames)"

complete -c telnet -s 8 -d (_ "Specifies an 8-bit data path")
complete -c telnet -s 7 -d (_ "Do not try to negotiate TELNET BINARY option")
complete -c telnet -s E -d (_ "Stops any character from being recognized as an escape character")
complete -c telnet -s F -d (_ "Use local Kerberos authentication, if possible")
complete -c telnet -s K -d (_ "Specifies no automatic login to remote system")
complete -c telnet -s L -d (_ "Specifies an 8-bit data path")
complete -c telnet -s a -d (_ "Attempt automatic login")
complete -c telnet -s c -d (_ "Disables reading user's .telnetrc")
complete -c telnet -s d -d (_ "Sets debug mode")
complete -c telnet -s S -x -d (_ "Sets IP TOS")
complete -c telnet -s X -x -d (_ "Disables specified type of authentication")
complete -c telnet -s l -x -a "(__fish_complete_users)" -d (_ "User login")
complete -c telnet -s n -x -d (_ "Log to tracefile")
complete -c telnet -s x -d (_ "Turn on encryption")
complete -c telnet -s r -d (_ "User interface similar to rlogin")
complete -c telnet -s k -x -d (_ "Use Kerberos realm for authentication")

