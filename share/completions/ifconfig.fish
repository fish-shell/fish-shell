complete -x -c ifconfig -a down --description "Stop interface"
complete -x -c ifconfig -a up --description "Start interface"
complete -x -c ifconfig -a "
	(array '' -)arp
	(array '' -)promisc
	(array '' -)allmulti
	metric
	mtu
	dstaddr
	netmask
	add
	del
	tunnel
	irq
	io_addr
	mem_start
	media
	(array '' -)broadcast
	(array '' -)pointopoint
	hw
	multicast
	address
	txqueuelen
"
complete -x -c ifconfig -a "(__fish_print_interfaces)" --description "Network interface"
