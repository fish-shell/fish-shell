set -l softwareupdate_cmds --list -l --download -d --install -i --list-full-installers \
    --fetch-full-installer --install-rosetta --background --schedule --history --dump-state \
    --evaluate-products --help -h

function __fish_softwareupdate_no_cmd --inherit-variable softwareupdate_cmds
    set -l toks (commandline -xpc)
    for tok in $toks[2..]
        if contains -- $tok $softwareupdate_cmds
            return 1
        end
    end
    return 0
end

function __fish_softwareupdate_doing_install
    contains -- --install (commandline -xpc)
    or contains -- -i (commandline -xpc)
end

function __fish_softwareupdate_doing_download
    contains -- --download (commandline -xpc)
    or contains -- -d (commandline -xpc)
end

function __fish_softwareupdate_doing_fetch
    contains -- --fetch-full-installer (commandline -xpc)
end

function __fish_softwareupdate_doing_rosetta
    contains -- --install-rosetta (commandline -xpc)
end

complete -c softwareupdate -d 'List all available updates' -n __fish_softwareupdate_no_cmd -s l -l list
complete -c softwareupdate -d 'Download updates without installing' -n __fish_softwareupdate_no_cmd -s d -l download
complete -c softwareupdate -d 'Download and install updates' -n __fish_softwareupdate_no_cmd -s i -l install
complete -c softwareupdate -d 'List available macOS full installers' -n __fish_softwareupdate_no_cmd -l list-full-installers
complete -c softwareupdate -d 'Download the latest recommended macOS full installer' -n __fish_softwareupdate_no_cmd -l fetch-full-installer
complete -c softwareupdate -d 'Install Rosetta 2 (Apple silicon only)' -n __fish_softwareupdate_no_cmd -l install-rosetta
complete -c softwareupdate -d 'Trigger a background scan and update operation' -n __fish_softwareupdate_no_cmd -l background
complete -c softwareupdate -d 'Get or set automatic background check schedule' -n __fish_softwareupdate_no_cmd -l schedule
complete -c softwareupdate -d 'Show the install history' -n __fish_softwareupdate_no_cmd -l history
complete -c softwareupdate -d 'Log internal SU daemon state to /var/log/install.log' -n __fish_softwareupdate_no_cmd -l dump-state
complete -c softwareupdate -d 'Evaluate a list of product keys (use with --products)' -n __fish_softwareupdate_no_cmd -l evaluate-products
complete -c softwareupdate -n __fish_softwareupdate_no_cmd -s h -l help -d 'Print command usage'
complete -c softwareupdate -d 'Install all applicable updates' -n __fish_softwareupdate_doing_install -s a -l all
complete -c softwareupdate -d 'Install only recommended updates' -n __fish_softwareupdate_doing_install -s r -l recommended
complete -c softwareupdate -d 'Automatically restart if required to complete installation' -n __fish_softwareupdate_doing_install -s R -l restart
complete -c softwareupdate -d 'Force a restart even if not flagged as required' -n __fish_softwareupdate_doing_install -l force
complete -c softwareupdate -d 'Install only macOS updates' -n __fish_softwareupdate_doing_install -l os-only
complete -c softwareupdate -d 'Install only Safari updates' -n __fish_softwareupdate_doing_install -l safari-only
complete -c softwareupdate -d 'Read owner password from stdin (Apple silicon only)' -n __fish_softwareupdate_doing_install -l stdinpass
complete -c softwareupdate -d 'Local username to authenticate as owner (Apple silicon only)' -n __fish_softwareupdate_doing_install -l user
complete -c softwareupdate -d 'Download all applicable updates' -n __fish_softwareupdate_doing_download -s a -l all
complete -c softwareupdate -d 'Download only recommended updates' -n __fish_softwareupdate_doing_download -s r -l recommended
complete -c softwareupdate -d 'Download only macOS updates' -n __fish_softwareupdate_doing_download -l os-only
complete -c softwareupdate -d 'Download only Safari updates' -n __fish_softwareupdate_doing_download -l safari-only
complete -c softwareupdate -d 'macOS version to fetch (e.g. 14.5)' -n __fish_softwareupdate_doing_fetch -l full-installer-version
complete -c softwareupdate -d 'Launch the installer automatically after download' -n __fish_softwareupdate_doing_fetch -l launch-installer
complete -c softwareupdate -d 'Agree to the software license without user interaction' -n __fish_softwareupdate_doing_rosetta -l agree-to-license
complete -c softwareupdate -l no-scan -d 'Use previously cached scan results; do not rescan'
complete -c softwareupdate -d 'Limit scan to a comma-separated list of product types' -l product-types -x -a '(__fish_append , macOS Safari)'
complete -c softwareupdate -l products -d 'Comma-separated list of product keys to operate on'
complete -c softwareupdate -l verbose -d 'Enable verbose output'
