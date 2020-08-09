set -l commands store restore nrestore init daemon rdaemon kill monitor

complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a store -d "Save current driver state" -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a restore -d "Load driver state" -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a nrestore -d "Restore and rescan for available soundcards" -f
complete -c alsactl -s F -l force -n "__fish_seen_subcommand_from restore nrestore" -d 'Try to restore control elements as much as possible' -f
complete -c alsactl -s g -l ignore -n "__fish_seen_subcommand_from store restore nrestore" -d 'Ignore missing soundcards' -f
complete -c alsactl -s P -l pedantic -n "__fish_seen_subcommand_from restore nrestore" -d 'Do not restore mismatching control elements' -f
complete -c alsactl -s I -l no-init-fallback -n "__fish_seen_subcommand_from restore nrestore" -d 'Do not init if restore fails' -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a init -d "Initialize all devices to a default state" -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a daemon -d "Periodically save state" -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a rdaemon -d "Restore state and then periodically save it" -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a kill -d "Notify daemon to do an operation" -f
complete -c alsactl -n "not __fish_seen_subcommand_from $commands" -a monitor -d "Monitor events" -f

complete -c alsactl -s h -l help -d 'Show available flags and commands' -f
complete -c alsactl -s d -l debug -d 'Make output a bit more verbose' -f
complete -c alsactl -s v -l version -d 'Print alsactl version number' -f
complete -c alsactl -s f -l file -d 'Select the configuration file to use'
complete -c alsactl -s l -l lock -d 'Use a lock file'
complete -c alsactl -s L -l no-lock -d 'Do not use a lock file'
complete -c alsactl -s O -l lock-state-file -d 'Select the state lock file path'
complete -c alsactl -s r -l runstate -d 'Save restore and init state to this file'
complete -c alsactl -s R -l remove -d 'Remove runstate file at first'
complete -c alsactl -s E -l env -d 'Set environment variable'
complete -c alsactl -s i -l initfile -d 'The configuration file for init'
complete -c alsactl -s p -l period -d 'The store period in seconds for the daemon command' -f
complete -c alsactl -s e -l pid-file -d 'The PID file to use'
complete -c alsactl -s b -l background -d 'Run the task in background'
complete -c alsactl -s s -l syslog -d 'Use syslog for messages'
complete -c alsactl -s n -l nice -d 'Set the process priority (see \'man nice\')' -a "(seq -20 19)"
complete -c alsactl -s c -l sched-idle -d 'Set the process scheduling policy to idle (SCHED_IDLE)'
