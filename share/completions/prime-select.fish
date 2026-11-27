complete -c prime-select -f

set -l subcommands nvidia intel on-demand query

complete -c prime-select -n "not __fish_seen_subcommand_from $subcommands" -a nvidia -d "Switch to the NVIDIA GPU"
complete -c prime-select -n "not __fish_seen_subcommand_from $subcommands" -a intel -d "Switch to the integrated GPU"
complete -c prime-select -n "not __fish_seen_subcommand_from $subcommands" -a on-demand -d "Load NVIDIA driver, use it on demand"
complete -c prime-select -n "not __fish_seen_subcommand_from $subcommands" -a query -d "Print the currently active profile"
