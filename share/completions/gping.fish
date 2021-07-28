# Completion for https://github.com/orf/gping

complete -x -c gping -d "Ping, but with a graph"
complete -x -c gping -l cmd -d "Graph execution time for this command"
complete -f -c gping -s h -l help -d "Print help information"
complete -f -c gping -s 4 -d "Resolve ping targets to IPv4 address"
complete -f -c gping -s 6 -d "Resolve ping targets to IPv6 address"
complete -f -c gping -s s -l simple-graphics -d "Use dot character instead of braille"
complete -x -c gping -s b -l buffer -d "Number of seconds to display on graph"
complete -x -c gping -s n -l watch-interval -d "Watch interval in seconds"
