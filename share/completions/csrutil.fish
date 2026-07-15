function __fish_csrutil_no_subcommand
    not __fish_seen_subcommand_from status enable disable clear authenticated-root \
        allow-research-guests
end

function __fish_csrutil_doing_authenticated_root
    __fish_seen_subcommand_from authenticated-root
end

function __fish_csrutil_doing_allow_research_guests
    __fish_seen_subcommand_from allow-research-guests
end

complete -c csrutil -d 'Display SIP configuration of the running (or each Recovery) OS' -f -n __fish_csrutil_no_subcommand -a status
complete -c csrutil -d 'Enable System Integrity Protection (Recovery OS only)' -f -n __fish_csrutil_no_subcommand -a enable
complete -c csrutil -d 'Disable System Integrity Protection (Recovery OS only)' -f -n __fish_csrutil_no_subcommand -a disable
complete -c csrutil -d 'Clear the existing SIP configuration' -f -n __fish_csrutil_no_subcommand -a clear
complete -c csrutil -d 'Manage authenticated-root (sealed system snapshot) policy' -f -n __fish_csrutil_no_subcommand -a authenticated-root
complete -c csrutil -d 'Manage allow-research-guests policy' -f -n __fish_csrutil_no_subcommand -a allow-research-guests

complete -c csrutil -d 'Show the current authenticated-root setting' -f -n __fish_csrutil_doing_authenticated_root -a status
complete -c csrutil -d 'Only allow booting from sealed system snapshots (Recovery OS only)' -f -n __fish_csrutil_doing_authenticated_root -a enable
complete -c csrutil -d 'Allow booting from non-sealed system snapshots (Recovery OS only)' -f -n __fish_csrutil_doing_authenticated_root -a disable

complete -c csrutil -d 'Show the current allow-research-guests setting' -f -n __fish_csrutil_doing_allow_research_guests -a status
complete -c csrutil -d 'Allow research guests (Recovery OS only)' -f -n __fish_csrutil_doing_allow_research_guests -a enable
complete -c csrutil -d 'Disallow research guests (Recovery OS only)' -f -n __fish_csrutil_doing_allow_research_guests -a disable
