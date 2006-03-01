complete -c pine -s f -d (N_ "Open folder") -a "(cd ~/.Mail; ls -d *)"
complete -c pine -s F -d (N_ "Open file") -r
complete -c pine -s h -d (N_ "Display help and exit")
complete -c pine -s i -d (N_ "Start in folder index")
complete -c pine -s I -d (N_ "Initial set of keystrokes") -x
complete -c pine -s k -d (N_ "Use function keys for commands")
complete -c pine -s l -d (N_ "Expand collections in FOLDER LIST display")
complete -c pine -s n -d (N_ "Start with specified current message number") -x
complete -c pine -s o -d (N_ "Open folder read-only")
complete -c pine -s p -d (N_ "Set configuration file") -r
complete -c pine -s P -d (N_ "Set global configuration file")
complete -c pine -s r -d (N_ "Restricted mode")
complete -c pine -s z -d (N_ "Enable suspension support")
complete -c pine -o conf -d (N_ "Produce a sample global configuration file")
complete -c pine -o pinerc -d (N_ "Produce sample configuration file") -r
complete -c pine -o sort -d (N_ "Set mail sort order") -a "
	arrival
	subject
	from
	date
	size
	orderedsubj
	reverse
"

complete -c pine -o option -d (N_ "Config option") -x
