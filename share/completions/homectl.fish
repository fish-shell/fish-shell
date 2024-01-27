set -l commands_need_user activate deactivate inspect authenticate remove update passwd resize lock unlock with
set -l commands list create lock-all $commands_need_user

function __homectl_users
    homectl list | string match -r -- '\S+\s+\d+\s+\d+' | string match -r -- '\S+'
end

function __homectl_subcommand_is
    set -l cmd (commandline -pxc)
    contains -- $cmd[-1] $argv
end

# Commands
complete -c homectl -f
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a list -d 'List home areas'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a activate -d 'Activate a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a deactivate -d 'Deactivate a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a inspect -d 'Inspect a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a authenticate -d 'Authenticate a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a create -d 'Create a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a remove -d 'Remove a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a update -d 'Update a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a passwd -d 'Change password of a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a resize -d 'Resize a home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a lock -d 'Temporarily lock an active home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a unlock -d 'Unlock a temporarily locked home area'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a lock-all -d 'Lock all suitable home areas'
complete -c homectl -n "not __fish_seen_subcommand_from $commands" -a with -d 'Run shell or command with access to a home area'
complete -c homectl -n "__homectl_subcommand_is $commands_need_user" -xa '(__homectl_users)'

# Options
complete -c homectl -s h -l help -d 'Show this help'
complete -c homectl -l version -d 'Show package version'
complete -c homectl -l no-pager -d 'Do not pipe output into a pager'
complete -c homectl -l no-legend -d 'Do not show the headers and footers'
complete -c homectl -l no-ask-password -d 'Do not ask for system passwords'
complete -c homectl -s H -l host -d 'Operate on remote host'
complete -c homectl -s M -l machine -d 'Operate on local container'
complete -c homectl -l identity -d 'Read JSON identity from file' -r
complete -c homectl -l json -d 'Output inspection data in JSON' -xa 'pretty short off'
complete -c homectl -s j -d 'Equivalent to --json (on TTY) or short (otherwise)'
complete -c homectl -l export-format -d 'Strip JSON inspection data' -xa 'full stripped minimal'
complete -c homectl -s E -d 'Equals -j --export-format (twice to minimal)'
complete -c homectl -o EE -d 'Equals -j --export-format=minimal'

# User Record Properties
set -l condition '__fish_seen_subcommand_from create update'

# General User Record Properties
complete -c homectl -n $condition -s c -l real-name -d 'Real name for user'
complete -c homectl -n $condition -l realm -d 'Realm to create user in'
complete -c homectl -n $condition -l email-address -d 'Email address for user'
complete -c homectl -n $condition -l location -d 'Set location of user on earth'
complete -c homectl -n $condition -l icon-name -d 'Icon name for user'
complete -c homectl -n $condition -s d -l home-dir -d 'Home directory' -r
complete -c homectl -n $condition -s u -l uid -d 'Numeric UID for user'
complete -c homectl -n $condition -s G -l member-of -d 'Add user to group'
complete -c homectl -n $condition -l skel -d 'Skeleton directory to use' -r
complete -c homectl -n $condition -l shell -d 'Shell for account' -xa "(string match -r '^[^#].*' < /etc/shells)"
complete -c homectl -n $condition -l setenv -d 'Set an environment variable at log-in'
complete -c homectl -n $condition -l timezone -d 'Set a time-zone'
complete -c homectl -n $condition -l language -d 'Set preferred language'
complete -c homectl -n $condition -l ssh-authorized-keys -d 'Specify SSH public keys'
complete -c homectl -n $condition -l pkcs11-token-uri -d 'URI to PKCS#11 security token'
complete -c homectl -n $condition -l fido2-device -d 'Path to FIDO2 hidraw device' -r

# Account Management User Record Properties
complete -c homectl -n $condition -l locked -d 'Set locked account state' -xa 'yes no'
complete -c homectl -n $condition -l not-before -d 'Do not allow logins before'
complete -c homectl -n $condition -l not-after -d 'Do not allow logins after'
complete -c homectl -n $condition -l rate-limit-interval -d 'Login rate-limit interval in seconds'
complete -c homectl -n $condition -l rate-limit-burst -d 'Login rate-limit attempts per interval'

