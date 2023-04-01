# Completions for `snap` command

function __ghoti_snap_no_subcommand -d 'Test if snap has yet to be given the subcommand'
    for i in (commandline -opc)
        if contains -- $i abort ack alias aliases buy changes connect disable disconnect download \
                enable find get help info install interfaces known list login logout prefer refresh remove \
                revert run set tasks try unalias version watch
            return 1
        end
    end
    return 0
end

function __ghoti_snap_using_subcommand -d 'Test if given subcommand is used'
    for i in (commandline -opc)
        if contains -- $i $argv[1]
            return 0
        end
    end
    return 1
end

function __ghoti_snap_use_file -d 'Test if snap command should have files as potential completion'
    for i in (commandline -opc)
        if contains -- $i ack try
            return 0
        end
    end
    return 1
end

function __ghoti_snap_subcommand
    set -l subcommand $argv[1]
    set -e argv[1]
    complete -f -c snap -n __ghoti_snap_no_subcommand -a $subcommand $argv
end

function __ghoti_snap_option
    set -l subcommand $argv[1]
    set -e argv[1]
    complete -f -c snap -n "__ghoti_snap_using_subcommand $subcommand" $argv
end

function __ghoti_snap_disabled_snaps -d 'List disabled snaps'
    snap list | string match "*disabled" | string replace -r '(.+?) .*disabled' '$1'
end

function __ghoti_snap_enabled_snaps -d 'List disabled snaps'
    snap list | string match -vr "disabled|Name" | string replace -r '(.+?) .*' '$1'
end

function __ghoti_snap_installed_snaps -d 'List installed snaps'
    snap list | string replace -r '(.+?) .*' '$1' | string match -v 'Name*'
end

function __ghoti_snap_interfaces -d 'List of interfaces'
    for snap in (__ghoti_snap_installed_snaps)
        if test $snap != core
            snap interfaces $snap 2>/dev/null | string replace -r '[- ]*([^ ]*)[ ]+([^ ]+)' '$2$1' | string match -v "*Slot*"
        end
    end
end

function __ghoti_snap_change_id -d 'List change IDs'
    snap changes | string match -v 'ID*' | string replace -r '([0-9]*) .*' '$1'
end

function __ghoti_snap_aliases -d 'List aliases'
    snap aliases | string match -v 'Command*' | string replace -r '.* (.+?) .*$' '$1'
    snap aliases | string match -v 'Command*' | string replace -r '(.*?) .*$' '$1'
end

function __ghoti_snap_no_assertion -d 'Check that no assertion type is used yet'
    for i in (commandline -opc)
        if contains -- $i account account-key model serial snap-declaration snap-build snap-revision \
                system-user validation
            return 1
        end
    end
    return 0
end

function __ghoti_snap_using_assertion -d 'Check if certain assertion type is used'
    if __ghoti_snap_using_subcommand known
        if __ghoti_snap_using_subcommand $argv[1]
            return 0
        end
    end
    return 1
end

function __ghoti_snap_assertion
    set -l assertion $argv[1]
    set -e argv[1]
    complete -f -c snap -n '__ghoti_snap_using_subcommand known; and __ghoti_snap_no_assertion' -a $assertion
    complete -f -c snap -n "__ghoti_snap_using_assertion $assertion" -a "(__ghoti_snap_filters $assertion)" \
        -d Filter
end

function __ghoti_snap_filters -d 'List assertion filters'
    snap known $argv[1] | string match -v 'type:*' | string match '*: *' | string replace -r '(.*): (.*)' '$1=$2'
end

# Enable file completions where appropriate
complete -c snap -n __ghoti_snap_use_file -a '(__ghoti_complete_path)'

# Support flags
complete -x -f -c snap -s h -l help -d 'Show this help message'
complete -x -f -c snap -s v -l version -d 'Print the version and exit'

# Abort
__ghoti_snap_subcommand abort -d "Abort a pending change"

# Ack
__ghoti_snap_subcommand ack -r -d "Adds an assertion to the system"

# Alias
__ghoti_snap_subcommand alias -r -d "Sets up a manual alias"
__ghoti_snap_option alias -a '(__ghoti_snap_installed_snaps)' -d Snap

# Aliases
__ghoti_snap_subcommand aliases -d "Lists aliases in the system"

