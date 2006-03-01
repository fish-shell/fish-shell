#
# Completions for the yum command
#

#All yum commands

#Test if the yum command has been specified
function __fish_yum_has_command 
	set modes install update check-update upgrade remove erase list provides whatprovides search info clean generate-rss
	for i in (commandline -poc);
		if contains $i $modes
			return 1
		end
	end
	return 0;
end

#Test if completing using package names is appropriate
function __fish_yum_package_ok
	for i in (commandline -poc)
		if contains $i update upgrade remove erase 
			return 0
		end
	end
	return 1
end

complete -c yum -n '__fish_yum_has_command' -xa "
    install\t'Install the latest version of a package'
    update\t'Update specified packages (defaults to all packages)'
    check-update\t'Print list of available updates'
    upgrade\t'Update specified packages including obsoletes (defaults to all packages)'
    remove\t'remove the specified packages and packages that depend on them'
	erase\t'remove the specified packages and packages that depend on them'
	list\t'List information about avaialble packages'
	provides\t'Find package providing a feature or file'
	whatprovides\t'Find package providing a feature or file'
    search\t'find packages matching description regexp'
    info\t'List information about available packages'
    clean\t'Clean up cache directory'
    generate-rss\t'Generate rss changelog'
"

complete -c yum -n '__fish_yum_package_ok' -a "(__fish_print_packages)"

complete -c yum -s h -l help -d (N_ "Display help and exit") 
complete -c yum -s y -d (N_ "Assume yes to all questions") 
complete -c yum -s c -d (N_ "Configuration file") -r 
complete -c yum -s d -d (N_ "Set debug level") -x 
complete -c yum -s e -d (N_ "Set error level") -x 
complete -c yum -s t -l tolerant -d (N_ "Be tolerant of errors in commandline") 
complete -c yum -s R -d (N_ "Set maximum delay between commands") -x
complete -c yum -s c -d (N_ "Run commands from cache") 
complete -c yum -l version -d (N_ "Display version and exit") 
complete -c yum -l installroot -d (N_ "Specify installroot") -r 
complete -c yum -l enablerepo -d (N_ "Enable repository") -r 
complete -c yum -l disablerepo -d (N_ "Disable repository") -r
complete -c yum -l obsoletes -d (N_ "Enables obsolets processing logic") 
complete -c yum -l rss-filename -d (N_ "Output rss-data to file") -r 
complete -c yum -l exclude -d (N_ "Exclude specified package from updates") -a "(__fish_print_packages)" 

complete -c yum -n 'contains list (commandline -poc)' -a "
	all\t'List all packages'
	available\t'List packages available for installation'
	updates\t'List packages with updates available'
	installed\t'List installed packages'
	extras\t'List packages not available in repositories'
	obsoletes\t'List packages that are obsoleted by packages in repositories'
"

complete -c yum -n 'contains clean (commandline -poc)' -x -a "
	packages\t'Delete cached package files'
	headers\t'Delete cached header files'
	all\t'Delete all cache contents'
"
