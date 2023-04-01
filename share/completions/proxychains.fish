complete -c proxychains -n __ghoti_is_first_token -s q -d "Makes proxychains quiet"
complete -c proxychains -n __ghoti_is_first_token -s f -r -F -d "Specify a config file"
complete -c proxychains -xa "(__ghoti_complete_subcommand)"