# Buy
__ghoti_snap_subcommand buy -r -d "Buys a snap"

# Changes
__ghoti_snap_subcommand changes -d "List system changes"

# Connect
__ghoti_snap_subcommand connect -r -d "Connects a plug to a slot"
__ghoti_snap_option connect -a '(__ghoti_snap_interfaces)' -d "Snap:Plug or Slot"

# Disable
__ghoti_snap_subcommand disable -r -d "Disables a snap in the system"
__ghoti_snap_option disable -a '(__ghoti_snap_enabled_snaps)' -d "Enabled snap"

# Disconnect
__ghoti_snap_subcommand disconnect -r -d "Disconnects a plug from a slot"
__ghoti_snap_option disconnect -a '(__ghoti_snap_interfaces)' -d "Snap:Plug or Slot"

# Downloads
__ghoti_snap_subcommand download -r -d "Downloads the given snap"
__ghoti_snap_option download -l channel -d "Use this channel instead of stable"
__ghoti_snap_option download -l edge -d "Install from the edge channel"
__ghoti_snap_option download -l beta -d "Install from the beta channel"
__ghoti_snap_option download -l candidate -d "Install from the candidate channel"
__ghoti_snap_option download -l stable -d "Install from the stable channel"
__ghoti_snap_option download -l revision -d "Download the given revision of snap, to which you must have developer access"

# Enable
__ghoti_snap_subcommand enable -r -d "Enables a snap in the system"
__ghoti_snap_option enable -a '(__ghoti_snap_disabled_snaps)' -d "Disabled snap"

# Find
__ghoti_snap_subcommand find -r -d "Finds packages to install"
__ghoti_snap_option find -l private -d "Search private snaps"
__ghoti_snap_option find -l section -d "Restrict the search to a given section"

# There seems to be no programmatic way of getting configuration options
# Get
__ghoti_snap_subcommand get -r -d "Prints configuration options"
__ghoti_snap_option get -s t -d "Strict typing with nulls and quoted strings"
__ghoti_snap_option get -s d -d "Always return documents, even with single key"
__ghoti_snap_option get -a '(__ghoti_snap_installed_snaps)' -d Snap

# Help
__ghoti_snap_subcommand help -d "The help command shows useful information"
__ghoti_snap_option help -l man -d "Generates the manpage"

# Info
__ghoti_snap_subcommand info -r -d "Show detailed information about a snap"
__ghoti_snap_option info -l verbose -d "Include a verbose list of snap's notes"
__ghoti_snap_option info -a '(__ghoti_snap_installed_snaps)' -d Snap

# Install
__ghoti_snap_subcommand install -r -d "Installs a snap to the system"
__ghoti_snap_option install -l channel -d "Use this channel instead of stable"
__ghoti_snap_option install -l edge -d "Install from the edge channel"
__ghoti_snap_option install -l beta -d "Install from the beta channel"
__ghoti_snap_option install -l candidate -d "Install from the candidate channel"
__ghoti_snap_option install -l stable -d "Install from the stable channel"
__ghoti_snap_option install -l revision -d "Install the given revision of snap, to which you must have developer access"
__ghoti_snap_option install -l devmode -d "Put snap in development mode and disable security confinement"
__ghoti_snap_option install -l jailmode -d "Put snap in enforced confinement mode"
__ghoti_snap_option install -l classic -d "Put snap in classic mode and disable security confinement"
__ghoti_snap_option install -l dangerous -d "Install the given snap file even if there are no pre-acknowledged signatures for it, meaning it was not  verified and could be dangerous"

# Interfaces
__ghoti_snap_subcommand interfaces -d "Lists interfaces in the system"
complete -f -c snap -n '__ghoti_snap_using_subcommand interfaces' -a '(__ghoti_snap_installed_snaps)' -d Snap
__ghoti_snap_option interfaces -s i -d "Constrain listing to specific interfaces"

# Known
__ghoti_snap_subcommand known -r -d "Shows known assertions of the provided type"
__ghoti_snap_option known -l remote -d "Shows known assertions of the provided type"
__ghoti_snap_assertion account -d 'Assertion type'
__ghoti_snap_assertion account-key -d 'Assertion type'
__ghoti_snap_assertion model -d 'Assertion type'
__ghoti_snap_assertion serial -d 'Assertion type'
__ghoti_snap_assertion snap-declaration -d 'Assertion type'
__ghoti_snap_assertion snap-build -d 'Assertion type'
__ghoti_snap_assertion snap-revision -d 'Assertion type'
__ghoti_snap_assertion system-user -d 'Assertion type'
__ghoti_snap_assertion validation -d 'Assertion type'

