complete -c yo -s h -l help -d 'Show help'
complete -c yo -s v -l version -d 'Show version'

complete -c yo -s f -l force -d 'Allow overwrite files that already exist'
complete -c yo -l no-color -d 'Disable colors'
complete -c yo -l generators -d 'List available generators'
complete -c yo -l local-only -n '__fish_seen_subcommand_from --generators' -d 'Disable lookup of globally-installed generators'

complete -c yo -a '(yo --generators | sed -n "3,\$p" | sed "\$d" | string replace --regex "^\s+" "")'
