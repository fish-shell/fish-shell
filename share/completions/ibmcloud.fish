set -l commands (ibmcloud --generate-bash-completion)

complete -c ibmcloud --no-files \
    --condition "not __fish_seen_subcommand_from $commands" \
    --arguments "$commands"

for cmd in $commands
    complete -c ibmcloud --no-files \
        --condition "__fish_seen_subcommand_from $cmd" \
        --arguments "(ibmcloud $cmd --generate-bash-completion)"
end
