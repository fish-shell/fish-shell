complete -x -c ifconfig -a down -d (_ "Stop interface")
complete -x -c ifconfig -a up -d (_ "Start interface")
complete -x -c ifconfig -a "
	{,-}arp
	{,-}promisc
	{,-}allmulti
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
	{,-}broadcast
	{,-}pointopoint
	hw
	multicast
	address
	txqueuelen
"
complete -x -c ifconfig -a "(__fish_print_interfaces)" -d (_ "Network interface")
