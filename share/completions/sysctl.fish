function __fish_sysctl_list_variable
  sysctl --all | sed "s/\s*=.*//g"
end

complete -f -c sysctl -a '(__fish_sysctl_list_variable)' -d 'Variable'

complete -f -c sysctl -s a -l all           -d 'Display all variables'
complete -f -c sysctl -s A                  -d 'Display all variables'
complete -f -c sysctl -s X                  -d 'Display all variables'
complete -f -c sysctl      -l deprecated    -d 'Include deprecated parameters to listing'
complete -f -c sysctl -s b -l binary        -d 'Print value without new line'
complete -f -c sysctl -s e -l ignore        -d 'Ignore unknown variables errors'
complete -f -c sysctl -s N -l names         -d 'Print variable names without values'
complete -f -c sysctl -s n -l values        -d 'Print only values of a variables'
complete    -c sysctl -s p -l load          -d 'Read values from file'
complete -f -c sysctl -s f                  -d 'Read values from file'
complete -f -c sysctl      -l system        -d 'Read values from all system directories'
complete -f -c sysctl -s r -l pattern       -d 'Select setting that match expression'
complete -f -c sysctl -s q -l quiet         -d 'Do not echo variable set'
complete -f -c sysctl -s w -l write         -d 'Enable writing a value to variable'
complete -f -c sysctl -s h -l help          -d 'Display help and exit'
complete -f -c sysctl -s d                  -d 'Display help and exit'
complete -f -c sysctl -s V -l version       -d 'Output version information and exit'
