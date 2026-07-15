# csrutil - configure System Integrity Protection (man 8 csrutil)
#
# csrutil uses subcommands (e.g. `csrutil status`).
# Two subcommands take their own sub-verbs: authenticated-root and
# allow-research-guests each accept {status,enable,disable}.

# ── command-line introspection ───────────────────────────────────────
# True when no top-level subcommand has been seen yet.
function __fish_csrutil_no_subcommand
    not __fish_seen_subcommand_from \
        status enable disable clear \
        authenticated-root allow-research-guests
end

# True when the top-level subcommand is authenticated-root.
function __fish_csrutil_doing_authenticated_root
    __fish_seen_subcommand_from authenticated-root
end

# True when the top-level subcommand is allow-research-guests.
function __fish_csrutil_doing_allow_research_guests
    __fish_seen_subcommand_from allow-research-guests
end

# ── top-level subcommands ────────────────────────────────────────────
complete -c csrutil -f -n __fish_csrutil_no_subcommand \
    -a status -d 'Display SIP configuration of the running (or each Recovery) OS'
complete -c csrutil -f -n __fish_csrutil_no_subcommand \
    -a enable -d 'Enable System Integrity Protection (Recovery OS only)'
complete -c csrutil -f -n __fish_csrutil_no_subcommand \
    -a disable -d 'Disable System Integrity Protection (Recovery OS only)'
complete -c csrutil -f -n __fish_csrutil_no_subcommand \
    -a clear -d 'Clear the existing SIP configuration'
complete -c csrutil -f -n __fish_csrutil_no_subcommand \
    -a authenticated-root \
    -d 'Manage authenticated-root (sealed system snapshot) policy'
complete -c csrutil -f -n __fish_csrutil_no_subcommand \
    -a allow-research-guests \
    -d 'Manage allow-research-guests policy'

# ── authenticated-root sub-verbs ─────────────────────────────────────
complete -c csrutil -f -n __fish_csrutil_doing_authenticated_root \
    -a status -d 'Show the current authenticated-root setting'
complete -c csrutil -f -n __fish_csrutil_doing_authenticated_root \
    -a enable -d 'Only allow booting from sealed system snapshots (Recovery OS only)'
complete -c csrutil -f -n __fish_csrutil_doing_authenticated_root \
    -a disable -d 'Allow booting from non-sealed system snapshots (Recovery OS only)'

# ── allow-research-guests sub-verbs ──────────────────────────────────
complete -c csrutil -f -n __fish_csrutil_doing_allow_research_guests \
    -a status -d 'Show the current allow-research-guests setting'
complete -c csrutil -f -n __fish_csrutil_doing_allow_research_guests \
    -a enable -d 'Allow research guests (Recovery OS only)'
complete -c csrutil -f -n __fish_csrutil_doing_allow_research_guests \
    -a disable -d 'Disallow research guests (Recovery OS only)'
