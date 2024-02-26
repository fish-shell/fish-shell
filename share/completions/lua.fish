complete -c lua -s e -d 'Execute string' -x
# Try the most common lib directories, silencing errors in case they don't exist.
complete -c lua -s l -d 'Require library' -xa "(find /usr/lib{,32,64}/lua/ -name \*.so -printf '%f\n' 2>/dev/null | string replace -r '.so\$' '')"
complete -c lua -s i -d 'Enter interactive mode after executing script'
complete -c lua -s v -d 'Show version'
complete -c lua -s h -l help -d 'Print help and exit'
complete -c lua -k -a "(__fish_complete_suffix .lua)"
complete -c lua -a - -d 'Execute stdin and stop handling options'
