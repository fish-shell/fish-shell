
complete -c lua -s e -d 'Execute string' -x
complete -c lua -s l -d 'Require library' -xa "( find /usr/lib/lua/ -name \*.so -printf '%f\n' | sed 's/.so//' )"
complete -c lua -s i -d 'Enter interactive mode after executing script'
complete -c lua -s v -d 'Show version'
complete -c lua -s h -l help -d 'Print help and exit'
complete -c lua -a "(__fish_complete_suffix .lua)"
complete -c lua -a '-' -d 'Execute stdin and stop handling options'
