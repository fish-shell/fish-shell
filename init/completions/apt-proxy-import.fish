#apt-proxy-import
complete -c apt-proxy-import -s h -l help -d "apt-proxy-import command help"
complete -f -c apt-proxy-import -s V -l version -d "print version"
complete -f -c apt-proxy-import -s v -l verbose -d "verbose info"
complete -f -c apt-proxy-import -s q -l quiet -d "no message to STDOUT"
complete -f -c apt-proxy-import -s r -l recursive -d "recurse into subdir"
complete -r -c apt-proxy-import -s i -l import-dir -a "(ls -Fp|grep /$)" -d "dir to import"
complete -r -c apt-proxy-import -s u -l user -a "(__fish_complete_users)" -d "change to user"
complete -r -c apt-proxy-import -s d -l debug -d "debug level[default 0]"

