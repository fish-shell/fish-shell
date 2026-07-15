function __fish_fdesetup_no_verb
    not __fish_seen_subcommand_from help list enable disable status sync add remove changerecovery \
        removerecovery authrestart isactive haspersonalrecoverykey hasinstitutionalrecoverykey \
        usingrecoverykey supportsauthrestart validaterecovery showdeferralinfo version
end

complete -c fdesetup -f -n __fish_fdesetup_no_verb -a help -d 'Show abbreviated help'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a list -d 'List enabled FileVault users or locked volumes'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a enable -d 'Enable FileVault on the current volume'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a disable -d 'Disable FileVault'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a status -d 'Display current FileVault status'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a sync -d 'Synchronize Open Directory attributes to FileVault users'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a add -d 'Add additional FileVault users'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a remove -d 'Remove an enabled user from FileVault'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a changerecovery -d 'Add or update the personal or institutional recovery key'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a removerecovery -d 'Remove the current personal or institutional recovery key'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a authrestart -d 'Restart the system bypassing the FileVault unlock prompt'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a isactive -d 'Return whether FileVault is currently enabled'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a haspersonalrecoverykey -d 'Return whether a personal recovery key is set'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a hasinstitutionalrecoverykey -d 'Return whether an institutional recovery key is set'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a usingrecoverykey -d 'Return whether the system is unlocked with the personal recovery key'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a supportsauthrestart -d 'Return whether the system supports authenticated restart'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a validaterecovery -d 'Validate the personal recovery key'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a showdeferralinfo -d 'Show current deferred enablement settings'
complete -c fdesetup -f -n __fish_fdesetup_no_verb -a version -d 'Display the current tool version'

set -l all_verbs list enable disable status add remove changerecovery removerecovery authrestart \
    isactive haspersonalrecoverykey hasinstitutionalrecoverykey usingrecoverykey validaterecovery
complete -c fdesetup -d 'Print additional status output' -n "__fish_seen_subcommand_from $all_verbs" -o verbose

complete -c fdesetup -d 'Show extended user and volume information' -n '__fish_seen_subcommand_from list' -o extended
complete -c fdesetup -d 'Show currently locked and offline CoreStorage volumes' -n '__fish_seen_subcommand_from list' -o offline
complete -c fdesetup -d 'Show extended status with encryption progress estimate' -n '__fish_seen_subcommand_from status' -o extended
complete -c fdesetup -d 'Short user name to enable for FileVault' -f -n '__fish_seen_subcommand_from enable' -o user -r
complete -c fdesetup -d 'Additional user to add to FileVault' -f -n '__fish_seen_subcommand_from enable' -o usertoadd -r
complete -c fdesetup -d 'Read configuration from stdin as a plist' -n '__fish_seen_subcommand_from enable' -o inputplist
complete -c fdesetup -d 'Write recovery key and system info to stdout as a plist' -n '__fish_seen_subcommand_from enable' -o outputplist
complete -c fdesetup -d 'Always prompt for information' -n '__fish_seen_subcommand_from enable' -o prompt
complete -c fdesetup -d 'Force a normal restart after FileVault is configured (CoreStorage only)' -n '__fish_seen_subcommand_from enable' -o forcerestart
complete -c fdesetup -d 'Perform an authenticated restart after successful enablement' -n '__fish_seen_subcommand_from enable' -o authrestart
complete -c fdesetup -d 'Use institutional recovery key from FileVaultMaster.keychain' -n '__fish_seen_subcommand_from enable' -o keychain
complete -c fdesetup -d 'Path to DER-encoded certificate file for institutional recovery key' -r -n '__fish_seen_subcommand_from enable' -o certificate
complete -c fdesetup -d 'Defer enablement; write recovery info to this file path' -r -n '__fish_seen_subcommand_from enable' -o defer
complete -c fdesetup -d 'Max login cancellations before FileVault enablement is required' -f -n '__fish_seen_subcommand_from enable' -o forceatlogin -r
complete -c fdesetup -d 'Do not prompt for enablement at logout' -n '__fish_seen_subcommand_from enable' -o dontaskatlogout
complete -c fdesetup -d 'Do not create a personal recovery key' -n '__fish_seen_subcommand_from enable' -o norecoverykey
complete -c fdesetup -d 'User to add to FileVault' -f -n '__fish_seen_subcommand_from add' -o usertoadd -r
complete -c fdesetup -d 'Read user information from stdin as a plist' -n '__fish_seen_subcommand_from add' -o inputplist
complete -c fdesetup -d 'FileVault UUID of the user to remove' -f -n '__fish_seen_subcommand_from remove' -o uuid -r
complete -c fdesetup -d 'Short user name of the user to remove' -f -n '__fish_seen_subcommand_from remove' -o user -r
complete -c fdesetup -d 'Change or add the personal recovery key' -n '__fish_seen_subcommand_from changerecovery' -o personal
complete -c fdesetup -d 'Change or add the institutional recovery key' -n '__fish_seen_subcommand_from changerecovery' -o institutional
complete -c fdesetup -d 'Short user name to authenticate the change' -f -n '__fish_seen_subcommand_from changerecovery' -o user -r
complete -c fdesetup -d 'Use institutional recovery key from FileVaultMaster.keychain' -n '__fish_seen_subcommand_from changerecovery' -o keychain
complete -c fdesetup -d 'Path to DER-encoded certificate for the institutional recovery key' -r -n '__fish_seen_subcommand_from changerecovery' -o certificate
complete -c fdesetup -d 'Path to keychain file containing the institutional private key' -r -n '__fish_seen_subcommand_from changerecovery' -o key
complete -c fdesetup -d 'Read recovery information from stdin as a plist' -n '__fish_seen_subcommand_from changerecovery' -o inputplist
complete -c fdesetup -d 'Remove the personal recovery key' -n '__fish_seen_subcommand_from removerecovery' -o personal
complete -c fdesetup -d 'Remove the institutional recovery key' -n '__fish_seen_subcommand_from removerecovery' -o institutional
complete -c fdesetup -d 'Short user name to authenticate the removal' -f -n '__fish_seen_subcommand_from removerecovery' -o user -r
complete -c fdesetup -d 'Path to keychain file containing the institutional private key' -r -n '__fish_seen_subcommand_from removerecovery' -o key
complete -c fdesetup -d 'Read configuration from stdin as a plist' -n '__fish_seen_subcommand_from removerecovery' -o inputplist
complete -c fdesetup -d 'Read authentication information from stdin as a plist' -n '__fish_seen_subcommand_from authrestart' -o inputplist
complete -c fdesetup -d 'Minutes to delay before restarting (0=immediately, -1=never)' -f -n '__fish_seen_subcommand_from authrestart' -o delayminutes -r
complete -c fdesetup -f -n '__fish_seen_subcommand_from haspersonalrecoverykey hasinstitutionalrecoverykey' -o device -r -d 'Device path, BSD name, or LV/LVF UUID to query'
complete -c fdesetup -d 'Read recovery key from stdin as a plist' -n '__fish_seen_subcommand_from validaterecovery' -o inputplist
