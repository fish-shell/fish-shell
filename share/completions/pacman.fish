# Completions for pacman
# Author: Giorgio Lando <patroclo7@gmail.com>
# Updated by maxfl, SanskritFritz, faho

set -l progname pacman

set -l listinstalled "(pacman -Q | tr ' ' \t)"
# This might be an issue if another package manager is also installed (e.g. for containers)
set -l listall "(__fish_print_packages)"
set -l listrepos "(__fish_print_pacman_repos)"
set -l listgroups "(pacman -Sg | sed 's/\(.*\)/\1\tPackage group/g')"

set -l noopt 'not __fish_contains_opt -s S -s D -s Q -s R -s U -s T database query sync remove upgrade deptest'
set -l database '__fish_contains_opt -s D database'
set -l query '__fish_contains_opt -s Q query'
set -l remove '__fish_contains_opt -s R remove'
set -l sync '__fish_contains_opt -s S sync'
set -l upgrade '__fish_contains_opt -s U upgrade'

# By default fish expands the arguments with the option which is not desired
# due to performance reasons.
# This will make sure we are expanding an argument and not an option:
set -l argument 'not expr -- (commandline --current-token) : "^-.*" > /dev/null'

complete -c pacman -e
complete -c pacman -f
# HACK: We only need these two to coerce fish to stop file completion and complete options
complete -c $progname -n $noopt -a "-D" -d "Modify the package database"
complete -c $progname -n $noopt -a "-Q" -d "Query the package database"
# Primary operations
complete -c $progname -s D -f -l database -n $noopt -d 'Modify the package database'
complete -c $progname -s Q -f -l query -n $noopt -d 'Query the package database'
complete -c $progname -s R -f -l remove -n $noopt -d 'Remove packages from the system'
complete -c $progname -s S -f -l sync -n $noopt -d 'Synchronize packages'
complete -c $progname -s T -f -l deptest -n $noopt -d 'Check dependencies'
complete -c $progname -s U -f -l upgrade -n $noopt -d 'Upgrade or add a local package'
complete -c $progname -s V -f -l version -d 'Display version and exit'
complete -c $progname -s h -f -l help -d 'Display help'

# General options
# Only offer these once a command has been given so they get prominent display
complete -c $progname -n "not $noopt" -s b -l dbpath -d 'Alternative database location' -xa '(__fish_complete_directories)'
complete -c $progname -n "not $noopt" -s r -l root -d 'Alternative installation root'
complete -c $progname -n "not $noopt" -s v -l verbose -d 'Output more status messages'
complete -c $progname -n "not $noopt" -l arch -d 'Alternate architecture' -f
complete -c $progname -n "not $noopt" -l cachedir -d 'Alternative package cache location'
complete -c $progname -n "not $noopt" -l config -d 'Alternate config file'
complete -c $progname -n "not $noopt" -l debug -d 'Display debug messages' -f
complete -c $progname -n "not $noopt" -l gpgdir -d 'GPG directory to verify signatures'
complete -c $progname -n "not $noopt" -l logfile -d 'Specify alternative log file'
complete -c $progname -n "not $noopt" -l noconfirm -d 'Bypass any question' -f

# Transaction options (sync, remove, upgrade)
for condition in sync remove upgrade
    complete -c $progname -n $$condition -s d -l nodeps -d 'Skip [all] dependency checks' -f
    complete -c $progname -n $$condition -l dbonly -d 'Modify database entry only' -f
    complete -c $progname -n $$condition -l noprogressbar -d 'Do not display progress bar' -f
    complete -c $progname -n $$condition -l noscriptlet -d 'Do not execute install script' -f
    complete -c $progname -n $$condition -s p -l print -d 'Dry run, only print targets' -f
    complete -c $progname -n $$condition -l print-format -x -d 'Specify printf-like format' -f
end

# Database and upgrade options (database, sync, upgrade)
for condition in database sync upgrade
    complete -c $progname -n $$condition -l asdeps -d 'Mark PACKAGE as dependency' -f
    complete -c $progname -n $$condition -l asexplicit -d 'Mark PACKAGE as explicitly installed' -f
end

