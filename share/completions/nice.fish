

complete -c nice -a "(__fish_complete_subcommand -- -n --adjustment)" --description "Command"

complete -c nice -s n -l adjustment -n "__fish_no_arguments" --description "Add specified amount to niceness value" -x
complete -c nice -l help -n "__fish_no_arguments" --description "Display help and exit"
complete -c nice -l version -n "__fish_no_arguments" --description "Display version and exit"

