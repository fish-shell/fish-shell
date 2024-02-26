function __fish_obnam_nosubcommand
    if __fish_seen_subcommand_from add-key backup client-keys clients diff dump-repo force-lock forget fsck \
            generations genids help help-all kdirstat list-errors list-formats list-keys list-toplevels \
            ls mount nagios-last-backup-age remove-client remove-key restore verify
        return 1
    end
    return 0
end

complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a add-key -d 'Add a key to the repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a backup -d 'Backup data to repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a client-keys -d 'List clients and their keys in the repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a clients -d 'List clients using the repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a diff -d 'Show difference between two generations'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a dump-repo -d 'Dump (some) data structures from a repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a force-lock -d 'Force a locked repository to be open'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a forget -d 'Forget (remove) specified backup generations'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a fsck -d 'Verify internal consistency of backup repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a generations -d 'List backup generations for client'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a genids -d 'List generation ids for client'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a help -d 'Print help'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a help-all -d 'Print help, including hidden subcommands'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a kdirstat -d 'List contents of a generation in kdirstat cache format'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a list-errors -d 'List error codes and explanations'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a list-formats -d 'List available repository formats'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a list-keys -d 'List keys and repository toplevels they are used in'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a list-toplevels -d 'List repository toplevel directories and their keys'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a ls -d 'List contents of a generation'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a mount -d 'Mount a backup repository as a FUSE filesystem'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a nagios-last-backup-age -d 'Check if the most recent generation is recent enough'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a remove-client -d 'Remove client and its key from repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a remove-key -d 'Remove a key from the repository'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a restore -d 'Restore some or all files from a generation'
complete -c obnam --no-files --condition __fish_obnam_nosubcommand -a verify -d 'Verify that live data and backed up data match'

complete -c obnam --no-files -l always-restore-setuid -d 'Restore setuid/setgid bits in restored files'
complete -c obnam --no-files -l no-always-restore-setuid -d 'Do not restore setuid/setgid bits in restored files'
complete -c obnam --no-files -l checksum-algorithm -a 'sha512 sha224 sha256 sha384' -d 'Use CHECKSUM algorithm'
complete -c obnam --no-files -l client-name -a '$hostname' -d 'Name of client'
complete -c obnam --no-files -l compress-with -a 'none deflate' -d 'Compress repository with'
complete -c obnam --no-files -l critical-age -d 'For --nagios-last-backup-age: maximum age'
complete -c obnam --no-files -l dump-repo-file-metadata -d 'Dump metadata about files'
complete -c obnam --no-files -l no-dump-repo-file-metadata -d 'Do not dump metadata about files'
complete -c obnam --no-files -l generate-manpage -d 'Generate man page'
complete -c obnam --no-files -l generation -d 'Which generation to restore'
complete -c obnam --no-files -s h -l help -d 'Show this help message and exit'
complete -c obnam --no-files -l keep -d 'Policy for what generations to keep when forgetting.'
complete -c obnam --no-files -l lock-timeout -d 'Wait TIMEOUT seconds for an existing lock'
complete -c obnam -l output -d 'Write output to FILE instead of STDOUT'
complete -c obnam --no-files -l pretend -l dry-run -l no-act -d 'Do not actually change anything'
complete -c obnam --no-files -l no-pretend -l no-dry-run -l no-no-act -d 'Actually commit changes'
complete -c obnam --no-files -l quiet -d 'Show only errors, no progress updates'
complete -c obnam --no-files -l no-quiet -d 'Show errors and progress updates'
complete -c obnam -s r -l repository -d 'Name of backup repository'
complete -c obnam --no-files -l repository-format -a '6 green-albatross-20160813' -d 'Use FORMAT for new repositories'
complete -c obnam -l to -d 'Where to restore / mount to'
complete -c obnam --no-files -l verbose -d 'Be more verbose'
complete -c obnam --no-files -l no-verbose -d 'Do not be verbose'
complete -c obnam --no-files -l verify-randomly -d 'Verify N files randomly from the backup'
complete -c obnam --no-files -l version -d 'Show version number and exit'
complete -c obnam -l warn-age -d 'For nagios-last-backup-age: maximum age'

