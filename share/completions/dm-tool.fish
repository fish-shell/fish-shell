# Completions for the dm-tool command (part of the lightdm display manager)

set -l cmds 'add-local-x-seat add-nested-seat add-seat list-seats lock switch-to-greeter switch-to-guest switch-to-user'

complete -c dm-tool -f
complete -c dm-tool -n "not __fish_seen_subcommand_from $cmds" -s h -l help -x -d "Show help options"
complete -c dm-tool -n "not __fish_seen_subcommand_from $cmds" -s v -l version -x -d "Show release version"
complete -c dm-tool -n "not __fish_seen_subcommand_from $cmds" -l session-bus -d "Connect using the session bus"

complete -c dm-tool -n "not __fish_seen_subcommand_from $cmds" -xa "$cmds"

# switch-to-user
set -l session_users "(dm-tool list-seats | string replace -rf '.*UserName=' '' | string trim -c '\'')"
set -l has_user "__fish_seen_subcommand_from $session_users"
complete -c dm-tool -n "__fish_seen_subcommand_from switch-to-user; and not $has_user" -xa "$session_users"
