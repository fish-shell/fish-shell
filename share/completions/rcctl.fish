complete -c rcctl -xa 'check ls reload restart stop start disable enable' -n 'not __fish_seen_subcommand_from list check ls reload restart stop start enable disable'
complete -c rcctl -n '__fish_seen_subcommand_from check reload restart stop start enable disable' -xa '(set -l files /etc/rc.d/*; string replace "/etc/rc.d/" "" -- $files)'
