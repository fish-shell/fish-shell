# Completions for gresource (a part of GLib)

# Global options
complete -f -c gresource -l section -d "Select which section to operate on"

# Commands
complete -f -c gresource -n __fish_use_subcommand -a list -d "List resource sections"
complete -f -c gresource -n __fish_use_subcommand -a details -d "List resources with details"
complete -f -c gresource -n __fish_use_subcommand -a extract -d "Extract a resource"
complete -f -c gresource -n "__fish_not_contain_opt section && __fish_use_subcommand" -a sections -d "List resource sections"
complete -f -c gresource -n "__fish_not_contain_opt section && __fish_use_subcommand" -a help -d "Prints help"

# Arguments of help command
complete -f -c gresource -n "__fish_seen_subcommand_from help" -a "list details extract sections help" -d Command
