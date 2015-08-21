#apt-proxy-import
complete -c apt-proxy-import -s h -l help --description 'Display help and exit'
complete -f -c apt-proxy-import -s V -l version --description 'Display version and exit'
complete -f -c apt-proxy-import -s v -l verbose --description 'Verbose mode'
complete -f -c apt-proxy-import -s q -l quiet --description 'No message to STDOUT'
complete -f -c apt-proxy-import -s r -l recursive --description 'Recurse into subdir'
complete -r -c apt-proxy-import -s i -l import-dir -a '(ls -Fp| __fish_sgrep /\$)' --description 'Dir to import'
complete -r -c apt-proxy-import -s u -l user -a '(__fish_complete_users)' --description 'Change to user'
complete -r -c apt-proxy-import -s d -l debug --description 'Debug level[default 0]'

