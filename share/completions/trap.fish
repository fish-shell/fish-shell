
complete -c trap -s l -l list-signals --description 'Display names of all signals'
complete -c trap -s p -l print --description 'Display all currently defined trap handlers'
complete -c trap -s h -l help --description 'Display help and exit'
complete -c trap -a '(trap -l | sed "s/ /\n/g")' --description 'Signal'
