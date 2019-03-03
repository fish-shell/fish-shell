set -l subcommands complete config details download help magnet open search

# Subcommands
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a complete --description "Print bash completion command"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a config --description "Show or update configuration"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a details --description "Show details about torrent with given ID"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a download --description "Download torrent with given ID"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a help --description "Print detailed help for another command"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a magnet --description "Copy magnet link with given ID to clipboard"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a open --description "Open torrent in the default torrent application"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -a search --description "Search for torrents"

# Global options
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -l version --description "Show program's version number and exit"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -s v -l verbose --description "Increase verbosity of output. Can be repeated"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -s q -l quiet --description "Suppress output except warnings and errors"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -s h -l help --description "Show help message and exit"
complete -f -n "__fish_use_subcommand $subcommands" -c mariner -l debug --description "Show tracebacks on errors"
complete -r -n "__fish_use_subcommand $subcommands" -c mariner -l log-file --description "Specify a file to log output. Default ~/.local/share/mariner/mariner.log"
complete -r -n "__fish_use_subcommand $subcommands" -c mariner -l config-file --description "Path to config file. Default ~/.config/mariner/config.yaml"

# Config options
complete -f -n "__fish_seen_subcommand_from config" -c mariner -s s -l show --description "Show the configuration"

# Download options
complete -r -n "__fish_seen_subcommand_from download" -c mariner -s p -l path --description "Path to download torrent files to"

# Search options
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s a -l all --description "Search all available trackers"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s A -l anime --description "Search trackers with anime content only"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s f -l filter -a "anime legal" --description "Filter trackers that should be searched"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s L -l legal --description "Search trackers with legal content only"
complete -x -n "__fish_seen_subcommand_from search" -c mariner -s l -l limit --description "Limit the number of results shown. Default is 50"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s n -l newest --description "Sort results by newest"
complete -x -n "__fish_seen_subcommand_from search" -c mariner -s t -l trackers --description "Trackers that should be searched"
