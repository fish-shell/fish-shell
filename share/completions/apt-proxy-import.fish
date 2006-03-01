#apt-proxy-import
complete -c apt-proxy-import -s h -l help -d (N_ 'Display help and exit')
complete -f -c apt-proxy-import -s V -l version -d (N_ 'Display version and exit')
complete -f -c apt-proxy-import -s v -l verbose -d (N_ 'Verbose mode')
complete -f -c apt-proxy-import -s q -l quiet -d (N_ 'No message to STDOUT')
complete -f -c apt-proxy-import -s r -l recursive -d (N_ 'Recurse into subdir')
complete -r -c apt-proxy-import -s i -l import-dir -a '(ls -Fp|grep /$)' -d (N_ 'Dir to import')
complete -r -c apt-proxy-import -s u -l user -a '(__fish_complete_users)' -d (N_ 'Change to user')
complete -r -c apt-proxy-import -s d -l debug -d (N_ 'Debug level[default 0]')

