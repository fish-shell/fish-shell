## Global Opts
complete -c emaint -s h -l help         -d "show this help message and exit"
complete -c emaint -s c -l check        -d "Check for problems (a default option for most modules)"
complete -c emaint      -l version      -d "show program's version number and exit"
complete -c emaint -s f -l fix          -d "Attempt to fix problems (a default option for most modules)"
complete -c emaint -s P -l purge        -d "Removes the list of previously failed merges. WARNING: Only use this option if you plan on manually fixing them or do not want them re-installed"

## Subcommands
complete -c emaint -n '__fish_use_subcommand' -xa 'all'         -d 'Perform all supported commands'
complete -c emaint -n '__fish_use_subcommand' -xa 'binhost'     -d 'Scan and generate metadata indexes for binary packages'
complete -c emaint -n '__fish_use_subcommand' -xa 'cleanconfmem' -d 'Check and clean the config tracker list for uninstalled packages'
complete -c emaint -n '__fish_use_subcommand' -xa 'cleanresume' -d 'Discard emerge --resume merge lists'
complete -c emaint -n '__fish_use_subcommand' -xa 'logs'        -d 'Check and clean old logs in the PORT_LOGDIR'
complete -c emaint -n '__fish_use_subcommand' -xa 'merges'      -d 'Scan for failed merges and fix them'
complete -c emaint -n '__fish_use_subcommand' -xa 'movebin'     -d 'Perform package move updates for binary packages'
complete -c emaint -n '__fish_use_subcommand' -xa 'moveinst'    -d 'Perform package move updates for installed and binary packages'
complete -c emaint -n '__fish_use_subcommand' -xa 'sync'        -d 'Check repos.conf settings and sync repositories'
complete -c emaint -n '__fish_use_subcommand' -xa 'world'       -d 'Check and fix problems in the world file'

## Local opts
# cleanlogs
complete -c emaint -n '__fish_seen_subcommand_from cleanlogs' -s t -l time      -d "Delete logs older than NUM of days" \
	-xa ""
complete -c emaint -n '__fish_seen_subcommand_from cleanlogs' -s p -l pretend   -d "Output logs that would be deleted"
complete -c emaint -n '__fish_seen_subcommand_from cleanlogs' -s C -l clean     -d "Cleans out logs more than 7 days old"
# sync
complete -c emaint -n '__fish_seen_subcommand_from sync' -s a -l auto          	-d "Sync auto-sync enabled repos only"
complete -c emaint -n '__fish_seen_subcommand_from sync' -s A -l allrepos      	-d "Sync all repos that have a sync-url defined"
complete -c emaint -n '__fish_seen_subcommand_from sync' -s r -l repo          	-d "Sync the specified repo" \
	-xa "(__fish_portage_print_repository_names)"
complete -c emaint -n '__fish_seen_subcommand_from sync'      -l sync-submodule -d "Restrict sync to the specified submodule(s)" \
	-xa "glsa news profiles"
