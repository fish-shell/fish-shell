set PROG hikari

complete -c $PROG -f
complete -c $PROG -s a -F -d 'Specify autostart executable'
complete -c $PROG -s c -F -d 'Specify a configuration file'
complete -c $PROG -f -s h -d 'Show help message and quit'
complete -c $PROG -f -s v -d 'Show version and quit'
