## Global Opts
complete -c emaint -s h -l help         -d "show this help message and exit"
complete -c emaint -s c -l check        -d "check for problems"
complete -c emaint      -l version      -d "show program's version number and exit"
complete -c emaint -s f -l fix          -d "attempt to fix problems"
complete -c emaint -s P -l purge        -d "remove the list of failed merges"

## Subcommands
complete -c emaint -n '__fish_use_subcommand' -xa 'all'         -d 'perform all supported commands'
complete -c emaint -n '__fish_use_subcommand' -xa 'binhost'     -d 'scan and generate metadata indexes for binary pkgs'
complete -c emaint -n '__fish_use_subcommand' -xa 'cleanconfmem' -d 'check and clean the config tracker list for uninstalled pkgs'
complete -c emaint -n '__fish_use_subcommand' -xa 'cleanresume' -d 'discard emerge --resume merge lists'
complete -c emaint -n '__fish_use_subcommand' -xa 'logs'        -d 'check and clean old logs in the PORT_LOGDIR'
complete -c emaint -n '__fish_use_subcommand' -xa 'merges'      -d 'scan for failed merges and fix them'
complete -c emaint -n '__fish_use_subcommand' -xa 'movebin'     -d 'perform pkg move updates for binary pkgs'
complete -c emaint -n '__fish_use_subcommand' -xa 'moveinst'    -d 'perform pkg move updates for installed and binary pkgs'
complete -c emaint -n '__fish_use_subcommand' -xa 'sync'        -d 'check repos.conf settings and sync repositories'
complete -c emaint -n '__fish_use_subcommand' -xa 'world'       -d 'check and fix problems in the world file'

## Local opts
# cleanlogs
complete -c emaint -n '__fish_seen_subcommand_from cleanlogs' -s t -l time      -d "delete logs older than NUM of days" \
	-xa "(seq 0 365)"
complete -c emaint -n '__fish_seen_subcommand_from cleanlogs' -s p -l pretend   -d "output logs that would be deleted"
complete -c emaint -n '__fish_seen_subcommand_from cleanlogs' -s C -l clean     -d "cleans out logs more than 7 days old"
# sync
complete -c emaint -n '__fish_seen_subcommand_from sync' -s a -l auto          	-d "sync auto-sync enabled repos only"
complete -c emaint -n '__fish_seen_subcommand_from sync' -s A -l allrepos      	-d "sync all repos that have a sync-url defined"
complete -c emaint -n '__fish_seen_subcommand_from sync' -s r -l repo          	-d "sync the specified repo" \
	-xa "(__fish_portage_print_repository_names)"
complete -c emaint -n '__fish_seen_subcommand_from sync'      -l sync-submodule -d "restrict sync to the specified submodule(s)" \
	-xa "glsa news profiles"
