set -l commands allow permit grant block deny revoke edit exec fetchurl help hook prune reload status stdlib version

set -l with_file allow permit grant block deny revoke edit

complete -c direnv -f

complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a "allow permit grant" -d "Grant direnv permission to load the given .envrc or .env file"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a "block deny revoke" -d "Revoke the authorization of a given .envrc or .env file"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a edit -d "Open given file or current .envrc/.env in \$EDITOR and allow it to be loaded afterwards"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a exec -d "Execute a command after loading the first .envrc or .env found in DIR"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a fetchurl -d "Fetch a given URL into direnv's CAS"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a help -d "show help"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a hook -d "Used to setup the shell hook"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a prune -d "Remove old allowed files"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a reload -d "Trigger an env reload"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a status -d "Print some debug status information"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a stdlib -d "Display the stdlib available in the .envrc execution context"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a version -d "Print the version or check that direnv is at least given version"

complete -c direnv -n "__fish_seen_subcommand_from $with_file" -F
