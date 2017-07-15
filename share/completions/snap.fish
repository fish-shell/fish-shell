# Completions for `snap` command

function __fish_snap_no_subcommand --description 'Test if snap has yet to be given the subcommand'
	for i in (commandline -opc)
		if contains -- $i abort ack alias aliases buy changes connect disable disconnect download\
            enable find get help info install interfaces known list login logout prefer refresh remove\
            revert run set tasks try unalias version watch 
			return 1
		end
	end
	return 0
end

function __fish_snap_using_subcommand --description 'Test if given subcommand is used'
    for i in (commandline -opc)
        if contains -- $i $argv[1]
            return 0
        end
    end
    return 1
end

function __fish_snap_use_package --description 'Test if snap command should have packages as potential completion'
	for i in (commandline -opc)
		if contains -- $i alias buy disable download enable info install refresh remove revert run try
			return 0
		end
	end
	return 1
end

function __fish_snap_use_file --description 'Test if snap command should have files as potential completion'
	for i in (commandline -opc)
		if contains -- $i ack try
			return 0
		end
	end
	return 1
end

function __fish_snap_subcommand
    set subcommand $argv[1]; set -e argv[1]
    complete -f -c snap -n '__fish_snap_no_subcommand' -a $subcommand $argv
end

function __fish_snap_disabled_snaps --description 'List disabled snaps'
    snap list | grep disabled | cut -d ' ' -f 1
end

function __fish_snap_enabled_snaps --description 'List disabled snaps'
    # Hack until fish supports array[2..-1]
    snap list | grep -v disabled | cut -d ' ' -f 1 | grep -v Name
end

function __fish_snap_installed_snaps --description 'List installed snaps'
    # Hack until fish supports array[2..-1]
    snap list | cut -d ' ' -f 1 | grep -v Name
end

function __fish_snap_connected_snaps --description 'List connected snaps'
    snap interfaces | tr -s ' ' | cut -d ' ' -f 2 | sort -u | grep -v -E '^-|Plug'
end

function __fish_snap_change_id --description 'List change IDs'
    snap changes | cut -d ' ' -f 1 | grep -v 'ID'
end

function __fish_snap_aliases --description 'List aliases'
    snap aliases | cut -d ' ' -f 1 | grep -v Command
    snap aliases | tr -s ' ' | cut -d ' ' -f 2 | grep -v Alias
end

function __fish_snap_no_assertion --description 'Check that no assertion type is used yet'
	for i in (commandline -opc)
		if contains -- $i account account-key model serial snap-declaration snap-build snap-revision\
            system-user validation
			return 1
		end
	end
	return 0
end

function __fish_snap_using_assertion --description 'Check if certain assertion type is used'
    if __fish_snap_using_subcommand known
        if __fish_snap_using_subcommand $argv[1]
            return 0
        end
    end
    return 1
end

function __fish_snap_assertion
    set assertion $argv[1]; set -e argv[1]
    complete -f -c snap -n '__fish_snap_using_subcommand known; and __fish_snap_no_assertion' -a $assertion
    complete -f -c snap -n "__fish_snap_using_assertion $assertion" -a "(__fish_snap_filters $assertion)"\
        --description "Filter"
end

function __fish_snap_filters --description 'List assertion filters'
    snap known $argv[1] | grep : | grep -v 'type: ' | sed -e 's/: /=/1'
end

# Enable when __fish_print_packages supports snaps
#complete -c snap -n '__fish_snap_use_package' -a '(__fish_print_packages)' --description 'Package'

# Enable file completions where appropriate
complete -c snap -n '__fish_snap_use_file' -a '(__fish_complete_path)'

# Support flags
complete -x -f -c snap -s h -l help     --description 'Show this help message'
complete -x -f -c snap -s v -l version  --description 'Print the version and exit'

# Abort
__fish_snap_subcommand abort                    --description "Abort a pending change"

# Ack
__fish_snap_subcommand ack -r                   --description "Adds an assertion to the system"

# Alias
__fish_snap_subcommand alias -r                 --description "Sets up a manual alias"
complete -f -c snap -n '__fish_snap_using_subcommand alias' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Aliases
__fish_snap_subcommand aliases                  --description "Lists aliases in the system"

# Buy
__fish_snap_subcommand buy -r                   --description "Buys a snap"

