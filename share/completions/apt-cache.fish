#apt-cache
complete -c apt-cache -s h -l help -d "Display help and exit"
complete -f -c apt-cache -a gencaches -d "Build apt cache"
complete -x -c apt-cache -a showpkg -d "Show package info"
complete -f -c apt-cache -a stats -d "Show cache statistics"
complete -x -c apt-cache -a showsrc -d "Show source package"
complete -f -c apt-cache -a dump -d "Show packages in cache"
complete -f -c apt-cache -a dumpavail -d "Print available list"
complete -f -c apt-cache -a unmet -d "List unmet dependencies in cache"
complete -x -c apt-cache -a show -d "Display package record"
complete -x -c apt-cache -a search -d "Search packagename by REGEX"
complete -c apt-cache -l full -a search -d "Search full package name"
complete -x -c apt-cache -l names-only -a search -d "Search packagename only"
complete -x -c apt-cache -a depends -d "List dependencies for the package"
complete -x -c apt-cache -a rdepends -d "List reverse dependencies for the package"
complete -x -c apt-cache -a pkgnames -d "Print package name by prefix"
complete -x -c apt-cache -a dotty -d "Generate dotty output for packages"
complete -x -c apt-cache -a policy -d "Debug preferences file"
complete -r -c apt-cache -s p -l pkg-cache -d "Select file to store package cache"
complete -r -c apt-cache -s s -l src-cache -d "Select file to store source cache"
complete -f -c apt-cache -s q -l quiet -d "Quiet mode"
complete -f -c apt-cache -s i -l important -d "Print important dependencies"
complete -f -c apt-cache -s a -l all-versions -d "Print full records"
complete -f -c apt-cache -s g -l generate -d "Auto-gen package cache"
complete -f -c apt-cache -l all-names -d "Print all names"
complete -f -c apt-cache -l recurse -d "Dep and rdep recursive"
complete -f -c apt-cache -l installed -d "Limit to installed"
complete -f -c apt-cache -s v -l version -d "Display version and exit"
complete -r -c apt-cache -s c -l config-file -d "Specify config file"
complete -x -c apt-cache -s o -l option -d "Specify options"

function __fish_apt-cache_use_package -d 'Test if apt command should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i contains show showpkg showsrc depends rdepends dotty policy
            return 0
        end
    end
    return 1
end

complete -c apt-cache -n __fish_apt-cache_use_package -a '(__fish_print_apt_packages)' -d Package
