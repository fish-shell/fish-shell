# softwareupdate - macOS system software update tool (man 8 softwareupdate)
#
# softwareupdate uses a flag-based grammar: one primary command flag selects
# the operation; modifier flags and option flags follow.  No subcommand verbs.

# ── command-line introspection ───────────────────────────────────────

# Primary command tokens — one of these must appear for a cmd to be "chosen".
set -l __fish_softwareupdate_cmds \
    --list -l --download -d --install -i \
    --list-full-installers --fetch-full-installer \
    --install-rosetta --background --schedule \
    --history --dump-state --evaluate-products \
    --help -h

# True when no primary command flag has been typed yet.
function __fish_softwareupdate_no_cmd
    set -l toks (commandline -opc)
    for tok in $toks[2..]
        if contains -- $tok $__fish_softwareupdate_cmds
            return 1
        end
    end
    return 0
end

# True when --install or -i is among the tokens already typed.
function __fish_softwareupdate_doing_install
    contains -- --install (commandline -opc)
    or contains -- -i (commandline -opc)
end

# True when --download or -d is among the tokens already typed.
function __fish_softwareupdate_doing_download
    contains -- --download (commandline -opc)
    or contains -- -d (commandline -opc)
end

# True when --fetch-full-installer is among the tokens already typed.
function __fish_softwareupdate_doing_fetch
    contains -- --fetch-full-installer (commandline -opc)
end

# True when --schedule is among the tokens already typed.
function __fish_softwareupdate_doing_schedule
    contains -- --schedule (commandline -opc)
end

# True when --install-rosetta is among the tokens already typed.
function __fish_softwareupdate_doing_rosetta
    contains -- --install-rosetta (commandline -opc)
end

# ── primary commands ─────────────────────────────────────────────────

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -s l -l list -f \
    -d 'List all available updates'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -s d -l download -f \
    -d 'Download updates without installing'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -s i -l install -f \
    -d 'Download and install updates'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l list-full-installers -f \
    -d 'List available macOS full installers'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l fetch-full-installer -f \
    -d 'Download the latest recommended macOS full installer'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l install-rosetta -f \
    -d 'Install Rosetta 2 (Apple silicon only)'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l background -f \
    -d 'Trigger a background scan and update operation'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l schedule -f \
    -d 'Get or set automatic background check schedule'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l history -f \
    -d 'Show the install history'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l dump-state -f \
    -d 'Log internal SU daemon state to /var/log/install.log'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -l evaluate-products -f \
    -d 'Evaluate a list of product keys (use with --products)'

complete -c softwareupdate -n __fish_softwareupdate_no_cmd \
    -s h -l help -f \
    -d 'Print command usage'

# ── --install and --download modifiers ──────────────────────────────

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -s a -l all -f \
    -d 'Install all applicable updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -s r -l recommended -f \
    -d 'Install only recommended updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -s R -l restart -f \
    -d 'Automatically restart if required to complete installation'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -l force -f \
    -d 'Force a restart even if not flagged as required'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -l os-only -f \
    -d 'Install only macOS updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -l safari-only -f \
    -d 'Install only Safari updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -l stdinpass -f \
    -d 'Read owner password from stdin (Apple silicon only)'

complete -c softwareupdate -n __fish_softwareupdate_doing_install \
    -l user -f \
    -d 'Local username to authenticate as owner (Apple silicon only)'

# Download shares the same batch-selection modifiers (but not restart/force)
complete -c softwareupdate -n __fish_softwareupdate_doing_download \
    -s a -l all -f \
    -d 'Download all applicable updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_download \
    -s r -l recommended -f \
    -d 'Download only recommended updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_download \
    -l os-only -f \
    -d 'Download only macOS updates'

complete -c softwareupdate -n __fish_softwareupdate_doing_download \
    -l safari-only -f \
    -d 'Download only Safari updates'

# ── --fetch-full-installer modifiers ────────────────────────────────

complete -c softwareupdate -n __fish_softwareupdate_doing_fetch \
    -l full-installer-version -f \
    -d 'macOS version to fetch (e.g. 14.5)'

complete -c softwareupdate -n __fish_softwareupdate_doing_fetch \
    -l launch-installer -f \
    -d 'Launch the installer automatically after download'

# ── --install-rosetta modifier ───────────────────────────────────────

complete -c softwareupdate -n __fish_softwareupdate_doing_rosetta \
    -l agree-to-license -f \
    -d 'Agree to the software license without user interaction'

# ── global options (valid with most commands) ────────────────────────

complete -c softwareupdate \
    -l no-scan -f \
    -d 'Use previously cached scan results; do not rescan'

complete -c softwareupdate \
    -l product-types -f \
    -d 'Limit scan to a product type (e.g. macOS, Safari)'

complete -c softwareupdate \
    -l products -f \
    -d 'Comma-separated list of product keys to operate on'

complete -c softwareupdate \
    -l verbose -f \
    -d 'Enable verbose output'
