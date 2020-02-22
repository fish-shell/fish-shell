# First written by:  Sean C. Higgins

complete -x -c telnet -d Hostname -a "(__fish_print_hostnames)"

# Common across all telnet implementations
complete -c telnet -s 4 -d "Use IPv4 addresses only"
complete -c telnet -s 6 -d "Use IPv6 addresses only"
complete -c telnet -s 8 -d (_ "Specifies an 8-bit data path")
complete -c telnet -s E -d (_ "Stops any character from being recognized as an escape character")
complete -c telnet -s e -x -d "Set escape character"
complete -c telnet -s K -d (_ "Specifies no automatic login to remote system")
complete -c telnet -s a -d (_ "Attempt automatic login")
complete -c telnet -s c -d (_ "Disables reading user's .telnetrc")
complete -c telnet -s l -x -a "(__fish_complete_users)" -d (_ "User login")
complete -c telnet -s L -d (_ "Specifies an 8-bit data path")
complete -c telnet -s n -x -d (_ "Log to tracefile")
complete -c telnet -s r -d (_ "User interface similar to rlogin")

switch (uname -s)
  case FreeBSD NetBSD Linux
    complete -c telnet -s d -d (_ "Sets debug mode")
end

switch (uname -s)
  case FreeBSD NetBSD
    complete -c telnet -s F -s f -d "Forward local credentials if using Kerberos V5 authentication"
    complete -c telnet -s S -x -d (_ "Sets IP TOS")
    complete -c telnet -s x -d (_ "Turn on encryption")
    complete -c telnet -s k -x -d (_ "Use Kerberos realm for authentication")
    complete -c telnet -s N -d "Prevent IP address to name lookup"
    complete -c telnet -s X -x -d (_ "Disables specified type of authentication")
end

switch (uname -s)
  case FreeBSD
    complete -c telnet -s y -d "Turn off encryption"
    complete -c telnet -s B -x -d "Set baudrate"
    complete -c telnet -s s -x -d "Set source IP address"
    complete -c telnet -s u -d "Force AF_UNIX addresses only"
  case NetBSD
    complete -c telnet -s P -x -d "Set IPsec policy specfication"
  case OpenBSD
    complete -c telnet -s 7 -d (_ "Do not try to negotiate TELNET BINARY option")
    complete -c telnet -s V -x -d "Set routing table"
    complete -c telnet -s D -d "Disable rewriting of the DISPLAY var"
    complete -c telnet -s b -x -a "(__fish_print_addresses)" -d "Local address to bind to"
  case Linux
    complete -c telnet -s '?' -l help -d "Print help and exit"
    complete -c telnet -l usage -d "Print short usage and exit"
    complete -c telnet -s V -l version -d "Print program version"
end
