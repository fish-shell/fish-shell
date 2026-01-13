complete -c cd -a "(__fish_complete_cd)"
complete -c cd -s h -l help -d 'Display help and exit'
complete -c cd -s L -l no-dereference -d 'Change directory without resolving symbolic links'
complete -c cd -s P -l dereference -d 'Resolve symbolic links before changing directory'
