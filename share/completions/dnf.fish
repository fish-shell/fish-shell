#
# Completions for the dnf command
#

# All dnf commands

# Test if completing using package names is appropriate
function __fish_dnf_package_installed
	for i in (commandline -poc)
		if contains -- $i update upgrade remove erase reinstall
			return 0
		end
	end
	return 1
end


function __fish_dnf_package_notinstalled
	for i in (commandline -poc)
		if contains -- $i install
			return 0
		end
	end
	return 1
end

# Installed packages
function __dnf_installed
	sqlite3 /var/cache/dnf/packages.db "select pkg from installed WHERE pkg LIKE \"$cur%\"" 2>/dev/null
	return
end

# Available packages
function __dnf_available
	sqlite3 /var/cache/dnf/packages.db "select available.pkg from available left join installed on available.pkg = installed.pkg where installed.pkg IS NULL and available.pkg LIKE \"$cur%\"" 2>/dev/null
	return
end


complete -c dnf -n '__fish_use_subcommand' -xa install --description "Install the latest version of a package"
complete -c dnf -n '__fish_use_subcommand' -xa 'update upgrade' --description "Update specified packages (defaults to all packages)"
complete -c dnf -n '__fish_use_subcommand' -xa check-update --description "Print list of available updates"
complete -c dnf -n '__fish_use_subcommand' -xa 'remove erase' --description "Remove the specified packages and packages that depend on them"
complete -c dnf -n '__fish_use_subcommand' -xa list --description "List available packages"
complete -c dnf -n '__fish_use_subcommand' -xa info --description "Describe available packages"
complete -c dnf -n '__fish_use_subcommand' -xa 'provides whatprovides' --description "Find package providing a feature or file"
complete -c dnf -n '__fish_use_subcommand' -xa search --description "find packages matching description regexp"
complete -c dnf -n '__fish_use_subcommand' -xa clean --description "Clean up cache directory"
complete -c dnf -n '__fish_use_subcommand' -xa generate-rss --description "Generate rss changelog"

complete -c dnf -n '__fish_dnf_package_installed' -a "(__dnf_installed)"
complete -c dnf -n '__fish_dnf_package_notinstalled' -a "(__dnf_available)"

complete -c dnf -s h -l help --description "Display help and exit"
complete -c dnf -s y --description "Assume yes to all questions"
complete -c dnf -s c --description "Configuration file" -r
complete -c dnf -s d --description "Set debug level" -x
complete -c dnf -s e --description "Set error level" -x
complete -c dnf -s t -l tolerant --description "Be tolerant of errors in commandline"
complete -c dnf -s R --description "Set maximum delay between commands" -x
complete -c dnf -s c --description "Run commands from cache"
complete -c dnf -l version --description "Display version and exit"
complete -c dnf -l installroot --description "Specify installroot" -r
complete -c dnf -l enablerepo --description "Enable repository" -r
complete -c dnf -l disablerepo --description "Disable repository" -r
complete -c dnf -l obsoletes --description "Enables obsolets processing logic"
complete -c dnf -l rss-filename --description "Output rss-data to file" -r
complete -c dnf -l exclude --description "Exclude specified package from updates" -a "(__fish_print_packages)"

complete -c dnf -n 'contains list (commandline -poc)' -a all --description 'List all packages'
complete -c dnf -n 'contains list (commandline -poc)' -a available --description 'List packages available for installation'
complete -c dnf -n 'contains list (commandline -poc)' -a updates --description 'List packages with updates available'
complete -c dnf -n 'contains list (commandline -poc)' -a installed --description 'List installed packages'
complete -c dnf -n 'contains list (commandline -poc)' -a extras --description 'List packages not available in repositories'
complete -c dnf -n 'contains list (commandline -poc)' -a obsoletes --description 'List packages that are obsoleted by packages in repositories'

complete -c dnf -n 'contains clean (commandline -poc)' -x -a packages --description 'Delete cached package files'
complete -c dnf -n 'contains clean (commandline -poc)' -x -a headers --description 'Delete cached header files'
complete -c dnf -n 'contains clean (commandline -poc)' -x -a all --description 'Delete all cache contents'
