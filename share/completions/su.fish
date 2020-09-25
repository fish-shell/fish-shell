function __fish_complete_su_env_whitelist
    env | string match -v -e -r '^(?:HOME|SHELL|USER|LOGNAME|PATH)=' | string replace -r '([^=]+)=(.*)' '$1\t$2'
end

complete -c su -x -a "(__fish_complete_users)"

complete -c su -s l -l login -d "Make login shell"
complete -c su -s c -l command -d "Pass command to shell" -xa "(complete -C '')"
complete -c su -s f -l fast -d "Pass -f to the shell"
complete -c su -s g -l group -x -a "(__fish_complete_groups)" -d "Specify the primary group"
complete -c su -s G -l supp-group -x -a "(__fish_complete_groups)" -d "Specify a supplemental group"
complete -c su -s m -s p -l preserve_environment -d "Preserve environment"
complete -c su -s P -l pty -d "Create pseudo-terminal for the session"
complete -c su -s s -l shell -x -a "(cat /etc/shells)" -d "Run the specified shell"
complete -c su -s w -l whitelist-environment -x -a "(__fish_complete_list , __fish_complete_su_env_whitelist)" -d "Don't reset these environment variables"
complete -c su -s h -l help -d "Display help and exit"
complete -c su -s V -l version -d "Display version and exit"
