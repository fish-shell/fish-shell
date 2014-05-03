
# First parameter is the profile name, or 'usage'
complete --command duply --no-files --condition '__fish_is_first_token' --arguments '(/bin/ls /etc/duply ^/dev/null) (/bin/ls ~/.duply ^/dev/null)' --description 'Profile'
complete --command duply --no-files --arguments 'usage' --description 'Get usage help text'

# Second parameter is a duply command
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'create'     --description 'Creates a configuration profile'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'backup'     --description 'Backup with pre/post script execution'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'bkp'        --description 'Backup without executing pre/post scripts'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'pre'        --description 'Execute <profile>/pre script'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'post'       --description 'Execute <profile>/post script'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'full'       --description 'Force full backup'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'incr'       --description 'Force incremental backup'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'list'       --description 'List all files in backup (as it was at <age>, default: now)'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'status'     --description 'Prints backup sets and chains currently in repository'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'verify'     --description 'List files changed since latest backup'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'purge'      --description 'Shows outdated backup archives [--force, delete these files]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'purge-full' --description 'Shows outdated backups [--force, delete these files]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'cleanup'    --description 'Shows broken backup archives [--force, delete these files]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'restore'    --description 'Restore the backup to <target_path> [as it was at <age>]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'fetch'      --description 'Restore single file/folder from backup [as it was at <age>]'

# Options
complete --command duply --no-files --long-option force   --description 'Really execute the commands: purge, purge-full, cleanup'
complete --command duply --no-files --long-option preview --description 'Do nothing but print out generated duplicity command lines'
complete --command duply --no-files --long-option dry-run --description 'Calculate what would be done, but don''t perform any actions'
complete --command duply --no-files --long-option allow-source-mismatch --description 'Don''t abort when backup different dirs to the same backend'
complete --command duply --no-files --long-option verbosity --arguments '0 2 4 8 9' --description 'Output verbosity level'