# Password Policy User Record Properties:
complete -c homectl -n $condition -l password-hint -d 'Set Password hint'
complete -c homectl -n $condition -l enforce-password-policy -d 'Control enforce password policy' -xa 'yes no'
complete -c homectl -n $condition -s P -d 'Equivalent to --enforce-password-password=no'
complete -c homectl -n $condition -l password-change-now -d 'Require the password to be changed on next login' -xa 'yes no'
complete -c homectl -n $condition -l password-change-min -d 'Require minimum time between password changes'
complete -c homectl -n $condition -l password-change-max -d 'Require maximum time between password changes'
complete -c homectl -n $condition -l password-change-warn -d 'How much time to warn before password expiry'
complete -c homectl -n $condition -l password-change-inactive -d 'How much time to block password after expiry'

# Resource Management User Record Properties:
complete -c homectl -n $condition -l disk-size -d 'Size to assign the user on disk'
complete -c homectl -n $condition -l access-mode -d 'User home directory access mode'
complete -c homectl -n $condition -l umask -d 'Umask for user when logging in'
complete -c homectl -n $condition -l nice -d 'Nice level for user'
complete -c homectl -n $condition -l rlimit -d 'Set resource limits'
complete -c homectl -n $condition -l tasks-max -d 'Set maximum number of per-user tasks'
complete -c homectl -n $condition -l memory-high -d 'Set high memory threshold in bytes'
complete -c homectl -n $condition -l memory-max -d 'Set maximum memory limit'
complete -c homectl -n $condition -l cpu-weight -d 'Set CPU weight'
complete -c homectl -n $condition -l io-weight -d 'Set IO weight'

# Storage User Record Properties:
complete -c homectl -n $condition -l storage -d 'Storage type to use' -xa 'luks fscrypt directory subvolume cifs'
complete -c homectl -n $condition -l image-path -d 'Path to image file/directory' -r

# LUKS Storage User Record Properties:
complete -c homectl -n $condition -l fs-type -d 'File system type to use in case of luks storage' -xa 'ext4 xfs btrfs'
complete -c homectl -n $condition -l luks-discard -d 'Whether to use discard feature' -xa 'yes no'
complete -c homectl -n $condition -l luks-offline-discard -d 'Whether to trim file on logout' -xa 'yes no'
complete -c homectl -n $condition -l luks-cipher -d 'Cipher to use for LUKS encryption'
complete -c homectl -n $condition -l luks-cipher-mode -d 'Cipher mode to use for LUKS encryption'
complete -c homectl -n $condition -l luks-volume-key-size -d 'Volume key size to use for LUKS encryption'
complete -c homectl -n $condition -l luks-pbkdf-type -d 'Password-based Key Derivation Function to use'
complete -c homectl -n $condition -l luks-pbkdf-hash-algorithm -d 'PBKDF hash algorithm to use'
complete -c homectl -n $condition -l luks-pbkdf-time-cost -d 'Time cost for PBKDF in seconds'
complete -c homectl -n $condition -l luks-pbkdf-memory-cost -d 'Memory cost for PBKDF in bytes'
complete -c homectl -n $condition -l luks-pbkdf-parallel-threads -d 'Number of parallel threads for PKBDF'

# Mounting User Record Properties:
complete -c homectl -n $condition -l nosuid -d 'Control the nosuid flag of the home mount' -xa 'yes no'
complete -c homectl -n $condition -l nodev -d 'Control the nodev flag of the home mount' -xa 'yes no'
complete -c homectl -n $condition -l noexec -d 'Control the noexec flag of the home mount' -xa 'yes no'

# CIFS User Record Properties:
complete -c homectl -n $condition -l cifs-domain -d 'CIFS (Windows) domain'
complete -c homectl -n $condition -l cifs-user-name -d 'CIFS (Windows) user name'
complete -c homectl -n $condition -l cifs-service -d 'CIFS (Windows) service to mount as home area'

# Login Behaviour User Record Properties:
complete -c homectl -n $condition -l stop-delay -d 'How long to leave user services running after logout'
complete -c homectl -n $condition -l kill-processes -d 'Whether to kill user processes when sessions terminate' -xa 'yes no'
complete -c homectl -n $condition -l auto-login -d 'Try to log this user in automatically' -xa 'yes no'
