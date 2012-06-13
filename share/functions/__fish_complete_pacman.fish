function __fish_complete_pacman -d 'Complete pacman (ARCH package manager)' --argument-names progname
    # Completions for pacman, using short options when possible
    # Author: Giorgio Lando <patroclo7@gmail.com>
    # Updated by maxfl
    set -q progname[1]; or set -l progname pacman

    set -l listinstalled "(pacman -Q | tr ' ' \t)" 
    set -l listall       "(pacman -Sl | cut --delim ' ' --fields 2- | tr ' ' \t | sort)" 
    set -l listgroups    "(cat /etc/pacman.conf | grep '^\[.\+\]' | sed 's/[]\[]//g')"

    set -l noopt   'commandline | not sgrep -qe "-[a-z]*[DQRSTU]\|--database\|--query\|--sync\|--remove\|--upgrade\|--deptest"' 
    set -l query   'commandline | sgrep -qe "-[a-z]*Q\|--query"' 
    set -l remove  'commandline | sgrep -qe "-[a-z]*R\|--remove"' 
    set -l sync    'commandline | sgrep -qe "-[a-z]*S\|--sync"' 
    set -l upgrade 'commandline | sgrep -qe "-[a-z]*U\|--upgrade"' 

    # Primary operations
    complete -c $progname -s D -f -l database -n $noopt -d 'Modify the package database'
    complete -c $progname -s Q -f -l query    -n $noopt -d 'Query the package database'
    complete -c $progname -s R -f -l remove   -n $noopt -d 'Remove packages from the system'
    complete -c $progname -s S -f -l sync     -n $noopt -d 'Synchronize packages'
    complete -c $progname -s T -f -l deptest  -n $noopt -d 'Check dependencies'
    complete -c $progname -s U -f -l upgrade  -n $noopt -d 'Upgrade or add a package in the system'
    complete -c $progname -s V -f -l version  -d 'Display version and exit'
    complete -c $progname -s h -f -l help     -d 'Display help'

    # General options
    complete -c $progname -s b -l dbpath -d 'Alternative database location' -xa '(__fish_complete_directories)'
    complete -c $progname -s d         -d 'Skips all dependency checks'
    complete -c $progname -s f         -d 'Bypass file conflict checks'
    complete -c $progname -s r -l root -d 'Specify an alternative installation root'
    complete -c $progname -s v -l verbose -d 'Output more status messages'
    complete -c $progname -l arch      -d 'Specify alternate architecture'
    complete -c $progname -l cachedir  -d 'Specify an alternative package cache location'
    complete -c $progname -l config    -d 'Specify an altenate config file'
    complete -c $progname -l debug     -d 'Display debug messages'
    complete -c $progname -l gpgdir    -d 'Specify a directory of files used by GnuPG to verify package signatures'
    complete -c $progname -l logfile   -d 'Specify alternative log file'
    complete -c $progname -l noconfirm -d 'Bypass any question'
    
    # Transaction options (query, sync, remove, upgrade)
    for condition in query sync remove upgrade
        complete -c $progname -n $$condition -s d -l nodeps     -d 'Skip dependency check'
        complete -c $progname -n $$condition -l dbonly          -d 'Add/remove the database entry only, leave all files'
        complete -c $progname -n $$condition -l noprogressbar   -d 'Do not display progressbar'
        complete -c $progname -n $$condition -l noscriptlet     -d 'Do not execute intall script'
        complete -c $progname -n $$condition -s p -l print      -d 'Only print targets instead of performing the actual operation'
        complete -c $progname -n $$condition -l print-format -x -d 'Specify printf-like format'
    end
    
    # Upgrade options (sync, upgrade)
    for condition in sync upgrade
        complete -c $progname -n $$condition -s f -l force       -d 'Bypass file conflict checks'
        complete -c $progname -n $$condition      -l asdeps      -d 'Install packages non-explicitly, as dependencies'
        complete -c $progname -n $$condition      -l asexplicit  -d 'Install packages explicitly'
        complete -c $progname -n $$condition      -l ignore      -d 'Ignore upgrade of this package' -xa $listinstalled
        complete -c $progname -n $$condition      -l ignoregroup -d 'Ignore upgrade of all packages in a group' -xa $listgroups
        complete -c $progname -n $$condition      -l needed      -d 'Do not reinstall up-to-date targets'
        complete -c $progname -n $$condition      -l recursive   -d 'Recursively reinstall all dependencies'
    end
    
    # Query and sync options
    for condition in query sync
        complete -c $progname -n $$condition -s g -l groups     -d 'Display all packages in the group'
        complete -c $progname -n $$condition -s i -l info       -d 'Display information on a given package'
        complete -c $progname -n $$condition -s q -l quiet      -d 'Show less information'
        complete -c $progname -n $$condition -s s -l search -r  -d 'Search packages for regexp'
    end

    # Query options
    complete -c $progname -n $query -s c -l changelog  -d 'View the changelog of a package'
    complete -c $progname -n $query -s d -l deps       -d 'List only non-explicit packages (dependencies)'
    complete -c $progname -n $query -s e -l explicit   -d 'List only explicitly installed packages'
    complete -c $progname -n $query -s k -l check      -d 'Check that all files owned by the package are present'
    complete -c $progname -n $query -s l -l list       -d 'List all files owned by a given package'
    complete -c $progname -n $query -s m -l foreign    -d 'List all packages which are not in the sync database'
    complete -c $progname -n $query -s o -l owns -r    -d 'Search for the package that owns file'
    complete -c $progname -n $query -s p -l file       -d 'Apply the query to a package file and not to an installed package'
    complete -c $progname -n $query -s t -l unrequired -d 'List packages not required by any of installed packages'
    complete -c $progname -n $query -s u -l upgrades   -d 'List all out of date packages in the system'
    complete -c $progname -n $query -xa $listinstalled -d 'Installed package'
    
    # Remove options
    complete -c $progname -n $remove -s c -l cascade   -d 'Remove also the packages that depends on the target packages'
    complete -c $progname -n $remove -s n -l nosave    -d 'Ignore file backup designations'
    complete -c $progname -n $remove -s s -l recursive -d 'Remove also the dependencies of the target packages'
    complete -c $progname -n $remove -s u -l unneeded  -d 'Remove targets that are not required by any other package'
    complete -c $progname -n $remove -xa $listinstalled -d 'Installed package'

    # Sync options
    complete -c $progname -n $sync -s c -l clean        -d 'Remove old packages from the cache; if iterated, remove all the packages from the cache'
    complete -c $progname -n $sync -s l -l list         -d 'List all packages in the repository'
    complete -c $progname -n $sync -s u -l sysupgrade   -d 'Upgrade all packages that are out of date'
    complete -c $progname -n $sync -s w -l downloadonly -d 'Only download the target packages'
    complete -c $progname -n $sync -s y -l refresh      -d 'Download a fresh copy of the master package list from the servers'
    complete -c $progname -n $sync -xa $listall -d 'Repo package'

    # Upgrade options
    complete -c $progname -n $upgrade -a '(__fish_complete_suffix pkg.tar.xz)' -d 'Local package'
    complete -c $progname -n $upgrade -a '(__fish_complete_suffix pkg.tar.gz)' -d 'Local package'
end
