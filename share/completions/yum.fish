#
# Completions for the yum command
#

# All yum commands

# Test if completing using package names is appropriate
function __fish_yum_package_ok
	for i in (commandline -poc)
		if contains $i update upgrade remove erase
			return 0
		end
	end
	return 1
end

complete -c yum -n '__fish_use_subcommand' -xa install --description "Install the latest version of a package"
complete -c yum -n '__fish_use_subcommand' -xa 'update upgrade' --description "Update specified packages (defaults to all packages)"
complete -c yum -n '__fish_use_subcommand' -xa check-update --description "Print list of available updates"
complete -c yum -n '__fish_use_subcommand' -xa 'remove erase' --description "Remove the specified packages and packages that depend on them"
complete -c yum -n '__fish_use_subcommand' -xa list --description "List available packages"
complete -c yum -n '__fish_use_subcommand' -xa info --description "Describe available packages"
complete -c yum -n '__fish_use_subcommand' -xa 'provides whatprovides' --description "Find package providing a feature or file"
complete -c yum -n '__fish_use_subcommand' -xa search --description "find packages matching description regexp"
complete -c yum -n '__fish_use_subcommand' -xa clean --description "Clean up cache directory"
complete -c yum -n '__fish_use_subcommand' -xa generate-rss --description "Generate rss changelog"

complete -c yum -n '__fish_yum_package_ok' -a "(__fish_print_packages)"

complete -c yum -s h -l help --description "Display help and exit"
complete -c yum -s y --description "Assume yes to all questions"
complete -c yum -s c --description "Configuration file" -r
complete -c yum -s d --description "Set debug level" -x
complete -c yum -s e --description "Set error level" -x
complete -c yum -s t -l tolerant --description "Be tolerant of errors in commandline"
complete -c yum -s R --description "Set maximum delay between commands" -x
complete -c yum -s c --description "Run commands from cache"
complete -c yum -l version --description "Display version and exit"
complete -c yum -l installroot --description "Specify installroot" -r
complete -c yum -l enablerepo --description "Enable repository" -r
complete -c yum -l disablerepo --description "Disable repository" -r
complete -c yum -l obsoletes --description "Enables obsolets processing logic"
complete -c yum -l rss-filename --description "Output rss-data to file" -r
complete -c yum -l exclude --description "Exclude specified package from updates" -a "(__fish_print_packages)"

complete -c yum -n 'contains list (commandline -poc)' -a all --description 'List all packages'
complete -c yum -n 'contains list (commandline -poc)' -a available --description 'List packages available for installation'
complete -c yum -n 'contains list (commandline -poc)' -a updates --description 'List packages with updates available'
complete -c yum -n 'contains list (commandline -poc)' -a installed --description 'List installed packages'
complete -c yum -n 'contains list (commandline -poc)' -a extras --description 'List packages not available in repositories'
complete -c yum -n 'contains list (commandline -poc)' -a obsoletes --description 'List packages that are obsoleted by packages in repositories'

complete -c yum -n 'contains clean (commandline -poc)' -x -a packages --description 'Delete cached package files'
complete -c yum -n 'contains clean (commandline -poc)' -x -a headers --description 'Delete cached header files'
complete -c yum -n 'contains clean (commandline -poc)' -x -a all --description 'Delete all cache contents'
