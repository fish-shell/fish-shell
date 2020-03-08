function __fish_print_portage_repository_names --description 'Print the names of all configured repositories'
    # repos.conf may be a file or a directory
    find /etc/portage/repos.conf -type f -exec cat '{}' + | string replace -r --filter '^\s*\[([[:alnum:]_][[:alnum:]_-]*)\]' '$1' | string match -v -e DEFAULT
end

## Global Opts
complete -c emaint -s h -l help -d "Show this help message and exit"
complete -c emaint -s c -l check -d "Check for problems"
complete -c emaint -l version -d "Show program's version number and exit"
complete -c emaint -s f -l fix -d "Attempt to fix problems"
complete -c emaint -s P -l purge -d "Remove the list of failed merges"

## Subcommands
complete -c emaint -n __fish_use_subcommand -xa all -d 'Perform all supported commands'
complete -c emaint -n __fish_use_subcommand -xa binhost -d 'Scan and generate metadata indexes for binary pkgs'
complete -c emaint -n __fish_use_subcommand -xa cleanconfmem -d 'Check and clean the config tracker list for uninstalled pkgs'
complete -c emaint -n __fish_use_subcommand -xa cleanresume -d 'Discard emerge --resume merge lists'
complete -c emaint -n __fish_use_subcommand -xa logs -d 'Check and clean old logs in the PORT_LOGDIR'
complete -c emaint -n __fish_use_subcommand -xa merges -d 'Scan for failed merges and fix them'
complete -c emaint -n __fish_use_subcommand -xa movebin -d 'Perform pkg move updates for binary pkgs'
complete -c emaint -n __fish_use_subcommand -xa moveinst -d 'Perform pkg move updates for installed and binary pkgs'
complete -c emaint -n __fish_use_subcommand -xa sync -d 'Check repos.conf settings and sync repositories'
complete -c emaint -n __fish_use_subcommand -xa world -d 'Check and fix problems in the world file'

## Local opts
# logs
complete -c emaint -n '__fish_seen_subcommand_from logs' -s t -l time -d "Delete logs older than NUM days" \
    -xa "(seq 0 365)"
complete -c emaint -n '__fish_seen_subcommand_from logs' -s p -l pretend -d "Output logs that would be deleted"
complete -c emaint -n '__fish_seen_subcommand_from logs' -s C -l clean -d "Cleans out logs more than 7 days old"
# sync
complete -c emaint -n '__fish_seen_subcommand_from sync' -s a -l auto -d "Sync auto-sync enabled repos only"
complete -c emaint -n '__fish_seen_subcommand_from sync' -s A -l allrepos -d "Sync all repos that have a sync-url defined"
complete -c emaint -n '__fish_seen_subcommand_from sync' -s r -l repo -d "Sync the specified repo" \
    -xa "(__fish_print_portage_repository_names)"
complete -c emaint -n '__fish_seen_subcommand_from sync' -l sync-submodule -d "Restrict sync to the specified submodule(s)" \
    -xa "glsa news profiles"
