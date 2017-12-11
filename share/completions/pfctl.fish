complete -c pfctl -s s --description 'Show a filter parameter by modifier' -xa \
	'queue\t"Show the currently loaded queue definitions." \
	rules\t"Show the currently loaded filter rules." \
	Anchors\t"Show the currently loaded anchors directly attached to the main ruleset." \
	states\t"Show the contents of the state table."
	Sources\t"Show the contents of the source tracking table." \
	info\t"Show filter information (statistics and counters)." \
	labels\t"Show per-rule statistics (label, evaluations, packets total, bytes total, packets in, bytes in, packets out, bytes out, state creations) of filter rules with labels, useful for accounting." \
	timeouts\t"Show the current global timeouts." \
	memory\t"Show the current pool memory hard limits." \
	Tables\t"Show the list of tables." \
	osfp\t"Show the list of operating system fingerprints." \
	Interfaces\t"Show the list of interfaces and interface drivers available to PF." \
	all\t"Everything."'
complete -c pfctl -s F --description 'Flush the filter params specified by mod' -xa \
	'rules\t"Flush the filter rules." \
	states\t"Flush the state table (NAT and filter)." \
	Sources\t"Flush the source tracking table." \
	info\t"Flush the filter information (statistics that are not bound to rules)." \
	Tables\t"Flush the tables." \
	ospf\t"Flush the passive operating system fingerprints." \
	all\t"Flush everything."'
complete -c pfctl -s T --description 'Table command' -xa \
	'kill\t"Kill a table." \
	flush\t"Flush all addresses of a table." \
	add\t"Add one or more addresses in a table." \
	delete\t"Delete one or more addresses from a table." \
	expire\t"Delete addresses which had their statistics cleared more than number seconds ago." \
	replace\t"Replace the addresses of the table." \
	show\t"Show the content (addresses) of a table." \
	test\t"Test if the given addresses match a table." \
	zero\t"Clear all the statistics of a table."'
