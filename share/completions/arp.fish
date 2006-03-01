#completion for arp
complete -c arp -s v -l verbose -d (N_ "Verbose mode")
complete -c arp -s n -l numeric -d (N_ "Numerical address")
complete -x -c arp -s H -l tw-type -a "ether arcnet pronet ax25 netrom" -d (N_ "Class of hw type")
complete -c arp -s a -l display -x -a "(__fish_print_hostnames)" -d (N_ "Show arp entries")
complete -x -c arp -s d -l delete -a "(__fish_print_hostnames)" -d (N_ "Remove an entry for hostname")
complete -c arp -s D -l use-device -d (N_ "Use hardware address")
complete -x -c arp -s i -l device -a "(__fish_print_interfaces)" -d (N_ "Select interface")
complete -x -c arp -s s -l set -d (N_ "Manually create ARP address") -a "(__fish_print_hostnames)"
complete -f -c arp -s f -l file -d (N_ "Take addr from filename, default /etc/ethers") 

