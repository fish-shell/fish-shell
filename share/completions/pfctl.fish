complete -c pfctl -s s -d 'Show a filter parameter by modifier' -xa \
    'queue\t"Show loaded queue definitions" \
	rules\t"Show loaded filter rules" \
	Anchors\t"Show anchors attached to main ruleset" \
	states\t"Show state table"
	Sources\t"Show source tracking table" \
	info\t"Show filter information" \
	labels\t"Show stats of labeled filter rules" \
	timeouts\t"Show global timeouts" \
	memory\t"Show pool memory hard limits" \
	Tables\t"Show the list of tables" \
	osfp\t"Show a list of OS fingerprints" \
	Interfaces\t"List PF interfaces/drivers" \
	all\t"Everything."'
complete -c pfctl -s F -d 'Flush filter params specified by mod' -xa \
    'rules\t"Flush filter rules" \
	states\t"Flush state table" \
	Sources\t"Flush source tracking table" \
	info\t"Flush filter information" \
	Tables\t"Flush the tables" \
	ospf\t"Flush the passive OS fingerprints" \
	all\t"Flush everything"'
complete -c pfctl -s T -d 'Table command' -xa \
    'kill\t"Kill a table." \
	flush\t"Flush addresses of a table" \
	add\t"Add one or more addresses in table" \
	delete\t"Delete one or more addresses from a table" \
	expire\t"Delete addresses where stats cleared > N seconds" \
	replace\t"Replace the addresses of the table" \
	show\t"Show the contents of a table" \
	test\t"Test if the given addresses match a table" \
	zero\t"Clear all the stats of a table"'
