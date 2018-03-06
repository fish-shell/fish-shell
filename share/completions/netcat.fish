# NetCat completions for fish
# By James Stanley

# magic completion safety check (do not remove this comment)
if not type -q netcat
    exit
end

complete -r -c nc -d "Remote hostname" -a "(__fish_print_hostnames)"
complete -r -c nc -s c -d "Same as -e, but use /bin/sh"
complete -r -c nc -s e -d "Program to execute after connection"
complete -c nc -s b -d "Allow broadcasts"
complete -r -c nc -s g -d "Source-routing hop points"
complete -r -c nc -s G -d "Source-routing pointer"
complete -c nc -s h -d "Show help"
complete -r -c nc -s i -d "Delay interval for lines sent, ports scaned"
complete -c nc -s k -d "Set keepalive option"
complete -c nc -s l -d "Listen mode, acts as a server"
complete -c nc -s n -d "Numeric-only IP addresses, no DNS"
complete -r -c nc -s o -d "Hex dump of traffic"
complete -r -c nc -s p -d "Local port number"
complete -c nc -s r -d "Randomize local and remote ports"
complete -r -c nc -s q -d "Quit after EOF on stdin and delay of secs"
complete -r -c nc -s s -d "Local source address"
complete -c nc -s t -d "Answer Telnet negotiation"
complete -c nc -s u -d "UDP Mode"
complete -c nc -s v -d "Verbose, use twice to be more verbose"
complete -r -c nc -s w -d "Timeout for connects and final net reads"
complete -r -c nc -s x -d "Set Type of Service"
complete -c nc -s z -d "No I/O - used for scanning"
