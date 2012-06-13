
complete -c rc.d -xa 'list start stop restart help' -n 'not __fish_seen_subcommand_from list start stop restart'
complete -c rc.d -s s -l started -n '__fish_seen_subcommand_from list start stop restart' -d 'Filter started daemons'
complete -c rc.d -s S -l stopped -n '__fish_seen_subcommand_from list start stop restart' -d 'Filter stopped daemons'
complete -c rc.d -s a -l auto    -n '__fish_seen_subcommand_from list start stop restart' -d 'Filter auto started daemons'
complete -c rc.d -s A -l noauto  -n '__fish_seen_subcommand_from list start stop restart' -d 'Filter manually started daemons'
complete -c rc.d -n '__fish_seen_subcommand_from list start stop restart' -xa '( __fish_print_arch_daemons )'
