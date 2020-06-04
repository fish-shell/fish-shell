set PROG 'hikari'

complete -c $PROG -e 
complete -c $PROG -f
complete -c $PROG -s 'a' -a 'executable' -d 'Specify autostart executable'
complete -c $PROG -s 'c' -a 'config' -d 'Specify a configuration file'
complete -c $PROG -f -s 'h' -d 'Show help message and quit'
complete -c $PROG -f -s 'v' -d 'Show version and quit'
