#completion for prt-get
# A function to verify if prt-get (the crux package management tool) needs to be completed by a further command

# a function to obtain a list of installed packages with prt-get
function __fish_prt_packages -d 'Obtain a list of installed packages'
    prt-get listinst
end

# a function to obtain a list of ports with prt-get

function __fish_prt_ports -d 'Obtain a list of ports'
    prt-get list
end

function __fish_prt_no_subcommand -d 'Test if prt-get has yet to be given the command'
    for i in (commandline -xpc)
        if contains -- $i install depinst grpinst update remove sysup lock unlock listlocked diff quickdiff search dsearch fsearch info path readme depends quickdep dependent deptree dup list printf listinst listorphans isinst current ls cat edit help dumpconfig version cache
            return 1
        end
    end
    return 0
end

# a function to verify if prt-get should have packages as potential completion
function __fish_prt_use_package -d 'Test if prt-get should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i update remove lock unlock current
            return 0
        end
    end
    return 1
end

# a function to test if prt-get should have ports as potential completions
function __fish_prt_use_port -d 'Test if prt-get should have ports as potential completion'
    for i in (commandline -xpc)
        if contains -- $i install depinst grpinst diff depends quickdep dependent deptree isinst info path readme ls cat edit
            return 0
        end
    end
    return 1
end

complete -f -c prt-get -n __fish_prt_use_package -a '(__fish_prt_packages)' -d Package
complete -f -c prt-get -n __fish_prt_use_port -a '(__fish_prt_ports)' -d Port

complete -f -n __fish_prt_no_subcommand -c prt-get -a install -d 'Install listed ports'
complete -f -n __fish_prt_no_subcommand -c prt-get -a depinst -d 'Install listed ports and their deps'
complete -f -n __fish_prt_no_subcommand -c prt-get -a grpinst -d 'Install listed ports, stop if one fails'
complete -f -n __fish_prt_no_subcommand -c prt-get -a update -d 'Update listed packages'
complete -f -n __fish_prt_no_subcommand -c prt-get -a remove -d 'Remove listed packages'
complete -f -n __fish_prt_no_subcommand -c prt-get -a sysup -d 'Update all outdated installed packages'
complete -f -n __fish_prt_no_subcommand -c prt-get -a lock -d 'Do not update this in sysup'
complete -f -n __fish_prt_no_subcommand -c prt-get -a unlock -d 'Remove this from lock'
complete -f -n __fish_prt_no_subcommand -c prt-get -a diff -d 'Print differences between installed packages and ports in the port tree'
complete -f -n __fish_prt_no_subcommand -c prt-get -a search -d 'Search for an expr in port names'
complete -f -n __fish_prt_no_subcommand -c prt-get -a dsearch -d 'Search for an expr in port names and descriptions'
complete -f -n __fish_prt_no_subcommand -c prt-get -a info -d 'Print info on a port'
complete -f -n __fish_prt_no_subcommand -c prt-get -a fsearch -d 'Search for a pattern in the footprints in the ports tree'
complete -f -n __fish_prt_no_subcommand -c prt-get -a path -d 'Print the path of a port'
complete -f -n __fish_prt_no_subcommand -c prt-get -a readme -d 'Print the eventual README of a port'
complete -f -n __fish_prt_no_subcommand -c prt-get -a depends -d 'Print a list of deps for the listed ports'
complete -f -n __fish_prt_no_subcommand -c prt-get -a quickdeo -d 'Print a simple list of deps for the listed ports'
complete -f -n __fish_prt_no_subcommand -c prt-get -a deptree -d 'Print a deptree for the port'
complete -f -n __fish_prt_no_subcommand -c prt-get -a dup -d 'List ports in multiple directories'
complete -f -n __fish_prt_no_subcommand -c prt-get -a list -d 'List all the ports'
complete -f -n __fish_prt_no_subcommand -c prt-get -a printf -d 'Print formatted list of ports'
complete -f -n __fish_prt_no_subcommand -c prt-get -a listinst -d 'List installed packages'
complete -f -n __fish_prt_no_subcommand -c prt-get -a listorphans -d 'List installed packages which have no dependent packages'
complete -f -n __fish_prt_no_subcommand -c prt-get -a isinst -d 'Check if a port is installed'
complete -f -n __fish_prt_no_subcommand -c prt-get -a current -d 'Print the version of an installed package'
complete -f -n __fish_prt_no_subcommand -c prt-get -a ls -d 'Print the listing of the directory of a port'
complete -f -n __fish_prt_no_subcommand -c prt-get -a cat -d 'Print a file in a port to stdout'
complete -f -n __fish_prt_no_subcommand -c prt-get -a edit -d 'Edit a file in a port'
complete -f -n __fish_prt_no_subcommand -c prt-get -a help -d 'Shows a help screen'
complete -f -n __fish_prt_no_subcommand -c prt-get -a dumpconfig -d 'Print the configuration of prt-get'
complete -f -n __fish_prt_no_subcommand -c prt-get -a version -d 'Show the current version of prt-get'
complete -f -n __fish_prt_no_subcommand -c prt-get -a cache -d 'Create a cache for prt-get'
complete -c prt-get -o f -o i -d 'Force install'
complete -c prt-get -o fr -d 'Force rebuild'
complete -c prt-get -o um -d 'Update md5sum'
complete -c prt-get -o im -d 'Ignore md5sum'
complete -c prt-get -o uf -d 'Update footprint'
complete -c prt-get -o if -d 'Ignore footprint'
complete -c prt-get -o ns -d 'No stripping'
complete -c prt-get -o kw -d 'Keep work directory'
complete -c prt-get -l ignore -a '(__fish_prt_ports)' -r -f -d 'Ignore the listed ports'
complete -c prt-get -l cache -d 'Use cache'
complete -c prt-get -l test -d 'Dry run'
complete -c prt-get -l pre-install -d 'Exec eventual pre-install script'
complete -c prt-get -l post-install -d 'Exec eventual post-install script'
complete -c prt-get -l install-scripts -d 'Exec eventual pre-post-install scripts'
complete -c prt-get -l no-std-config -r -d 'Ignore default config file'
complete -c prt-get -l config-prepend -r -d 'Prepend string to config file'
complete -c prt-get -l config-append -r -d 'Append string to config file'
complete -c prt-get -l config-set -r -d 'Overrirde config file with string'
complete -c prt-get -o v -d 'Be verbose'
complete -c prt-get -l margs -r -d 'Arguments for pkgmk'
complete -c prt-get -l aargs -r -d 'Arguments for pkgadd'
complete -c prt-get -l rargs -r -d 'Arguments for pkgrm'
complete -c prt-get -l prefer-higher -o ph -d 'Prefer higher version'
complete -c prt-get -l strict-diff -o sd -d 'Override prefer-higher'
complete -c prt-get -l config -r -d 'Use alternative config file'
complete -c prt-get -l install-root -r -d 'Use this alternative root for installation'
complete -c prt-get -l log -d 'Write output to log file'

