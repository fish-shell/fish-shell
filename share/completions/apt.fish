# Completions for the `apt` command

set -l pkg_subcmds install remove upgrade full-upgrade show search purge changelog policy depends rdepends

function __fish_apt_no_subcommand -d 'Test if apt has yet to be given the subcommand'
	for i in (commandline -opc)
		if contains -- $i update upgrade full-upgrade search list install show remove edit-sources purge changelog autoremove depends rdepends
			return 1
		end
	end
	return 0
end

function __fish_apt_subcommand
    set subcommand $argv[1]
    set -e argv[1]
    complete -f -c apt -n '__fish_apt_no_subcommand' -a $subcommand $argv
end

function __fish_apt_using_subcommand -d 'Test if given subcommand is used'
    for i in (commandline -opc)
        if contains -- $i $argv
            return 0
        end
    end
    return 1
end

function __fish_apt_option
    set subcommand $argv[1]
    set -e argv[1]
    complete -f -c apt -n "__fish_seen_subcommand_from $subcommand" $argv
end

complete -c apt -n '__fish_seen_subcommand_from $pkg_subcmds' -a '(__fish_print_packages)' -d 'Package'

# Support flags
complete -x -f -c apt -s h -l help     -d 'Display help'
complete -x -f -c apt -s v -l version  -d 'Display version and exit'

# General options
complete -f -c apt -s o -l option       -d 'Set a configuration option'
complete -f -c apt -s c -l config-file  -d 'Configuration file'
complete -f -c apt -s t                 -d 'Target release'

# List
__fish_apt_subcommand list                  -d 'List packages'
__fish_apt_option     list -l installed     -d 'Installed packages'
__fish_apt_option     list -l upgradable    -d 'Upgradable packages'
__fish_apt_option     list -l all-versions  -d 'Show all versions of any package'

# Search
__fish_apt_subcommand search -r        -d 'Search for packages'

# Search
__fish_apt_subcommand show -r          -d 'Show package information'

# Install
__fish_apt_subcommand install -r       -d 'Install packages'

# Remove
__fish_apt_subcommand remove -r        -d 'Remove packages'

# Edit sources
__fish_apt_subcommand edit-sources     -d 'Edit sources list'

# Update
__fish_apt_subcommand update -x        -d 'Update package list'

# Upgrade
__fish_apt_subcommand upgrade -r       -d 'Upgrade packages'

# Full Upgrade
__fish_apt_subcommand full-upgrade -r  -d 'Upgrade packages, removing others when needed'

# Purge
__fish_apt_subcommand purge -x         -d 'Remove packages and delete their config files'

# Changelog
__fish_apt_subcommand changelog -r     -d 'Download and display package changelog'

# Autoremove
__fish_apt_subcommand autoremove       -d 'Remove packages no longer needed as dependencies'

# Policy
__fish_apt_subcommand policy -x        -d 'Display source or package priorities'

# Depends
__fish_apt_subcommand depends -r       -d 'List package dependencies'

# Rdepends
__fish_apt_subcommand rdepends -r      -d 'List package reverse dependencies'
