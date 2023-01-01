# Subcommands
complete -f -n __fish_use_subcommand -c mariner -a complete -d "Print bash completion command"
complete -f -n __fish_use_subcommand -c mariner -a config -d "Show or update configuration"
complete -f -n __fish_use_subcommand -c mariner -a details -d "Show details about torrent with given ID"
complete -f -n __fish_use_subcommand -c mariner -a download -d "Download torrent with given ID"
complete -f -n __fish_use_subcommand -c mariner -a help -d "Print detailed help for another command"
complete -f -n __fish_use_subcommand -c mariner -a magnet -d "Copy magnet link with given ID to clipboard"
complete -f -n __fish_use_subcommand -c mariner -a open -d "Open torrent in the default torrent application"
complete -f -n __fish_use_subcommand -c mariner -a search -d "Search for torrents"

# Global options
complete -f -n __fish_use_subcommand -c mariner -l version -d "Show program's version number and exit"
complete -f -n __fish_use_subcommand -c mariner -s v -l verbose -d "Increase verbosity of output. Can be repeated"
complete -f -n __fish_use_subcommand -c mariner -s q -l quiet -d "Suppress output except warnings and errors"
complete -f -n __fish_use_subcommand -c mariner -s h -l help -d "Show help message and exit"
complete -f -n __fish_use_subcommand -c mariner -l debug -d "Show tracebacks on errors"
complete -r -n __fish_use_subcommand -c mariner -l log-file -d "Specify a file to log output. Default ~/.local/share/mariner/mariner.log"
complete -r -n __fish_use_subcommand -c mariner -l config-file -d "Path to config file. Default ~/.config/mariner/config.yaml"

# Config options
complete -f -n "__fish_seen_subcommand_from config" -c mariner -s s -l show -d "Show the configuration"

# Download options
complete -r -n "__fish_seen_subcommand_from download" -c mariner -s p -l path -d "Path to download torrent files to"

# Search options
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s a -l all -d "Search all available trackers"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s A -l anime -d "Search trackers with anime content only"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s f -l filter -a "anime legal" -d "Filter trackers that should be searched"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s L -l legal -d "Search trackers with legal content only"
complete -x -n "__fish_seen_subcommand_from search" -c mariner -s l -l limit -d "Limit the number of results shown. Default is 50"
complete -f -n "__fish_seen_subcommand_from search" -c mariner -s n -l newest -d "Sort results by newest"
complete -x -n "__fish_seen_subcommand_from search" -c mariner -s t -l trackers -d "Trackers that should be searched"