# Upgrade options (sync, upgrade)
for condition in sync upgrade
    complete -c $progname -n $$condition -l force -d 'Bypass file conflict checks' -f
    complete -c $progname -n $$condition -l ignore -d 'Ignore upgrade of PACKAGE' -xa "$listinstalled" -f
    complete -c $progname -n $$condition -l ignoregroup -d 'Ignore upgrade of GROUP' -xa "$listgroups" -f
    complete -c $progname -n $$condition -l needed -d 'Do not reinstall up-to-date targets' -f
    complete -c $progname -n $$condition -l recursive -d 'Recursively reinstall all dependencies' -f
end

# Query and sync options
for condition in query sync
    complete -c $progname -n $$condition -s g -l groups -d 'Display all packages in GROUP' -xa "$listgroups" -f
    complete -c $progname -n $$condition -s i -l info -d 'Display information on PACKAGE' -f
    complete -c $progname -n $$condition -s q -l quiet -d 'Show less information' -f
    complete -c $progname -n $$condition -s s -l search -r -d 'Search packages for regexp' -f
end

# Query options
complete -c $progname -n $query -s c -l changelog -d 'View the change log of PACKAGE' -f
complete -c $progname -n $query -s d -l deps -d 'List only non-explicit packages (dependencies)' -f
complete -c $progname -n $query -s e -l explicit -d 'List only explicitly installed packages' -f
complete -c $progname -n $query -s k -l check -d 'Check if all files owned by PACKAGE are present' -f
complete -c $progname -n $query -s l -l list -d 'List all files owned by PACKAGE' -f
complete -c $progname -n $query -s m -l foreign -d 'List all packages not in the database' -f
complete -c $progname -n $query -s o -l owns -r -d 'Search for the package that owns FILE' -xa '' -f
complete -c $progname -n $query -s p -l file -d 'Apply the query to a package file, not package' -xa '' -f
complete -c $progname -n $query -s t -l unrequired -d 'List only unrequired packages' -f
complete -c $progname -n $query -s u -l upgrades -d 'List only out-of-date packages' -f
complete -c $progname -n "$query; and $argument" -d 'Installed package' -xa $listinstalled -f

# Remove options
complete -c $progname -n $remove -s c -l cascade -d 'Also remove packages depending on PACKAGE' -f
complete -c $progname -n $remove -s n -l nosave -d 'Ignore file backup designations' -f
complete -c $progname -n $remove -s s -l recursive -d 'Also remove dependencies of PACKAGE' -f
complete -c $progname -n $remove -s u -l unneeded -d 'Only remove targets not required by PACKAGE' -f
complete -c $progname -n "$remove; and $argument" -d 'Installed package' -xa $listinstalled -f

# Sync options
complete -c $progname -n $sync -s c -l clean -d 'Remove [all] packages from cache'
complete -c $progname -n $sync -s l -l list -xa "$listrepos" -d 'List all packages in REPOSITORY'
complete -c $progname -n "$sync; and not __fish_contains_opt -s u sysupgrade" -s u -l sysupgrade -d 'Upgrade all packages that are out of date'
complete -c $progname -n "$sync; and __fish_contains_opt -s u sysupgrade" -s u -l sysupgrade -d 'Also downgrade packages'
complete -c $progname -n $sync -s w -l downloadonly -d 'Only download the target packages'
complete -c $progname -n $sync -s y -l refresh -d 'Download fresh copy of the package list'
complete -c $progname -n "$argument; and $sync" -xa "$listall $listgroups"

# Upgrade options
# Theoretically, pacman reads packages in all formats that libarchive supports
# In practice, it's going to be tar.xz or tar.gz
# Using "pkg.tar.*" here would change __fish_complete_suffix's descriptions to "unknown"
complete -c $progname -n "$upgrade; and $argument" -xa '(__fish_complete_suffix pkg.tar.xz)' -d 'Package file'
complete -c $progname -n "$upgrade; and $argument" -xa '(__fish_complete_suffix pkg.tar.gz)' -d 'Package file'

set -e progname
set -e listinstalled
set -e listall
set -e listrepos
set -e listgroups
set -e noopt
set -e database
set -e query
set -e remove
set -e sync
set -e upgrade
set -e argument
