# Completions for su

complete -x -c su -a "(__fish_complete_users)"
complete -c su -s l -l login -d "Make login shell"
complete -c su -s c -l command -d "Pass command to shell" -xa "(__fish_complete_external_command)"
complete -c su -s f -l fast -d "Pass -f to the shell"
complete -c su -s m -l preserve_environment -d "Preserve environment"
complete -c su -s p -d "Preserve environment"
complete -x -c su -s s -l shell -a "(cat /etc/shells)"
complete -c su -l help -d "Display help and exit"
complete -c su -l version -d "Display version and exit"

