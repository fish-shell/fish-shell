complete -c pine -s f -d (_ "Open folder") -a "(cd ~/.Mail; ls -d *)"
complete -c pine -s F -d (_ "Open file") -r
complete -c pine -s h -d (_ "Display help and exit")
complete -c pine -s i -d (_ "Start in folder index")
complete -c pine -s I -d (_ "Initial set of keystrokes") -x
complete -c pine -s k -d (_ "Use function keys for commands")
complete -c pine -s l -d (_ "Expand collections in FOLDER LIST display")
complete -c pine -s n -d (_ "Start with specified current message number") -x
complete -c pine -s o -d (_ "Open folder read-only")
complete -c pine -s p -d (_ "Set configuration file") -r
complete -c pine -s P -d (_ "Set global configuration file")
complete -c pine -s r -d (_ "Restricted mode")
complete -c pine -s z -d (_ "Enable suspention support")
complete -c pine -o conf -d (_ "Produce a sample global configuration file")
complete -c pine -o pinerc -d (_ "Produce sample configuration file") -r
complete -c pine -o sort -d (_ "Set mail sort order") -a "
	arrival
	subject
	from
	date
	size
	orderedsubj
	reverse
"

complete -c pine -o option -d (_ "Config option") -x