# Changes
__fish_snap_subcommand changes                  --description "List system changes"

# TODO connect to interface
# Connect
__fish_snap_subcommand connect -r               --description "Connects a plug to a slot"
complete -f -c snap -n '__fish_snap_using_subcommand connect' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Disable
__fish_snap_subcommand disable -r               --description "Disables a snap in the system"
complete -f -c snap -n '__fish_snap_using_subcommand disable' -a '(__fish_snap_enabled_snaps)' --description "Enabled snap"

# TODO disconnect from interface
# Disconnect
__fish_snap_subcommand disconnect -r            --description "Disconnects a plug from a slot"
complete -f -c snap -n '__fish_snap_using_subcommand disconnect' -a '(__fish_snap_connected_snaps)' --description "Snap"

# Downloads
__fish_snap_subcommand download -r              --description "Downloads the given snap"
__fish_snap_subcommand download -l channel      --description "Use this channel instead of stable"
__fish_snap_subcommand download -l edge         --description "Install from the edge channel"
__fish_snap_subcommand download -l beta         --description "Install from the beta channel"
__fish_snap_subcommand download -l candidate    --description "Install from the candidate channel"
__fish_snap_subcommand download -l stable       --description "Install from the stable channel"
__fish_snap_subcommand download -l revision     --description "Download the given revision of snap, to which you must have developer access"

# Enable
__fish_snap_subcommand enable -r                --description "Enables a snap in the system"
complete -f -c snap -n '__fish_snap_using_subcommand enable' -a '(__fish_snap_disabled_snaps)' --description "Disabled snap"

# Find
__fish_snap_subcommand find -r                  --description "Finds packages to install"
__fish_snap_subcommand find -l private          --description "Search private snaps"
__fish_snap_subcommand find -l section          --description "Restrict the search to a given section"

# TODO Get a list of configuration options
# Get
__fish_snap_subcommand get -r                   --description "Prints configuration options"
__fish_snap_subcommand get -s t                 --description "Strict typing with nulls and quoted strings"
__fish_snap_subcommand get -s d                 --description "Always return documents, even with single key"
complete -f -c snap -n '__fish_snap_using_subcommand get' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Help
__fish_snap_subcommand help                     --description "The help command shows useful information"
__fish_snap_subcommand help -l man              --description "Generates the manpage"

# Info
__fish_snap_subcommand info -r                  --description "Show detailed information about a snap"
__fish_snap_subcommand info -l verbose          --description "Include a verbose list of snap's notes"
complete -f -c snap -n '__fish_snap_using_subcommand info' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Install
__fish_snap_subcommand install -r               --description "Installs a snap to the system"
__fish_snap_subcommand install -l channel       --description "Use this channel instead of stable"
__fish_snap_subcommand install -l edge          --description "Install from the edge channel"
__fish_snap_subcommand install -l beta          --description "Install from the beta channel"
__fish_snap_subcommand install -l candidate     --description "Install from the candidate channel"
__fish_snap_subcommand install -l stable        --description "Install from the stable channel"
__fish_snap_subcommand install -l revision      --description "Install the given revision of snap, to which you must have developer access"
__fish_snap_subcommand install -l devmode       --description "Put snap in development mode and disable security confinement"
__fish_snap_subcommand install -l jailmode      --description "Put snap in enforced confinement mode"
__fish_snap_subcommand install -l classic       --description "Put snap in classic mode and disable security confinement"
__fish_snap_subcommand install -l dangerous     --description "Install the given snap file even if there are no pre-acknowledged signatures for it, meaning it was not  verified and could be dangerous"

# Interfaces
__fish_snap_subcommand interfaces               --description "Lists interfaces in the system"
__fish_snap_subcommand interfaces -s i          --description "Constrain listing to specific interfaces"

# Known
__fish_snap_subcommand known -r                 --description "Shows known assertions of the provided type"
__fish_snap_subcommand known -l remote          --description "Shows known assertions of the provided type"
__fish_snap_assertion account                   --description 'Assertion type'
__fish_snap_assertion account-key               --description 'Assertion type'
__fish_snap_assertion model                     --description 'Assertion type'
__fish_snap_assertion serial                    --description 'Assertion type'
__fish_snap_assertion snap-declaration          --description 'Assertion type'
__fish_snap_assertion snap-build                --description 'Assertion type'
__fish_snap_assertion snap-revision             --description 'Assertion type'
__fish_snap_assertion system-user               --description 'Assertion type'
__fish_snap_assertion validation                --description 'Assertion type'

