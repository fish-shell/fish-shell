#completion for arp
complete -c arp -s v -l verbose -d (_ "Verbose mode")
complete -c arp -s n -l numeric -d (_ "numerical address")
complete -x -c arp -s H -l tw-type -a "ether arcnet pronet ax25 netrom" -d (_ "class of hw type")
complete -c arp -s a -l display -x -a "(__fish_print_hostnames)" -d (_ "show arp entries")
complete -x -c arp -s d -l delete -a "(__fish_print_hostnames)" -d (_ "remove an entry for hostname")
complete -c arp -s D -l use-device -d (_ "use hardware address")
complete -x -c arp -s i -l device -a "(__fish_print_interfaces)" -d (_ "select interface")
complete -x -c arp -s s -l set -d (_ "Manually create ARP address") -a "(__fish_print_hostnames)"
complete -f -c arp -s f -l file -d (_ "take addr from filename, default /etc/ethers") 

