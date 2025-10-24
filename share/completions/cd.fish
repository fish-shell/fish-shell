complete -c cd -a "(__fish_complete_cd)"
complete -c cd -s h -l help -d 'Display help and exit'
complete -c cd -s L -l logical -d 'Update PWD without resolving symbolic links'
complete -c cd -s P -l physical -d 'Resolve symbolic links before updating PWD'
