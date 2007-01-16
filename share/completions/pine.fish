complete -c pine -s f --description "Open folder" -a "(cd ~/.Mail; ls -d *)"
complete -c pine -s F --description "Open file" -r
complete -c pine -s h --description "Display help and exit"
complete -c pine -s i --description "Start in folder index"
complete -c pine -s I --description "Initial set of keystrokes" -x
complete -c pine -s k --description "Use function keys for commands"
complete -c pine -s l --description "Expand collections in FOLDER LIST display"
complete -c pine -s n --description "Start with specified current message number" -x
complete -c pine -s o --description "Open folder read-only"
complete -c pine -s p --description "Set configuration file" -r
complete -c pine -s P --description "Set global configuration file"
complete -c pine -s r --description "Restricted mode"
complete -c pine -s z --description "Enable suspension support"
complete -c pine -o conf --description "Produce a sample global configuration file"
complete -c pine -o pinerc --description "Produce sample configuration file" -r
complete -c pine -o sort --description "Set mail sort order" -a "
	arrival
	subject
	from
	date
	size
	orderedsubj
	reverse
"

complete -c pine -o option --description "Config option" -x
