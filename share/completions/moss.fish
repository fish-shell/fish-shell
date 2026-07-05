function __fish_moss_available_packages
    moss la 2>/dev/null | string replace -rf '^([^ ]+) +[^ ]+ - ' '$1\t'
end

function __fish_moss_installed_packages
    moss li 2>/dev/null | string replace -rf '^([^ ]+) +[^ ]+ - ' '$1\t'
end

set -l commands boot cache extract fetch fe index ix info inspect install it list li la ls lu remove rm repo ar lr rr ur er dr search sr search-file sf state sync up version help

complete -c moss -f
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a boot -d 'Boot management'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a cache -d 'Manage cached data'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a extract -d 'Extract a `.stone` content to disk'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'fetch fe' -d 'Fetch package(s)'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'index ix' -d 'Index a collection of packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a info -d 'Query packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a inspect -d 'Examine raw stone files'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'install it' -d 'Install packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a list -d 'List packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a li -d 'List all installed packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a la -d 'List all available packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'ls lu' -d 'List packages with sync changes'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'remove rm' -d 'Remove packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a repo -d 'Manage software repositories'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a ar -d 'Add a repository for the system'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a lr -d 'List system software repositories'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a rr -d 'Remove a repository for the system'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a ur -d 'Update the system repositories'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a er -d 'Enable the system repositories'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a dr -d 'Disable the system repositories'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'search sr' -d 'Search packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'search-file sf' -d 'Search files'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a state -d 'Manage state'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a 'sync up' -d 'Sync packages'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a version -d 'Display version and exit'
complete -c moss -n "not __fish_seen_subcommand_from $commands" -a help -d 'Print this message or the help of the given subcommand(s)'

complete -c moss -s v -l verbose -d 'Prints additional information about what moss is doing'
complete -c moss -s V -l version -d 'Prints version information about the binary'
complete -c moss -s D -l directory -d 'Root directory'
complete -c moss -l cache -d 'Cache directory'
complete -c moss -l log -d 'Logging configuration'
complete -c moss -s y -l yes-all -d 'Assume yes for all questions'
complete -c moss -n 'not __fish_seen_subcommand_from help' -s h -l help -d 'Print help'

complete -c moss -n "__fish_seen_subcommand_from boot" -a status -d 'Status of boot configuration'
complete -c moss -n "__fish_seen_subcommand_from boot" -a sync -d 'Synchronize boot configuration'
complete -c moss -n "__fish_seen_subcommand_from boot" -a help -d 'Print this message or the help of the given subcommand(s)'

complete -c moss -n "__fish_seen_subcommand_from cache" -a prune -d 'Prune cached artefacts'
complete -c moss -n "__fish_seen_subcommand_from cache" -a help -d 'Print this message or the help of the given subcommand(s)'

complete -c moss -n '__fish_seen_subcommand_from extract' -F
complete -c moss -n '__fish_seen_subcommand_from extract' -s o -l output-dir -r -F -d 'Directory to extract the stone(s) to'

complete -c moss -n '__fish_seen_subcommand_from fetch fe' -a '(__fish_moss_available_packages)'
complete -c moss -n '__fish_seen_subcommand_from fetch fe' -s o -l output-dir -r -F -d 'Directory to extract the stone(s) to'

complete -c moss -n '__fish_seen_subcommand_from index ix' -F
complete -c moss -n '__fish_seen_subcommand_from index ix' -s o -l output-dir -r -F -d 'Directory to write the stone.index'

complete -c moss -n '__fish_seen_subcommand_from info' -a '(__fish_moss_available_packages)'
complete -c moss -n '__fish_seen_subcommand_from info' -s f -l files -d 'Show files provided by package'

complete -c moss -n '__fish_seen_subcommand_from inspect' -F
complete -c moss -n '__fish_seen_subcommand_from inspect' -l check -d 'Check the integrity of the stone file(s)'
complete -c moss -n '__fish_seen_subcommand_from inspect' -s q -l quiet -d 'Suppress output, only exit status indicates success or failure (requires --check)'

complete -c moss -n '__fish_seen_subcommand_from install it' -a '(__fish_moss_available_packages)'
complete -c moss -n '__fish_seen_subcommand_from install it' -l dry-run -d 'Simulate the operation (dry-run)'
complete -c moss -n '__fish_seen_subcommand_from install it' -l to -r -F -d 'Blit this sync to the provided directory instead of the root'

