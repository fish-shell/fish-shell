# profiles - macOS Profiles Tool (man 1 profiles)
#
# profiles uses subcommand verbs (e.g. `profiles list -type configuration`).
# macOS 11+ removed the ability to install configuration profiles via this tool;
# the install verb is kept for bootstraptoken and legacy completions.

# ── command-line introspection ───────────────────────────────────────
# Print the subcommand verb (first non-option token after the command name).
function __fish_profiles_verb
    set -l toks (commandline -opc)
    set -e toks[1]
    for t in $toks
        if not string match -q -- '-*' $t
            echo $t
            return 0
        end
    end
    return 1
end

# True when no verb has been chosen yet.
function __fish_profiles_no_verb
    not __fish_profiles_verb >/dev/null 2>&1
end

# True when the given verb is active.
function __fish_profiles_using_verb
    set -l v (__fish_profiles_verb 2>/dev/null)
    test -n "$v" && test "$v" = "$argv[1]"
end

# ── verbs ────────────────────────────────────────────────────────────
complete -c profiles -f -n __fish_profiles_no_verb -a help -d 'Show abbreviated help'
complete -c profiles -f -n __fish_profiles_no_verb -a list -d 'List installed profiles'
complete -c profiles -f -n __fish_profiles_no_verb -a show -d 'Show expanded profile information'
complete -c profiles -f -n __fish_profiles_no_verb -a remove -d 'Remove installed profiles'
complete -c profiles -f -n __fish_profiles_no_verb -a status -d 'Display profile installation status'
complete -c profiles -f -n __fish_profiles_no_verb -a sync -d 'Sync configuration profiles with local users'
complete -c profiles -f -n __fish_profiles_no_verb -a renew -d 'Renew certificates or retry DEP enrollment'
complete -c profiles -f -n __fish_profiles_no_verb -a validate -d 'Validate a provisioning or enrollment profile'
complete -c profiles -f -n __fish_profiles_no_verb -a install -d 'Install a profile (bootstraptoken; config install removed in macOS 11)'
complete -c profiles -f -n __fish_profiles_no_verb -a version -d 'Display the tool version'

# ── -type values ─────────────────────────────────────────────────────
# Verbs that accept the full set of profile types.
set -l type_verbs_full list show remove status renew validate install
for __v in $type_verbs_full
    complete -c profiles -x -n "__fish_profiles_using_verb $__v" \
        -l type -d 'Profile type' \
        -a 'configuration\tConfiguration\ profile provisioning\tProvisioning\ profile enrollment\tDEP/MDM\ enrollment bootstraptoken\tBootstrap\ Token'
end

# sync only supports -type configuration.
complete -c profiles -x -n '__fish_profiles_using_verb sync' \
    -l type -d 'Profile type' \
    -a 'configuration\tConfiguration\ profile'

# ── per-verb options ─────────────────────────────────────────────────

# list
complete -c profiles -f -n '__fish_profiles_using_verb list' -l user -d 'OD short user name'
complete -c profiles -f -n '__fish_profiles_using_verb list' -l output -d 'Output path (stdout | stdout-xml | file)'
complete -c profiles -f -n '__fish_profiles_using_verb list' -l all -d 'List all profiles on the system (requires root)'
complete -c profiles -f -n '__fish_profiles_using_verb list' -l verbose -d 'Display additional information'

# show
complete -c profiles -f -n '__fish_profiles_using_verb show' -l user -d 'OD short user name'
complete -c profiles -f -n '__fish_profiles_using_verb show' -l output -d 'Output path (stdout | stdout-xml | file)'
complete -c profiles -f -n '__fish_profiles_using_verb show' -l all -d 'Show all profiles on the system (requires root)'
complete -c profiles -f -n '__fish_profiles_using_verb show' -l cached -d 'Use only locally cached enrollment information'
complete -c profiles -f -n '__fish_profiles_using_verb show' -l verbose -d 'Display additional information'

# remove
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l user -d 'OD short user name'
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l identifier -d 'Profile PayloadIdentifier to remove'
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l uuid -d 'Profile PayloadUUID to remove'
complete -c profiles -n '__fish_profiles_using_verb remove' -l path -d 'Path to profile file to remove'
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l forced -d 'Suppress confirmation and ignore removal errors'
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l all -d 'Remove all profiles for the user or device'
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l password -d 'Removal password for password-protected profiles'
complete -c profiles -f -n '__fish_profiles_using_verb remove' -l verbose -d 'Display additional information'

# status
complete -c profiles -f -n '__fish_profiles_using_verb status' -l verbose -d 'Display additional information'

# renew
complete -c profiles -f -n '__fish_profiles_using_verb renew' -l identifier -d 'Profile PayloadIdentifier to renew'
complete -c profiles -f -n '__fish_profiles_using_verb renew' -l output -d 'Output path (stdout | stdout-xml | file)'
complete -c profiles -f -n '__fish_profiles_using_verb renew' -l verbose -d 'Display additional information'

# validate
complete -c profiles -n '__fish_profiles_using_verb validate' -l path -d 'Path to provisioning profile to validate'
complete -c profiles -f -n '__fish_profiles_using_verb validate' -l verbose -d 'Display additional information'

# install
complete -c profiles -n '__fish_profiles_using_verb install' -l path -d 'Path to profile file to install'
complete -c profiles -f -n '__fish_profiles_using_verb install' -l user -d 'OD short user name'
complete -c profiles -f -n '__fish_profiles_using_verb install' -l verbose -d 'Display additional information'

# sync (only supports -type configuration; no other options documented)
complete -c profiles -f -n '__fish_profiles_using_verb sync' -l verbose -d 'Display additional information'
