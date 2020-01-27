# dumpcap - Dump network traffic

__fish_complete_wireshark dumpcap

complete -c dumpcap -s C -d 'Limit the amount of memory in bytes for storing captured packets in memory' -x
complete -c dumpcap -s d -d 'Dump the code generated for the capture filter in a human-readable form, and exit'
complete -c dumpcap -s M -d 'When used with -D, -L, -S or --list-time-stamp-types print machine-readable output'
complete -c dumpcap -s N -d 'Limit the number of packets used for storing captured packets in memory' -x
complete -c dumpcap -s P -d 'Save files as pcap instead of the default pcapng'
complete -c dumpcap -s S -d 'Print statistics for each interface once every second'
complete -c dumpcap -s t -d 'Use a separate thread per interface'
