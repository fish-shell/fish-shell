
complete -c time -a "(__fish_complete_subcommand -- -o --output -f --format)" --description "Command"

complete -c time -s f -l format -n "__fish_no_arguments" --description "Specify output format" -x
complete -c time -s p -l portable -n "__fish_no_arguments" --description "Use the portable output format"
complete -c time -s o -l output -n "__fish_no_arguments" --description "Do not send the results to stderr, but overwrite the specified file" -r
complete -c time -s a -l append -n "__fish_no_arguments" --description "(Used together with -o) Do not overwrite but append"
complete -c time -s v -l verbose -n "__fish_no_arguments" --description "Verbose mode"
complete -c time -l help -n "__fish_no_arguments" --description "Display help and exit"
complete -c time -s V -l version -n "__fish_no_arguments" --description "Display version and exit"
