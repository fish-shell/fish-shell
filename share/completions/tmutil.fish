# completion for tmutil (macOS)

function __fish_tmutil_destination_ids
    for line in $(tmutil destinationinfo)
        if string match -q '*===*' -- $line
            # New section so clear out variables
            set -f name ''
            set -f kind ''
            continue
        end

        if string match -q -r '^Name' -- $line
            # Got the destination name
            set -f name "$(string match -r -g '^Name\s+:\s+(.*)$' $line | string trim)"
            continue
        end

        if string match -q -r '^Kind' -- $line
            # Got the destination name
            set -f kind "$(string match -r -g '^Kind\s+:\s+(\w+)' $line | string trim)"
            continue
        end

        if string match -q -r '^ID' -- $line
            # Got the destination ID
            set -f ID "$(string match -r -g '^ID\s+:\s+(.*)$' $line | string trim)"
            echo $ID\t$name $kind
            continue
        end
    end
end

complete -c tmutil -n __fish_use_subcommand -a status -d 'Display Time Machine status'
complete -c tmutil -f -n '__fish_seen_subcommand_from status'

complete -c tmutil -f -n __fish_use_subcommand -a addexclusion -d 'Add an exclusion not to back up a file'
complete -c tmutil -r -n '__fish_seen_subcommand_from addexclusion' -s v -d 'Volume exclusion'
complete -c tmutil -r -n '__fish_seen_subcommand_from addexclusion' -s p -d 'Path exclusion'

complete -c tmutil -r -n __fish_use_subcommand -a associatedisk -d 'Bind a snapshot volume directory to the specified local disk'

complete -c tmutil -r -n __fish_use_subcommand -a calculatedrift -d 'Determine the amount of change between snapshots'

complete -c tmutil -r -n __fish_use_subcommand -a compare -d 'Perform a backup diff'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s a -d 'Compare all supported metadata'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s n -d 'No metadata comparison'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s @ -d 'Compare extended attributes'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s c -d 'Compare creation times'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s d -d 'Compare file data forks'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s e -d 'Compare ACLs'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s f -d 'Compare file flags'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s g -d 'Compare GIDs'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s m -d 'Compare file modes'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s s -d 'Compare sizes'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s t -d 'Compare modification times'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s u -d 'Compare UIDs'
complete -c tmutil -r -n '__fish_seen_subcommand_from compare' -s D -d 'Limit traversal depth to depth levels from the beginning of iteration'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s E -d 'Dont take exclusions into account'
complete -c tmutil -r -n '__fish_seen_subcommand_from compare' -s I -d 'Ignore path'
complete -c tmutil -f -n '__fish_seen_subcommand_from compare' -s U -d 'Ignore logical volume identity'

complete -c tmutil -r -n __fish_use_subcommand -a delete -d 'Delete one or more snapshots'
complete -c tmutil -r -n '__fish_seen_subcommand_from delete' -s d -d 'Backup mount point'
complete -c tmutil -r -f -n '__fish_seen_subcommand_from delete' -s t -d Timestamp
complete -c tmutil -r -n '__fish_seen_subcommand_from delete' -s p -d Path

complete -c tmutil -r -n __fish_use_subcommand -a deletelocalsnapshots -d 'Delete all local Time Machine snapshots for the specified date (formatted YYYY-MM-DD-HHMMSS)'

complete -c tmutil -r -n __fish_use_subcommand -a deleteinprogress -d 'Delete all in-progress backups for a machine directory'

complete -c tmutil -f -n __fish_use_subcommand -a destinationinfo -d 'Print information about destinations'

complete -c tmutil -f -n __fish_use_subcommand -a disable -d 'Turn off automatic backups'

complete -c tmutil -f -n __fish_use_subcommand -a disablelocal -d 'Turn off local Time Machine snapshots'

complete -c tmutil -f -n __fish_use_subcommand -a enable -d 'Turn on automatic backups'

complete -c tmutil -f -n __fish_use_subcommand -a enablelocal -d 'Turn on local Time Machine snapshots'

complete -c tmutil -r -n __fish_use_subcommand -a inheritbackup -d 'Claim a machine directory or sparsebundle for use by the current machine'

