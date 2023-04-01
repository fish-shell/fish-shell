

complete -c nice -a "(__ghoti_complete_subcommand -- -n --adjustment)" -d Command

complete -c nice -s n -l adjustment -n __ghoti_no_arguments -d "Add specified amount to niceness value" -x
complete -c nice -l help -n __ghoti_no_arguments -d "Display help and exit"
complete -c nice -l version -n __ghoti_no_arguments -d "Display version and exit"
