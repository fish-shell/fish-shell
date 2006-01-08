#apt-proxy-import
complete -c apt-proxy-import -s h -l help -d (_ 'Apt-proxy-import command help')
complete -f -c apt-proxy-import -s V -l version -d (_ 'Print version')
complete -f -c apt-proxy-import -s v -l verbose -d (_ 'Verbose info')
complete -f -c apt-proxy-import -s q -l quiet -d (_ 'No message to STDOUT')
complete -f -c apt-proxy-import -s r -l recursive -d (_ 'Recurse into subdir')
complete -r -c apt-proxy-import -s i -l import-dir -a '(ls -Fp|grep /$)' -d (_ 'Dir to import')
complete -r -c apt-proxy-import -s u -l user -a '(__fish_complete_users)' -d (_ 'Change to user')
complete -r -c apt-proxy-import -s d -l debug -d (_ 'Debug level[default 0]')

