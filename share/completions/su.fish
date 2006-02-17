# Completions for su

complete -x -c su -a "(__fish_complete_users)"
complete -c su -s l -l login -d (_ "Make login shell")
complete -r -c su -s c -l command -d (_ "Pass command to shell") -xa "(__fish_complete_command)" 
complete -c su -s f -l fast -d (_ "Pass -f to the shell")
complete -c su -s m -l preserve_environment -d (_ "Preserve environment")
complete -c su -s p -d (_ "Preserve environment")
complete -x -c su -s s -l shell -a "(cat /etc/shells)"
complete -c su -l help -d (_ "Display help and exit")
complete -c su -l version -d (_ "Display version and exit")

