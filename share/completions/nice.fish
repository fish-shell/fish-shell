

complete -c nice -a "(__fish_complete_subcommand -- -n --adjustment)" -d (N_ "Command")

complete -c nice -s n -l adjustment -n "__fish_no_arguments" -d (N_ "Add specified amount to niceness value") -x
complete -c nice -l help -n "__fish_no_arguments" -d (N_ "Display help and exit")
complete -c nice -l version -n "__fish_no_arguments" -d (N_ "Display version and exit")

