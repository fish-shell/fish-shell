# Filter Completion
function __fish_btrbk_complete_filter
    btrbk list config --format col:h:snapshot_name,source_subvolume,target_url | string replace -r '([^ ]+)\s+([^ ]+)\s+([^ ]*)' '$1\t$2 -> $3'
end

# options with arguments
complete -c btrbk -l format        -x -d 'Change output format'                     -a "table long raw"
complete -c btrbk -l loglevel -s l -x -d 'Set logging level'                        -a "error warn info debug trace"
complete -c btrbk -l exclude       -x -d 'Exclude configured sections'              -a "(__fish_btrbk_complete_filter)"
complete -c btrbk -l override      -x -d 'Globally override a configuration option'

# options with file completion
complete -c btrbk -l config          -r -s c -d 'Specify configuration file'
complete -c btrbk -l lockfile        -r      -d 'Create and check lockfile'

# options without arguments
complete -c btrbk -l help               -s h -d 'Display this help message'
complete -c btrbk -l version                 -d 'Display version information'
complete -c btrbk -l dry-run            -s n -d 'Perform a trial run with no changes made'
complete -c btrbk -l preserve           -s p -d 'Preserve all (do not delete anything)'
complete -c btrbk -l preserve-snapshots      -d 'Preserve snapshots (do not delete snapshots)'
complete -c btrbk -l preserve-backups        -d 'Preserve backups (do not delete backups)'
complete -c btrbk -l wipe                    -d 'Delete all but latest snapshots'
complete -c btrbk -l verbose            -s v -d 'Be more verbose (increase logging level)'
complete -c btrbk -l quiet              -s q -d 'Be quiet (do not print backup summary)'
complete -c btrbk -l table              -s t -d 'Change output to table format'
complete -c btrbk -l long               -s L -d 'Change output to long format'
complete -c btrbk -l print-schedule     -s S -d 'Print scheduler details (for the "run" command)'
complete -c btrbk -l progress                -d 'Show progress bar on send-receive operation'

# uncommon options from manpage
complete -c btrbk -l single-column -s 1 -d 'Print output as a single column'
complete -c btrbk -l pretty             -d 'Print pretty table output with lowercase and underlined column headings'
complete -c btrbk -l raw                -d 'Create raw targets for archive command'

# subcommands
complete -c btrbk -f -n __fish_use_subcommand -a run      -d 'Run snapshot and backup operations'
complete -c btrbk -f -n __fish_use_subcommand -a dryrun   -d 'Show what would be executed without running btrfs commands'
complete -c btrbk -f -n __fish_use_subcommand -a snapshot -d 'Run snapshot operations only'
complete -c btrbk -f -n __fish_use_subcommand -a resume   -d 'Run backup operations and delete snapshots'
complete -c btrbk -f -n __fish_use_subcommand -a prune    -d 'Only delete snapshots and backups'
complete -c btrbk -f -n __fish_use_subcommand -a archive  -d 'Recursively copy all subvolumes (src -> dst)'
complete -c btrbk -f -n __fish_use_subcommand -a clean    -d 'Delete incomplete (garbled) backups'
complete -c btrbk -f -n __fish_use_subcommand -a stats    -d 'Print snapshot/backup statistics'
complete -c btrbk -f -n __fish_use_subcommand -a usage    -d 'Print filesystem usage'
complete -c btrbk -f -n __fish_use_subcommand -a ls       -d 'List all btrfs subvolumes below a given path'
complete -c btrbk -f -n __fish_use_subcommand -a origin   -d 'Print origin information for a subvolume'
complete -c btrbk -f -n __fish_use_subcommand -a diff     -d 'List file changes between related subvolumes'
complete -c btrbk -f -n __fish_use_subcommand -a extents  -d 'Calculate accurate disk space usage for a path'
complete -c btrbk -f -n __fish_use_subcommand -a list     -d 'List snapshots and backups'

# subsubcommands for "list"
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a all       -d 'List all snapshots and backups'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a snapshots -d 'List snapshots only'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a backups   -d 'List backups and correlated snapshots'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a latest    -d 'List most recent snapshots and backups'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a config    -d 'List configured source/snapshot/target relations'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a source    -d 'List configured source/snapshot relations'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a volume    -d 'List configured volume sections'
complete -c btrbk -n '__fish_seen_subcommand_from list' -f -a target    -d 'List configured targets'

complete -c btrbk -n '__fish_seen_subcommand_from run dryrun snapshot resume prune clean' -x -a '(__fish_btrbk_complete_filter)'
