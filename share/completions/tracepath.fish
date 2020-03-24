complete -c tracepath -x -a "(__fish_print_hostnames)"

complete -c tracepath -s 4 -d 'Use IPv4 only'
complete -c tracepath -s 6 -d 'Use IPv6 only'
complete -c tracepath -s n -d 'Print IP addresses numerically'
complete -c tracepath -s b -d 'Print host names and IP addresses'
complete -c tracepath -s l -x -d 'Sets the initial packet length'
complete -c tracepath -s m -x -d 'Set maximum hops'
complete -c tracepath -s p -x -d 'Sets the initial destination port'
complete -c tracepath -s V -d 'Print version and exit'
