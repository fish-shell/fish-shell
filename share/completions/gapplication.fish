# Completions for gapplication (a part of GLib)

# Commands
complete -f -c gapplication -n __fish_use_subcommand -a help -d "Print help"
complete -f -c gapplication -n __fish_use_subcommand -a version -d "Print version"
complete -f -c gapplication -n __fish_use_subcommand -a list-apps -d "List applications"
complete -f -c gapplication -n __fish_use_subcommand -a launch -d "Launch an application"
complete -f -c gapplication -n __fish_use_subcommand -a list-actions -d "List available actions"
complete -f -c gapplication -n __fish_use_subcommand -a action -d "Activate an action"

# Arguments of help command
complete -f -c gapplication -n "__fish_seen_subcommand_from help" -a "help version list-apps launch list-actions action" -d Command