complete -f -c prt-cache -n __fish_prt_use_package -a '(__fish_prt_packages)' -d Port
complete -f -c prt-cache -n __fish_prt_use_port -a '(__fish_prt_ports)' -d Package

complete -f -n __fish_prt_no_subcommand -c prt-cache -a install -d 'Install listed ports'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a depinst -d 'Install listed ports and their deps'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a grpinst -d 'Install listed ports, stop if one fails'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a update -d 'Update listed packages'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a remove -d 'Remove listed packages'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a sysup -d 'Update all outdated installed packages'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a lock -d 'Do not update this in sysup'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a unlock -d 'Remove this from lock'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a diff -d 'Print differences between installed packages and ports in the port tree'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a search -d 'Search for an expr in port names'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a dsearch -d 'Search for an expr in port names and descriptions'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a info -d 'Print info on a port'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a fsearch -d 'Search for a pattern in the footprints in the ports tree'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a path -d 'Print the path of a port'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a readme -d 'Print the eventual README of a port'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a depends -d 'Print a list of deps for the listed ports'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a quickdeo -d 'Print a simple list of deps for the listed ports'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a deptree -d 'Print a deptree for the port'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a list -d 'List all the ports'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a printf -d 'Print formatted list of ports'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a listinst -d 'List installed packages'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a listorphans -d 'List installed packages which have no dependent packages'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a current -d 'Print the version of an installed package'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a ls -d 'Print the listing of the directory of a port'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a cat -d 'Print a file in a port to stdout'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a edit -d 'Edit a file in a port'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a help -d 'Shows a help screen'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a dumpconfig -d 'Print the configuration of prt-get'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a version -d 'Show the current version of prt-get'
complete -f -n __fish_prt_no_subcommand -c prt-cache -a cache -d 'Create a cache for prt-get'
complete -c prt-cache -o f -o i -d 'Force install'
complete -c prt-cache -o fr -d 'Force rebuild'
complete -c prt-cache -o um -d 'Update md5sum'
complete -c prt-cache -o im -d 'Ignore md5sum'
complete -c prt-cache -o uf -d 'Update footprint'
complete -c prt-cache -o if -d 'Ignore footprint'
complete -c prt-cache -o ns -d 'No stripping'
complete -c prt-cache -o kw -d 'Keep work directory'
complete -c prt-cache -l ignore -a '(__fish_prt_ports)' -r -f -d 'Ignore the listed ports'
complete -c prt-cache -l test -d 'Dry run'
complete -c prt-cache -l pre-install -d 'Exec eventual pre-install script'
complete -c prt-cache -l post-install -d 'Exec eventual post-install script'
complete -c prt-cache -l install-scripts -d 'Exec eventual pre-post-install scripts'
complete -c prt-cache -l no-std-config -r -d 'Ignore default config file'
complete -c prt-cache -l config-prepend -r -d 'Prepend string to config file'
complete -c prt-cache -l config-append -r -d 'Append string to config file'
complete -c prt-cache -l config-set -r -d 'Overrirde config file with string'
complete -c prt-cache -o v -d 'Be verbose'
complete -c prt-cache -l margs -r -d 'Arguments for pkgmk'
complete -c prt-cache -l aargs -r -d 'Arguments for pkgadd'
complete -c prt-cache -l rargs -r -d 'Arguments for pkgrm'
complete -c prt-cache -l prefer-higher -o ph -d 'Prefer higher version'
complete -c prt-cache -l strict-diff -o sd -d 'Override prefer-higher'
complete -c prt-cache -l config -r -d 'Use alternative config file'
complete -c prt-cache -l install-root -r -d 'Use this alternative root for installation'
complete -c prt-cache -l log -d 'Write output to log file'
