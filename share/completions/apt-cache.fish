#apt-cache
complete -c apt-cache -s h -l help --description "Display help and exit"
complete -f -c apt-cache -a gencaches --description "Build apt cache"
complete -x -c apt-cache -a showpkg --description "Show package info"
complete -f -c apt-cache -a stats --description "Show cache statistics"
complete -x -c apt-cache -a showsrc --description "Show source package"
complete -f -c apt-cache -a dump --description "Show packages in cache"
complete -f -c apt-cache -a dumpavail --description "Print available list"
complete -f -c apt-cache -a unmet --description "List unmet dependencies in cache"
complete -x -c apt-cache -a show --description "Display package record"
complete -x -c apt-cache -a search --description "Search packagename by REGEX"
complete -c apt-cache -l full -a search --description "Search full package name"
complete -x -c apt-cache -l names-only -a search --description "Search packagename only"
complete -x -c apt-cache -a depends --description "List dependencies for the package"
complete -x -c apt-cache -a rdepends --description "List reverse dependencies for the package"
complete -x -c apt-cache -a pkgnames --description "Print package name by prefix"
complete -x -c apt-cache -a dotty --description "Generate dotty output for packages"
complete -x -c apt-cache -a policy --description "Debug preferences file"
complete -r -c apt-cache -s p -l pkg-cache --description "Select file to store package cache"
complete -r -c apt-cache -s s -l src-cache --description "Select file to store source cache"
complete -f -c apt-cache -s q -l quiet --description "Quiet mode"
complete -f -c apt-cache -s i -l important --description "Print important dependencies"
complete -f -c apt-cache -s a -l all-versions --description "Print full records"
complete -f -c apt-cache -s g -l generate --description "Auto-gen package cache"
complete -f -c apt-cache -l all-names --description "Print all names"
complete -f -c apt-cache -l recurse --description "Dep and rdep recursive"
complete -f -c apt-cache -l installed --description "Limit to installed"
complete -f -c apt-cache -s v -l version --description "Display version and exit"
complete -r -c apt-cache -s c -l config-file --description "Specify config file"
complete -x -c apt-cache -s o -l option --description "Specify options"

function __fish_apt-cache_use_package --description 'Test if apt command should have packages as potential completion'
	for i in (commandline -opc)
		if contains -- $i contains show showpkg showsrc depends rdepends dotty policy
			return 0
		end
	end
	return 1
end

complete -c apt-cache -n '__fish_apt-cache_use_package' -a '(__fish_print_packages)' --description 'Package'

