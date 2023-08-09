set -l subcommands 'bind split'
set -l subcommand_show_condition "not __fish_seen_subcommand_from $subcommands"
set -l split_option_show_condition "__fish_seen_subcommand_from split"

complete -c horcrux -a bind -n "$subcommand_show_condition" -f -d 'Bind directory'
complete -c horcrux -a split -n "$subcommand_show_condition" -f -d 'Split file'
complete -c horcrux -s n -r -n "$split_option_show_condition" -d 'Count of horcruxes to make'
complete -c horcrux -s t -r -n "$split_option_show_condition" -d 'Count of horcruxes required to resurrect the original file'