# Backing up
complete -c obnam --no-files -l checkpoint -d 'Make a checkpoint after a given SIZE'
complete -c obnam --no-files -l deduplicate -a 'fatalist never verify' -d 'Deduplicate mode'
complete -c obnam -l exclude -d 'REGEXP for pathnames to exclude'
complete -c obnam --no-files -l exclude-caches -d 'Exclude directories tagged as cache'
complete -c obnam --no-files -l no-exclude-caches -d 'Include directories tagged as cache'
complete -c obnam -l exclude-from -d 'Read exclude patterns from FILE'
complete -c obnam --no-files -l include -d 'REGEXP for pathnames to include, even if matches --exclude'
complete -c obnam --no-files -l leave-checkpoints -d 'Leave checkpoint generations at the end of backup'
complete -c obnam --no-files -l no-leave-checkpoints -d 'Omit checkpoint generations at the end of backup'
complete -c obnam --no-files -l one-file-system -d 'Do not follow mount points'
complete -c obnam --no-files -l no-one-file-system -d 'Follow mount points'
complete -c obnam -l root -d 'What to backup'
complete -c obnam --no-files -l small-files-in-btree -d 'Put small files directly into the B-tree'
complete -c obnam --no-files -l no-small-files-in-btree -d 'No not put small files into the B-tree'

# Configuration files and settings
complete -c obnam -l config -d 'Add FILE to config files'
complete -c obnam --no-files -l dump-config -d 'Write out the current configuration'
complete -c obnam --no-files -l dump-setting-names -d 'Write out setting names'
complete -c obnam --no-files -l help-all -d 'Show all options'
complete -c obnam --no-files -l list-config-files -d 'List config files'
complete -c obnam --no-files -l no-default-configs -d 'Clear list of configuration files to read'

# Development of Obnam itself
complete -c obnam --no-files -l crash-limit -d 'Artificially crash the program after COUNTER files'
complete -c obnam --no-files -l pretend-time -d 'Pretend it is TIMESTAMP'
complete -c obnam --no-files -l testing-fail-matching -d 'Simulate failures for files that match REGEXP'
complete -c obnam -l trace -d 'FILENAME pattern for trace debugging'

# Encryption
complete -c obnam --no-files -l encrypt-with -d 'PGP key with which to encrypt'
complete -c obnam -l gnupghome -d 'Home directory for GPG'
complete -c obnam --no-files -l key-details -d 'Show additional user IDs'
complete -c obnam --no-files -l no-key-details -d 'Do not show additional user IDs'
complete -c obnam --no-files -l keyid -d 'PGP key id'
complete -c obnam --no-files -l symmetric-key-bits -d 'Size of symmetric key'
complete -c obnam --no-files -l weak-random -d 'Use /dev/urandom instead of /dev/random'
complete -c obnam --no-files -l no-weak-random -d 'Use default /dev/random'

