
complete -c time -a "(__fish_complete_subcommand -- -o --output -f --format)" -d (_ "Command")

complete -c time -s f -l format -n "__fish_no_arguments" -d (_ "Specify output format") -x
complete -c time -s p -l portable -n "__fish_no_arguments" -d (_ "Use the portable output format")
complete -c time -s o -l output -n "__fish_no_arguments" -d (_ "Do not send the results to stderr, but overwrite the specified file") -r
complete -c time -s a -l append -n "__fish_no_arguments" -d (_ "(Used together with -o) Do not overwrite but append")
complete -c time -s v -l verbose -n "__fish_no_arguments" -d (_ "Verbose mode")
complete -c time -l help -n "__fish_no_arguments" -d (_ "Display help and exit")
complete -c time -s V -l version -n "__fish_no_arguments" -d (_ "Display version and exit")

