
# First parameter is the profile name, or 'usage'
complete --command duply --no-files --condition '__fish_is_first_token' --arguments '(/bin/ls /etc/duply 2>/dev/null) (/bin/ls ~/.duply 2>/dev/null)' -d 'Profile'
complete --command duply --no-files --arguments 'usage' -d 'Get usage help text'

# Second parameter is a duply command
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'create'     -d 'Creates a configuration profile'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'backup'     -d 'Backup with pre/post script execution'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'bkp'        -d 'Backup without executing pre/post scripts'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'pre'        -d 'Execute <profile>/pre script'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'post'       -d 'Execute <profile>/post script'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'full'       -d 'Force full backup'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'incr'       -d 'Force incremental backup'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'list'       -d 'List all files in backup (as it was at <age>, default: now)'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'status'     -d 'Prints backup sets and chains currently in repository'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'verify'     -d 'List files changed since latest backup'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'purge'      -d 'Shows outdated backup archives [--force, delete these files]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'purge-full' -d 'Shows outdated backups [--force, delete these files]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'cleanup'    -d 'Shows broken backup archives [--force, delete these files]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'restore'    -d 'Restore the backup to <target_path> [as it was at <age>]'
complete --command duply --no-files --condition 'not __fish_is_first_token' --arguments 'fetch'      -d 'Restore single file/folder from backup [as it was at <age>]'

# Options
complete --command duply --no-files --long-option force   -d 'Really execute the commands: purge, purge-full, cleanup'
complete --command duply --no-files --long-option preview -d 'Do nothing but print out generated duplicity command lines'
complete --command duply --no-files --long-option dry-run -d 'Calculate what would be done, but don''t perform any actions'
complete --command duply --no-files --long-option allow-source-mismatch -d 'Don''t abort when backup different dirs to the same backend'
complete --command duply --no-files --long-option verbosity --arguments '0 2 4 8 9' -d 'Output verbosity level'