# Integrity checking (fsck)
complete -c obnam --no-files -l fsck-fix -d 'fsck should try to fix problems'
complete -c obnam --no-files -l no-fsck-fix -d 'fsck should not try to fix problems'
complete -c obnam --no-files -l fsck-ignore-chunks -d 'Ignore chunks when checking integrity'
complete -c obnam --no-files -l no-fsck-ignore-chunks -d 'Check chunks when checking integrity'
complete -c obnam --no-files -l fsck-ignore-client -d 'Do not check data for cient NAME'
complete -c obnam --no-files -l fsck-last-generation-only -d 'Check only the last generation'
complete -c obnam --no-files -l no-fsck-last-generation-only -d 'Check all generations'
complete -c obnam --no-files -l fsck-rm-unused -d 'fsck should remove unused chunks'
complete -c obnam --no-files -l no-fsck-rm-unused -d 'fsck should remove unused chunks'
complete -c obnam --no-files -l fsck-skip-checksums -d 'Do not check checksums of files'
complete -c obnam --no-files -l no-fsck-skip-checksums -d 'Check checksums of files'
complete -c obnam --no-files -l fsck-skip-dirs -d 'Do not check directories'
complete -c obnam --no-files -l no-fsck-skip-dirs -d 'Check directories'
complete -c obnam --no-files -l fsck-skip-files -d 'Do not check files'
complete -c obnam --no-files -l no-fsck-skip-files -d 'Check files'
complete -c obnam --no-files -l fsck-skip-generations -d 'Do not check any generations'
complete -c obnam --no-files -l no-fsck-skip-generations -d 'Check all generations'
complete -c obnam --no-files -l fsck-skip-per-client-b-trees -d 'Do not check per-client B-trees'
complete -c obnam --no-files -l no-fsck-skip-per-client-b-trees -d 'Check per-client B-trees'
complete -c obnam --no-files -l fsck-skip-shared-b-trees -d 'Do not check shared B-trees'
complete -c obnam --no-files -l no-fsck-skip-shared-b-trees -d 'Check shared B-trees'

# Logging
complete -c obnam -l log -a syslog -d 'Write log to FILE or syslog'
complete -c obnam --no-files -l log-keep -d 'Keep last N logs (10)'
complete -c obnam --no-files -l log-level -a 'debug info warning error critical fatal' -d 'Log at LEVEL'
complete -c obnam --no-files -l log-max -d 'Rotate logs larger than SIZE'
complete -c obnam --no-files -l log-mode -d 'Set permissions of logfiles to MODE'

# Mounting with FUSE
complete -c obnam --no-files -l fuse-opt -d 'Options to pass to FUSE'

# Peformance
complete -c obnam --no-files -l dump-memory-profile -a 'none simple meliae heapy' -d 'Make memory profiling dumps using METHOD'
complete -c obnam --no-files -l memory-dump-interval -d 'Make memory profiling dumps at SECONDS'

# Performance tweaking
complete -c obnam --no-files -l chunk-size -d 'Size of chunks of file data'
complete -c obnam --no-files -l chunkids-per-group -d 'Encode NUM chunk ids per group'
complete -c obnam --no-files -l idpath-bits -d 'Chunk id level size'
complete -c obnam --no-files -l idpath-depth -d 'Depth of chunk id mapping'
complete -c obnam --no-files -l idpath-skip -d 'Chunk id mapping lowest bits skip'
complete -c obnam --no-files -l lru-size -d 'Size of LRU cache for B-tree nodes'
complete -c obnam --no-files -l node-size -d 'Size of B-tree nodes on disk'
complete -c obnam --no-files -l upload-queue-size -d 'Length of upload queue for B-tree nodes'

# Repository format green-albatross
complete -c obnam --no-files -l chunk-bag-size -d 'Approx max SIZE of bag combining chunk objects'
complete -c obnam --no-files -l chunk-cache-size -d 'In-memory cache SIZE for chunk objects'
complete -c obnam --no-files -l dir-bag-size -d 'Approx max SIZE of bags combining dir objects'
complete -c obnam --no-files -l dir-cache-size -d 'In-memory cache SIZE for dir objects'

# SSH/SFTP
complete -c obnam --no-files -l pure-paramiko -d 'Use only paramiko, no openssh'
complete -c obnam --no-files -l no-pure-paramiko -d 'Use openssh if available'
complete -c obnam -l ssh-command -d 'Executable to be used instead of "ssh"'
complete -c obnam --no-files -l ssh-host-keys-check -a 'no yes ask ssh-config' -d 'ssh host key check'
complete -c obnam -l ssh-key -d 'Use FILENAME as the ssh RSA key'
complete -c obnam -l ssh-known-hosts -a '\\~/.ssh/known_hosts' -d 'FILENAME of the known_hosts file'
