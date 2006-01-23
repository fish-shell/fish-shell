
complete -c time -a "(__fish_complete_command)" -d (_ "Command")

complete -c time -s f -l format -d (_ "Specify output format") -x
complete -c time -s p -l portable -d (_ "Use the portable output format")
complete -c time -s o -l output -d (_ "Do not send the results to stderr, but overwrite the specified file") -r
complete -c time -s a -l append -d (_ "(Used together with -o) Do not overwrite but append")
complete -c time -s v -l verbose -d (_ "Verbose mode")
complete -c time -l help -d (_ "Display help and exit")
complete -c time -s V -l version -d (_ "Display version and exit")

