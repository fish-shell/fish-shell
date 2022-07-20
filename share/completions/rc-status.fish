set -l levels '(set -l p /etc/runlevels/*; string replace /etc/runlevels "" $p)'

#disable file listing
complete -f rc-status
complete -c rc-status -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -xa "$levels"

complete -c rc-status -s h -l help -d 'Display the help output'
complete -c rc-status -s a -l all -d 'Show services from all run levels'
complete -c rc-status -s f -l format -d 'format status to be parsable'
complete -c rc-status -s l -l list -d 'Show list of run levels'
complete -c rc-status -s r -l runlevel -d 'Show the name of the current runlevel'
complete -c rc-status -s m -l manual -d 'Show manually started services'
complete -c rc-status -s s -l servicelist -d 'Show service list'
complete -c rc-status -s S -l supervised -d 'Show supervised services'
complete -c rc-status -s u -l unused -d 'Show services not assigned to any runlevel'
complete -c rc-status -s v -l version -d 'Display software version'
complete -c rc-status -s q -l quiet -d 'Run quietly'
complete -c rc-status -s C -l nocolor -d 'Disable color output'
complete -c rc-status -s V -l version -d 'Display software version'
