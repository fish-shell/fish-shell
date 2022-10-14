set -l commands allow permit grant block deny revoke edit exec fetchurl help hook prune reload status stdlib version

set -l with_file allow permit grant block deny revoke edit

complete -c direnv -f

complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a "allow permit grant" -d "Grants direnv permission to load the given .envrc or .env file"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a "block deny revoke" -d "Revokes the authorization of a given .envrc or .env file"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a edit -d "Opens PATH_TO_RC or the current .envrc or .env into an \$EDITOR and allow the file to be loaded afterwards"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a exec -d "Executes a command after loading the first .envrc or .env found in DIR"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a fetchurl -d "Fetches a given URL into direnv's CAS"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a help -d "show help"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a hook -d "Used to setup the shell hook"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a prune -d "removes old allowed files"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a reload -d "triggers an env reload"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a status -d "prints some debug status information"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a stdlib -d "Displays the stdlib available in the .envrc execution context"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a version -d "prints the version or checks that direnv is VERSION_AT_LEAST or newer"


complete -c direnv -n "__fish_seen_subcommand_from $with_file" -F
