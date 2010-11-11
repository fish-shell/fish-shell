#completion for arp
complete -c arp -s v -l verbose --description "Verbose mode"
complete -c arp -s n -l numeric --description "Numerical address"
complete -x -c arp -s H -l tw-type -a "ether arcnet pronet ax25 netrom" --description "Class of hw type"
complete -c arp -s a -l display -x -a "(__fish_print_hostnames)" --description "Show arp entries"
complete -x -c arp -s d -l delete -a "(__fish_print_hostnames)" --description "Remove an entry for hostname"
complete -c arp -s D -l use-device --description "Use hardware address"
complete -x -c arp -s i -l device -a "(__fish_print_interfaces)" --description "Select interface"
complete -x -c arp -s s -l set --description "Manually create ARP address" -a "(__fish_print_hostnames)"
complete -f -c arp -s f -l file --description "Take addr from filename, default /etc/ethers"

