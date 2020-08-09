#apt-proxy-import
complete -c apt-proxy-import -s h -l help -d 'Display help and exit'
complete -f -c apt-proxy-import -s V -l version -d 'Display version and exit'
complete -f -c apt-proxy-import -s v -l verbose -d 'Verbose mode'
complete -f -c apt-proxy-import -s q -l quiet -d 'No message to STDOUT'
complete -f -c apt-proxy-import -s r -l recursive -d 'Recurse into subdir'
complete -r -c apt-proxy-import -s i -l import-dir -a '(for i in */; echo $i; end)' -d 'Dir to import'
complete -r -c apt-proxy-import -s u -l user -a '(__fish_complete_users)' -d 'Change to user'
complete -r -c apt-proxy-import -s d -l debug -d 'Debug level[default 0]'
