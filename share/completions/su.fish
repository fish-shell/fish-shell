# Completions for su

complete -x -c su -a "(__fish_complete_users)"
complete -c su -s l -l login -d (N_ "Make login shell")
complete -r -c su -s c -l command -d (N_ "Pass command to shell") -xa "(__fish_complete_command)" 
complete -c su -s f -l fast -d (N_ "Pass -f to the shell")
complete -c su -s m -l preserve_environment -d (N_ "Preserve environment")
complete -c su -s p -d (N_ "Preserve environment")
complete -x -c su -s s -l shell -a "(cat /etc/shells)"
complete -c su -l help -d (N_ "Display help and exit")
complete -c su -l version -d (N_ "Display version and exit")

