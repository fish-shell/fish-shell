complete -c trap -s l -l list-signals -d 'Display names of all signals'
complete -c trap -s p -l print -d 'Display all currently defined trap handlers'
complete -c trap -s h -l help -d 'Display help and exit'
complete -c trap -a '(trap -l | sed "s/ /\n/g")' -d Signal
