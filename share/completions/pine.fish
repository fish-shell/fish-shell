complete -c pine -s f -d "Open folder" -a "(ls -d ~/.Mail/*)"
complete -c pine -s F -d "Open file" -r
complete -c pine -s h -d "Display help and exit"
complete -c pine -s i -d "Start in folder index"
complete -c pine -s I -d "Initial set of keystrokes" -x
complete -c pine -s k -d "Use function keys for commands"
complete -c pine -s l -d "Expand collections in FOLDER LIST display"
complete -c pine -s n -d "Start with specified current message number" -x
complete -c pine -s o -d "Open folder read-only"
complete -c pine -s p -d "Set configuration file" -r
complete -c pine -s P -d "Set global configuration file"
complete -c pine -s r -d "Restricted mode"
complete -c pine -s z -d "Enable suspension support"
complete -c pine -o conf -d "Produce a sample global configuration file"
complete -c pine -o pinerc -d "Produce sample configuration file" -r
complete -c pine -o sort -d "Set mail sort order" -a "
	arrival
	subject
	from
	date
	size
	orderedsubj
	reverse
"

complete -c pine -o option -d "Config option" -x
