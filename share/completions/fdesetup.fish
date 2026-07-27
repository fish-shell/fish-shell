# fdesetup - FileVault configuration tool (man 8 fdesetup)
#
# fdesetup uses subcommand verbs (e.g. `fdesetup enable -user alice`).
# The first non-option token after the command name is the verb.

# ── command-line introspection ───────────────────────────────────────

# True when no verb has been chosen yet (so verbs should be offered).
function __fish_fdesetup_no_verb
    not __fish_seen_subcommand_from \
        help list enable disable status sync add remove \
        changerecovery removerecovery authrestart isactive \
        haspersonalrecoverykey hasinstitutionalrecoverykey \
        usingrecoverykey supportsauthrestart validaterecovery \
        showdeferralinfo version
end

# ── verbs ────────────────────────────────────────────────────────────
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

# ── shared / multi-verb options ──────────────────────────────────────
# -verbose is accepted by nearly every verb
set -l _all_verbs list enable disable status add remove changerecovery \
    removerecovery authrestart isactive haspersonalrecoverykey \
    hasinstitutionalrecoverykey usingrecoverykey validaterecovery

complete -c fdesetup -f -n "__fish_seen_subcommand_from $_all_verbs" \
    -l verbose -d 'Print additional status output'

# ── list ─────────────────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from list' \
    -l extended -d 'Show extended user and volume information'
complete -c fdesetup -f -n '__fish_seen_subcommand_from list' \
    -l offline -d 'Show currently locked and offline CoreStorage volumes'

# ── status ───────────────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from status' \
    -l extended -d 'Show extended status with encryption progress estimate'

# ── enable ───────────────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l user -r -d 'Short user name to enable for FileVault'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l usertoadd -r -d 'Additional user to add to FileVault'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l inputplist -d 'Read configuration from stdin as a plist'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l outputplist -d 'Write recovery key and system info to stdout as a plist'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l prompt -d 'Always prompt for information'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l forcerestart -d 'Force a normal restart after FileVault is configured (CoreStorage only)'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l authrestart -d 'Perform an authenticated restart after successful enablement'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l keychain -d 'Use institutional recovery key from FileVaultMaster.keychain'
complete -c fdesetup -r -n '__fish_seen_subcommand_from enable' \
    -l certificate -d 'Path to DER-encoded certificate file for institutional recovery key'
complete -c fdesetup -r -n '__fish_seen_subcommand_from enable' \
    -l defer -d 'Defer enablement; write recovery info to this file path'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l forceatlogin -r -d 'Max login cancellations before FileVault enablement is required'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l dontaskatlogout -d 'Do not prompt for enablement at logout'
complete -c fdesetup -f -n '__fish_seen_subcommand_from enable' \
    -l norecoverykey -d 'Do not create a personal recovery key'

# ── disable ──────────────────────────────────────────────────────────
# Only -verbose, already covered above.

# ── add ──────────────────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from add' \
    -l usertoadd -r -d 'User to add to FileVault'
complete -c fdesetup -f -n '__fish_seen_subcommand_from add' \
    -l inputplist -d 'Read user information from stdin as a plist'

# ── remove ───────────────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from remove' \
    -l uuid -r -d 'FileVault UUID of the user to remove'
complete -c fdesetup -f -n '__fish_seen_subcommand_from remove' \
    -l user -r -d 'Short user name of the user to remove'

# ── changerecovery ───────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from changerecovery' \
    -l personal -d 'Change or add the personal recovery key'
complete -c fdesetup -f -n '__fish_seen_subcommand_from changerecovery' \
    -l institutional -d 'Change or add the institutional recovery key'
complete -c fdesetup -f -n '__fish_seen_subcommand_from changerecovery' \
    -l user -r -d 'Short user name to authenticate the change'
complete -c fdesetup -f -n '__fish_seen_subcommand_from changerecovery' \
    -l keychain -d 'Use institutional recovery key from FileVaultMaster.keychain'
complete -c fdesetup -r -n '__fish_seen_subcommand_from changerecovery' \
    -l certificate -d 'Path to DER-encoded certificate for the institutional recovery key'
complete -c fdesetup -r -n '__fish_seen_subcommand_from changerecovery' \
    -l key -d 'Path to keychain file containing the institutional private key'
complete -c fdesetup -f -n '__fish_seen_subcommand_from changerecovery' \
    -l inputplist -d 'Read recovery information from stdin as a plist'

# ── removerecovery ───────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from removerecovery' \
    -l personal -d 'Remove the personal recovery key'
complete -c fdesetup -f -n '__fish_seen_subcommand_from removerecovery' \
    -l institutional -d 'Remove the institutional recovery key'
complete -c fdesetup -f -n '__fish_seen_subcommand_from removerecovery' \
    -l user -r -d 'Short user name to authenticate the removal'
complete -c fdesetup -r -n '__fish_seen_subcommand_from removerecovery' \
    -l key -d 'Path to keychain file containing the institutional private key'
complete -c fdesetup -f -n '__fish_seen_subcommand_from removerecovery' \
    -l inputplist -d 'Read configuration from stdin as a plist'

# ── authrestart ──────────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from authrestart' \
    -l inputplist -d 'Read authentication information from stdin as a plist'
complete -c fdesetup -f -n '__fish_seen_subcommand_from authrestart' \
    -l delayminutes -r -d 'Minutes to delay before restarting (0=immediately, -1=never)'

# ── haspersonalrecoverykey / hasinstitutionalrecoverykey ─────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from haspersonalrecoverykey hasinstitutionalrecoverykey' \
    -l device -r -d 'Device path, BSD name, or LV/LVF UUID to query'

# ── validaterecovery ─────────────────────────────────────────────────
complete -c fdesetup -f -n '__fish_seen_subcommand_from validaterecovery' \
    -l inputplist -d 'Read recovery key from stdin as a plist'