# List
__ghoti_snap_subcommand list -d "List installed snaps"
__ghoti_snap_option list -l all -d "Show all revisions"

# Login
__ghoti_snap_subcommand login -d "Authenticates on snapd and the store"

# Logout
__ghoti_snap_subcommand logout -d "Log out of the store"

# Prefer
__ghoti_snap_subcommand prefer -r -d "Prefes aliases from a snap and disable conflicts"
__ghoti_snap_option prefer -a '(__ghoti_snap_installed_snaps)' -d Snap

# Refresh
__ghoti_snap_subcommand refresh -r -d "Refreshes a snap in the system"
__ghoti_snap_option refresh -l channel -d "Use this channel instead of stable"
__ghoti_snap_option refresh -l edge -d "Install from the edge channel"
__ghoti_snap_option refresh -l beta -d "Install from the beta channel"
__ghoti_snap_option refresh -l candidate -d "Install from the candidate channel"
__ghoti_snap_option refresh -l stable -d "Install from the stable channel"
__ghoti_snap_option refresh -l revision -d "Refresh to the given revision"
__ghoti_snap_option refresh -l devmode -d "Put snap in development mode and disable security confinement"
__ghoti_snap_option refresh -l jailmode -d "Put snap in enforced confinement mode"
__ghoti_snap_option refresh -l classic -d "Put snap in classic mode and disable security confinement"
__ghoti_snap_option refresh -l ignore-validation -d "Ignore validation by other snaps blocking the refresh"
__ghoti_snap_option refresh -a '(__ghoti_snap_installed_snaps)' -d Snap

# Remove
__ghoti_snap_subcommand remove -r -d "Removes a snap from the system"
__ghoti_snap_option remove -l revision -d "Removes only the given revision"
__ghoti_snap_option remove -a '(__ghoti_snap_installed_snaps)' -d Snap

# Revert
__ghoti_snap_subcommand revert -r -d "Revert the given snap to the previous state"
__ghoti_snap_option refresh -l revision -d "Revert to the given revision"
__ghoti_snap_option refresh -l devmode -d "Put snap in development mode and disable security confinement"
__ghoti_snap_option refresh -l jailmode -d "Put snap in enforced confinement mode"
__ghoti_snap_option refresh -l classic -d "Put snap in classic mode and disable security confinement"
__ghoti_snap_option revert -a '(__ghoti_snap_installed_snaps)' -d Snap

# Run
__ghoti_snap_subcommand run -r -d "Run the given snap command"
__ghoti_snap_option run -l shell -d "Run a shell instead of the command (useful for debugging)"
__ghoti_snap_option run -a '(__ghoti_snap_installed_snaps)' -d Snap

# There seems to be no programmatic way of getting configuration options
# Set
__ghoti_snap_subcommand set -r -d "Changes configuration options"
__ghoti_snap_option set -a '(__ghoti_snap_installed_snaps)' -d Snap

# Tasks
__ghoti_snap_subcommand tasks -d "List a change's tasks"
__ghoti_snap_option tasks -a '(__ghoti_snap_change_id)' -d ID

# Try
__ghoti_snap_subcommand try -r -d "Tests a snap in the system"
__ghoti_snap_option try -l devmode -d "Put snap in development mode and disable security confinement"
__ghoti_snap_option try -l jailmode -d "Put snap in enforced confinement mode"
__ghoti_snap_option try -l classic -d "Put snap in classic mode and disable security confinement"

# Unalias
__ghoti_snap_subcommand unalias -r -d "Unalias a manual alias or an entire snap"
__ghoti_snap_option unalias -a '(__ghoti_snap_aliases)' -d "Alias or snap"

# Version
__ghoti_snap_subcommand version -d "Shows version details"

# Watch
__ghoti_snap_subcommand watch -d "Watch a change in progress"
__ghoti_snap_option watch -a '(__ghoti_snap_change_id)' -d ID
