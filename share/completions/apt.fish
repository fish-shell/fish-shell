#!/usr/bin/fish
# Completions for the new `apt` command


function __fish_apt_no_subcommand --description 'Test if apt has yet to be given the subcommand'
	for i in (commandline -opc)
		if contains -- $i update upgrade full-upgrade search list install show remove edit-sources
			return 1
		end
	end
	return 0
end

function __fish_apt_use_package --description 'Test if apt command should have packages as potential completion'
	for i in (commandline -opc)
		if contains -- $i contains install remove upgrade full-upgrade show search
			return 0
		end
	end
	return 1
end

function apt_subcommand 
    set subcommand $argv[1]; set -e argv[1]
    complete -f -c apt -n '__fish_apt_no_subcommand' -a $subcommand $argv
end

complete -c apt -n '__fish_apt_use_package' -a '(__fish_print_packages)' --description 'Package'

# Support flags
complete -x -f -c apt -s h -l help     --description 'Display help'
complete -x -f -c apt -s v -l version  --description 'Display version and exit'

# General options
complete -f -c apt -s o -l option       --description 'Set a configuration option'
complete -f -c apt -s c -l config-file  --description 'Configuration file'
complete -f -c apt -s t                 --description 'Target release'

# List
apt_subcommand list                  --description 'List packages'
apt_subcommand list -l installed     --description 'Installed packages'
apt_subcommand list -l upgradable    --description 'Upgradable packages'
apt_subcommand list -l all-versions  --description 'Show all versions of any package'

# Search
apt_subcommand search -r        --description 'Search for packages'

# Search
apt_subcommand show -r          --description 'Show package information'

# Install
apt_subcommand install -r       --description 'Install packages'

# Remove
apt_subcommand remove -r        --description 'Remove packages'

# Edit sources
apt_subcommand edit-sources     --description 'Edit sources list'

# Update
apt_subcommand update -x        --description 'Update package list'

# Upgrade
apt_subcommand upgrade -r       --description 'Upgrade packages'

# Full Upgrade
apt_subcommand full-upgrade -r  --description 'Upgrade packages, removing others when needed'