# List
__fish_snap_subcommand list                     --description "List installed snaps"
__fish_snap_subcommand list -l all              --description "Show all revisions"

# Login
__fish_snap_subcommand login                    --description "Authenticates on snapd and the store"

# Logout
__fish_snap_subcommand logout                   --description "Log out of the store"

# Prefer
__fish_snap_subcommand prefer -r                --description "Prefes aliases from a snap and disable conflicts"
complete -f -c snap -n '__fish_snap_using_subcommand prefer' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Refresh
__fish_snap_subcommand refresh -r               --description "Refreshes a snap in the system"
__fish_snap_subcommand refresh -l channel       --description "Use this channel instead of stable"
__fish_snap_subcommand refresh -l edge          --description "Install from the edge channel"
__fish_snap_subcommand refresh -l beta          --description "Install from the beta channel"
__fish_snap_subcommand refresh -l candidate     --description "Install from the candidate channel"
__fish_snap_subcommand refresh -l stable        --description "Install from the stable channel"
__fish_snap_subcommand refresh -l revision      --description "Refresh to the given revision"
__fish_snap_subcommand refresh -l devmode       --description "Put snap in development mode and disable security confinement"
__fish_snap_subcommand refresh -l jailmode      --description "Put snap in enforced confinement mode"
__fish_snap_subcommand refresh -l classic       --description "Put snap in classic mode and disable security confinement"
__fish_snap_subcommand refresh -l ignore-validation --description "Ignore validation by other snaps blocking the refresh"
complete -f -c snap -n '__fish_snap_using_subcommand refresh' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Remove
__fish_snap_subcommand remove -r                --description "Removes a snap from the system"
__fish_snap_subcommand remove -l revision       --description "Removes only the given revision"
complete -f -c snap -n '__fish_snap_using_subcommand remove' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Revert
__fish_snap_subcommand revert -r                --description "Revert the given snap to the previous state"
__fish_snap_subcommand refresh -l revision      --description "Revert to the given revision"
__fish_snap_subcommand refresh -l devmode       --description "Put snap in development mode and disable security confinement"
__fish_snap_subcommand refresh -l jailmode      --description "Put snap in enforced confinement mode"
__fish_snap_subcommand refresh -l classic       --description "Put snap in classic mode and disable security confinement"
complete -f -c snap -n '__fish_snap_using_subcommand revert' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Run
__fish_snap_subcommand run -r                   --description "Run the given snap command"
__fish_snap_subcommand run -l shell             --description "Run a shell instead of the command (useful for debugging)"
complete -f -c snap -n '__fish_snap_using_subcommand run' -a '(__fish_snap_installed_snaps)' --description "Snap"

# TODO Get a list of config values
# Set
__fish_snap_subcommand set -r                   --description "Changes configuration options"
complete -f -c snap -n '__fish_snap_using_subcommand set' -a '(__fish_snap_installed_snaps)' --description "Snap"

# Tasks
__fish_snap_subcommand tasks                    --description "List a change's tasks"
complete -f -c snap -n '__fish_snap_using_subcommand tasks' -a '(__fish_snap_change_id)' --description "ID"

# Try
__fish_snap_subcommand try -r                   --description "Tests a snap in the system"
__fish_snap_subcommand try -l devmode           --description "Put snap in development mode and disable security confinement"
__fish_snap_subcommand try -l jailmode          --description "Put snap in enforced confinement mode"
__fish_snap_subcommand try -l classic           --description "Put snap in classic mode and disable security confinement"

# Unalias
__fish_snap_subcommand unalias -r               --description "Unalias a manual alias or an entire snap"
complete -f -c snap -n '__fish_snap_using_subcommand unalias' -a '(__fish_snap_aliases)' --description "Alias or snap"

# Version
__fish_snap_subcommand version                  --description "Shows version details"

# Watch
__fish_snap_subcommand watch                    --description "Watch a change in progress"
complete -f -c snap -n '__fish_snap_using_subcommand watch' -a '(__fish_snap_change_id)' --description "ID"
