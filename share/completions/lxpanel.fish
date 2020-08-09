complete -c lxpanel -s h -l help -d 'print this help and exit'
complete -c lxpanel -s v -l version -d 'print version and exit'
complete -c lxpanel -s p -l profile -d 'use specified profile' -xa '(find ~/.config/lxpanel/ -maxdepth 1 -mindepth 1  -type d -printf "%f\n")'
complete -c lxpanel -l log -r -d 'set log level 0-5. 0 - none 5 - chatty'