complete -c tmutil -r -n __fish_use_subcommand -a isexcluded -d 'Determine if a file, directory, or volume are excluded from backups'

complete -c tmutil -f -n __fish_use_subcommand -a latestbackup -d 'Print the path to the latest snapshot'

complete -c tmutil -f -n __fish_use_subcommand -a listlocalsnapshotdates -d 'List the creation dates of all local Time Machine snapshots'

complete -c tmutil -r -n __fish_use_subcommand -a listlocalsnapshots -d 'List local Time Machine snapshots of the specified volume'

complete -c tmutil -f -n __fish_use_subcommand -a listbackups -d 'Print paths for all snapshots'

complete -c tmutil -f -n __fish_use_subcommand -a localsnapshot -d 'Create new local Time Machine snapshot of APFS volume in TM backup'

complete -c tmutil -f -n __fish_use_subcommand -a machinedirectory -d 'Print the path to the current machine directory'

complete -c tmutil -f -n __fish_use_subcommand -a removedestination -d 'Removes a backup destination'
complete -c tmutil -f -n '__fish_seen_subcommand_from removedestination' -a '$(__fish_tmutil_destination_ids)'

complete -c tmutil -f -n __fish_use_subcommand -a removeexclusion -d 'Remove an exclusion'
complete -c tmutil -f -n '__fish_seen_subcommand_from removeexclusion' -s v -d 'Volume exclusion'
complete -c tmutil -f -n '__fish_seen_subcommand_from removeexclusion' -s p -d 'Path exclusion'

complete -c tmutil -f -n __fish_use_subcommand -a restore -d 'Restore an item'
complete -c tmutil -r -n '__fish_seen_subcommand_from restore' -s v

complete -c tmutil -r -n __fish_use_subcommand -a setdestination -d 'Set a backup destination'
complete -c tmutil -f -n '__fish_seen_subcommand_from setdestination' -s a -d 'Add to the list of destinations'
complete -c tmutil -f -n '__fish_seen_subcommand_from setdestination' -s p -d 'Enter the password at a non-echoing interactive prompt'

complete -c tmutil -f -n __fish_use_subcommand -a snapshot -f -d 'Create new local Time Machine snapshot'

complete -c tmutil -f -n __fish_use_subcommand -a startbackup -d 'Begin a backup if one is not already running'
complete -c tmutil -f -n '__fish_seen_subcommand_from startbackup' -s a -l auto -d 'Automatic mode'
complete -c tmutil -f -n '__fish_seen_subcommand_from startbackup' -s b -l block -d 'Block until finished'
complete -c tmutil -f -n '__fish_seen_subcommand_from startbackup' -s r -l rotation -d 'Automatic rotation'
complete -c tmutil -f -n '__fish_seen_subcommand_from startbackup' -s d -l destination -r -a '$(__fish_tmutil_destination_ids)' -d 'Backup destination'

complete -c tmutil -f -n __fish_use_subcommand -a stopbackup -d 'Cancel a backup currently in progress'

complete -c tmutil -r -n __fish_use_subcommand -a thinlocalsnapshots -d 'Thin local Time Machine snapshots for the specified volume'

complete -c tmutil -r -n __fish_use_subcommand -a uniquesize -d 'Analyze the specified path and determine its unique size'

complete -c tmutil -r -n __fish_use_subcommand -a verifychecksums -d 'Verify snapshot'

complete -c tmutil -f -n __fish_use_subcommand -a version -d 'Print version'

complete -c tmutil -f -n __fish_use_subcommand -a setquota -d 'Set the quota for the destination in gigabytes'
complete -c tmutil -f -n '__fish_seen_subcommand_from setquota' -r -a '$(__fish_tmutil_destination_ids)'

complete -c tmutil -f -n '__fish_seen_subcommand_from destinationinfo isexcluded compare status' -s X -d 'Print as XML'
complete -c tmutil -f -n '__fish_seen_subcommand_from latestbackup listbackups' -s m -d 'Destination volume to list backups from'
complete -c tmutil -f -n '__fish_seen_subcommand_from latestbackup listbackups' -s t -d 'Attempt to mount the backups and list their mounted paths'
complete -c tmutil -f -n '__fish_seen_subcommand_from latestbackup listbackups' -s d -d 'Backup mount point'
