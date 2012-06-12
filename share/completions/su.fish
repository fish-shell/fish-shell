# Completions for su

complete -x -c su -a "(__fish_complete_users)"
complete -c su -s l -l login --description "Make login shell"
complete -r -c su -s c -l command --description "Pass command to shell" -xa "(complete -C(commandline -ct))"
complete -c su -s f -l fast --description "Pass -f to the shell"
complete -c su -s m -l preserve_environment --description "Preserve environment"
complete -c su -s p --description "Preserve environment"
complete -x -c su -s s -l shell -a "(cat /etc/shells)"
complete -c su -l help --description "Display help and exit"
complete -c su -l version --description "Display version and exit"