complete -c moss -n "__fish_seen_subcommand_from list" -a installed -d 'List all installed packages'
complete -c moss -n "__fish_seen_subcommand_from list; and __fish_seen_subcommand_from installed; or __fish_seen_subcommand_from li" -s e -l explicit -d 'List explicit packages only'
complete -c moss -n "__fish_seen_subcommand_from list" -a available -d 'List all available packages'
complete -c moss -n "__fish_seen_subcommand_from list" -a sync -d 'List packages with sync changes'
complete -c moss -n "__fish_seen_subcommand_from list; and __fish_seen_subcommand_from sync; or __fish_seen_subcommand_from ls" -l upgrade-only -d 'Only sync packages that have a version upgrade'

complete -c moss -n "__fish_seen_subcommand_from remove rm" -a '(__fish_moss_installed_packages)'
complete -c moss -n '__fish_seen_subcommand_from remove rm' -l dry-run -d 'Simulate the operation (dry-run)'

complete -c moss -n "__fish_seen_subcommand_from repo" -a add -d 'Add a repository for the system'
complete -c moss -n "__fish_seen_subcommand_from repo; and __fish_seen_subcommand_from add; or __fish_seen_subcommand_from ar" -s c -r -d 'Set the comment for the repository'
complete -c moss -n "__fish_seen_subcommand_from repo; and __fish_seen_subcommand_from add; or __fish_seen_subcommand_from ar" -s p -r -d 'Repository priority'
complete -c moss -n "__fish_seen_subcommand_from repo" -a list -d 'List system software repositories'
complete -c moss -n "__fish_seen_subcommand_from repo" -a remove -d 'Remove a repository for the system'
complete -c moss -n "__fish_seen_subcommand_from repo" -a update -d 'Update the system repositories'
complete -c moss -n "__fish_seen_subcommand_from repo" -a enable -d 'Enable the system repositories'
complete -c moss -n "__fish_seen_subcommand_from repo" -a disable -d 'Disable the system repositories'
complete -c moss -n "__fish_seen_subcommand_from repo" -a help -d 'Print this message or the help of the given subcommand(s)'

complete -c moss -n '__fish_seen_subcommand_from search sr' -s i -l installed -d 'Search among installed packages only'
complete -c moss -n '__fish_seen_subcommand_from search sr' -s p -l provides -r -a 'library name soname pkgconfig interpreter cmake python binary sysbinary pkgconfig32' -f -d 'Search for packages by provider'

complete -c moss -n "__fish_seen_subcommand_from search-file sf" -F

complete -c moss -n "__fish_seen_subcommand_from state" -a active -d 'List the active state'
complete -c moss -n "__fish_seen_subcommand_from state" -a list -d 'List all states'
complete -c moss -n "__fish_seen_subcommand_from state" -a activate -d 'Activate a state'
complete -c moss -n "__fish_seen_subcommand_from state; and __fish_seen_subcommand_from activate" -l skip-triggers -d 'Do not run triggers on activation'
complete -c moss -n "__fish_seen_subcommand_from state; and __fish_seen_subcommand_from activate" -l skip-boot -d 'Do not sync boot on activation'
complete -c moss -n "__fish_seen_subcommand_from state" -a query -d 'Query information for a state'
complete -c moss -n "__fish_seen_subcommand_from state" -a prune -d 'Prune archived states'
complete -c moss -n "__fish_seen_subcommand_from state; and __fish_seen_subcommand_from prune" -s k -l keep -d 'Keep this many states'
complete -c moss -n "__fish_seen_subcommand_from state; and __fish_seen_subcommand_from prune" -l include-newer -d 'Include states newer than the active state when pruning'
complete -c moss -n "__fish_seen_subcommand_from state" -a remove -d 'Remove archived state(s)'
complete -c moss -n "__fish_seen_subcommand_from state" -a verify -d 'Verify and fix system states and assets'
complete -c moss -n "__fish_seen_subcommand_from state" -a export -d 'Export a state as a system-model.kdl file'
complete -c moss -n "__fish_seen_subcommand_from state; and __fish_seen_subcommand_from export" -s o -l output -r -f -d 'Export to the provided path'
complete -c moss -n "__fish_seen_subcommand_from state" -a help -d 'Print this message or the help of the given subcommand(s)'

complete -c moss -n "__fish_seen_subcommand_from sync up" -s u -l update -d 'Update repositories before syncing'
complete -c moss -n '__fish_seen_subcommand_from sync up' -l to -r -F -d 'Blit this sync to the provided directory instead of the root'
complete -c moss -n '__fish_seen_subcommand_from sync up' -l dry-run -d 'Simulate the sync (dry-run)'
complete -c moss -n '__fish_seen_subcommand_from sync up' -l import -r -f -d 'Sync against the provided system-model.kdl'
