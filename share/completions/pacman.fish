# Completions for pacman
# Author: Giorgio Lando <patroclo7@gmail.com>
# Updated by maxfl, SanskritFritz, faho, f1u77y, akiirui

set -l progname pacman

set -l listinstalled "(__fish_print_pacman_packages --installed)"
# This might be an issue if another package manager is also installed (e.g. for containers)
set -l listall "(__fish_print_pacman_packages)"
set -l listrepos "(__fish_print_pacman_repos)"
set -l listgroups "(pacman -Sg)\t'Package Group'"
# Theoretically, pacman reads packages in all formats that libarchive supports
# In practice, it's going to be tar.xz, tar.gz, tar.zst, or just pkg.tar (uncompressed pkg)
set -l listfiles '(__fish_complete_suffix pkg.tar.zst pkg.tar.xz pkg.tar.gz pkg.tar)'

set -l noopt 'not __fish_contains_opt -s S -s D -s Q -s R -s U -s T -s F database query sync remove upgrade deptest files'
set -l database '__fish_contains_opt -s D database'
set -l query '__fish_contains_opt -s Q query'
set -l queryfile '__fish_contains_opt -s p file'
set -l remove '__fish_contains_opt -s R remove'
set -l sync '__fish_contains_opt -s S sync'
set -l upgrade '__fish_contains_opt -s U upgrade'
set -l files '__fish_contains_opt -s F files'

complete -c $progname -e
complete -c $progname -f
# HACK: We only need these two to coerce fish to stop file completion and complete options
complete -c $progname -n "$noopt" -a -D -d "Modify the package database"
complete -c $progname -n "$noopt" -a -Q -d "Query the package database"

# Primary operations
complete -c $progname -s D -f -l database -n "$noopt" -d 'Modify the package database'
complete -c $progname -s Q -f -l query -n "$noopt" -d 'Query the package database'
complete -c $progname -s R -f -l remove -n "$noopt" -d 'Remove packages from the system'
complete -c $progname -s S -f -l sync -n "$noopt" -d 'Synchronize packages'
complete -c $progname -s T -f -l deptest -n "$noopt" -d 'Check dependencies'
complete -c $progname -s U -l upgrade -n "$noopt" -d 'Upgrade or add a local package'
complete -c $progname -s F -f -l files -n "$noopt" -d 'Query the files database'
complete -c $progname -s V -f -l version -d 'Display version and exit'
complete -c $progname -s h -f -l help -d 'Display help'

# General options
# Only offer these once a command has been given so they get prominent display
complete -c $progname -n "not $noopt" -s b -l dbpath -d 'Alternate database location' -xa "(__fish_complete_directories)"
complete -c $progname -n "not $noopt" -s r -l root -d 'Alternate installation root' -xa "(__fish_complete_directories)"
complete -c $progname -n "not $noopt" -s v -l verbose -d 'Output more status messages' -f
complete -c $progname -n "not $noopt" -l arch -d 'Alternate architecture' -f
complete -c $progname -n "not $noopt" -l cachedir -d 'Alternate package cache location' -xa "(__fish_complete_directories)"
complete -c $progname -n "not $noopt" -l color -d 'Colorize the output' -fa '{auto,always,never}'
complete -c $progname -n "not $noopt" -l config -d 'Alternate config file' -rF
complete -c $progname -n "not $noopt" -l confirm -d 'Always ask for confirmation' -f
complete -c $progname -n "not $noopt" -l debug -d 'Display debug messages' -f
complete -c $progname -n "not $noopt" -l disable-download-timeout -d 'Use relaxed timeouts for download' -f
complete -c $progname -n "not $noopt" -l gpgdir -d 'Alternate home directory for GnuPG' -xa "(__fish_complete_directories)"
complete -c $progname -n "not $noopt" -l hookdir -d 'Alternate hook location' -xa "(__fish_complete_directories)"
complete -c $progname -n "not $noopt" -l logfile -d 'Alternate log file'
complete -c $progname -n "not $noopt" -l noconfirm -d 'Bypass any confirmation' -f
complete -c $progname -n "not $noopt" -l sysroot -d 'Operate on a mounted guest system (root-only)' -xa "(__fish_complete_directories)"

# File, query, sync options (files, query, sync)
for condition in files query sync
    complete -c $progname -n "$$condition" -s q -l quiet -d 'Show less information' -f
end

# Transaction options (sync, remove, upgrade)
for condition in sync remove upgrade
    complete -c $progname -n "$$condition" -s d -l nodeps -d 'Skip [all] dependency checks' -f
    complete -c $progname -n "$$condition" -s p -l print -d 'Dry run, only print targets' -f
    complete -c $progname -n "$$condition" -l assume-installed -d 'Add a virtual package to satisfy dependencies' -f
    complete -c $progname -n "$$condition" -l dbonly -d 'Modify database entry only' -f
    complete -c $progname -n "$$condition" -l noprogressbar -d 'Do not display progress bar' -f
    complete -c $progname -n "$$condition" -l noscriptlet -d 'Do not execute install script' -f
    complete -c $progname -n "$$condition" -l print-format -d 'Specify printf-like format' -x
end

# File and query options (files, query)
for condition in files query
    complete -c $progname -n "$$condition" -s l -l list -d 'List the files owned by PACKAGE' -f
end

# File and sync options (files, sync)
for condition in files sync
    complete -c $progname -n "$$condition" -s y -l refresh -d 'Download fresh package databases [force]' -f
end

# Query and sync options (query, sync)
for condition in query sync
    complete -c $progname -n "$$condition" -s g -l groups -d 'Display members of [all] package GROUP' -xa "$listgroups"
end

# Sync and upgrade options (sync, upgrade)
for condition in sync upgrade
    complete -c $progname -n "$$condition" -l asdeps -d 'Install packages as non-explicitly installed' -f
    complete -c $progname -n "$$condition" -l asexplicit -d 'Install packages as explicitly installed' -f
    complete -c $progname -n "$$condition" -l ignore -d 'Ignore a package upgrade (can be used more than once)' -xa "$listall"
    complete -c $progname -n "$$condition" -l ignoregroup -d 'Ignore a group upgrade (can be used more than once)' -xa "$listgroups"
    complete -c $progname -n "$$condition" -l needed -d 'Do not reinstall up to date packages' -f
    complete -c $progname -n "$$condition" -l overwrite -d 'Overwrite conflicting files (can be used more than once)' -rF
end

# Database options
set -l has_db_opt '__fish_contains_opt asdeps asexplicit check -s k'
complete -c $progname -n "$database; and not $has_db_opt" -s k -l check -d 'Check database validity'
complete -c $progname -n "$database" -s q -l quiet -d 'Suppress output of success messages' -f
complete -c $progname -n "$database; and not $has_db_opt" -l asdeps -d 'Mark PACKAGE as dependency' -x
complete -c $progname -n "$database; and not $has_db_opt" -l asexplicit -d 'Mark PACKAGE as explicitly installed' -x
complete -c $progname -n "$has_db_opt; and $database" -xa "$listinstalled"

# File options - since pacman 5
complete -c $progname -n "$files" -s x -l regex -d 'Interpret each query as a regular expression' -f
complete -c $progname -n "$files" -l machinereadable -d 'Print each match in a machine readable output format' -f
complete -c $progname -n "$files" -d Package -xa "$listall"

# Query options
complete -c $progname -n "$query" -s c -l changelog -d 'View the change log of PACKAGE' -f
complete -c $progname -n "$query" -s d -l deps -d 'List only non-explicit packages (dependencies)' -f
complete -c $progname -n "$query" -s e -l explicit -d 'List only explicitly installed packages' -f
complete -c $progname -n "$query" -s i -l info -d 'View PACKAGE [backup files] information' -f
complete -c $progname -n "$query" -s k -l check -d 'Check that PACKAGE files exist' -f
complete -c $progname -n "$query" -s m -l foreign -d 'List installed packages not found in sync database' -f
complete -c $progname -n "$query" -s n -l native -d 'list installed packages only found in sync database' -f
complete -c $progname -n "$query" -s o -l owns -d 'Query the package that owns FILE' -rF
complete -c $progname -n "$query" -s p -l file -d 'Query a package file instead of the database' -rF
complete -c $progname -n "$query" -s s -l search -d 'Search locally-installed packages for regexp' -f
complete -c $progname -n "$query" -s t -l unrequired -d 'List only unrequired packages [and optdepends]' -f
complete -c $progname -n "$query" -s u -l upgrades -d 'List only out-of-date packages' -f
complete -c $progname -n "$query" -n "not $queryfile" -d 'Installed package' -xa "$listinstalled"
complete -c $progname -n "$query" -n "$queryfile" -d 'Package file' -k -xa "$listfiles"

# Remove options
complete -c $progname -n "$remove" -s c -l cascade -d 'Also remove packages depending on PACKAGE' -f
complete -c $progname -n "$remove" -s n -l nosave -d 'Ignore file backup designations' -f
complete -c $progname -n "$remove" -s s -l recursive -d 'Also remove dependencies of PACKAGE' -f
complete -c $progname -n "$remove" -s u -l unneeded -d 'Only remove targets not required by PACKAGE' -f
complete -c $progname -n "$remove" -d 'Installed package' -xa "$listinstalled"

# Sync options
complete -c $progname -n "$sync" -s c -l clean -d 'Remove [all] packages from cache' -f
complete -c $progname -n "$sync" -s i -l info -d 'View PACKAGE [extended] information' -f
complete -c $progname -n "$sync" -s l -l list -d 'List all packages in REPOSITORY' -xa "$listrepos"
complete -c $progname -n "$sync" -s s -l search -d 'Search remote repositories for regexp' -f
complete -c $progname -n "$sync" -s u -l sysupgrade -d 'Upgrade all packages that are out of date'
complete -c $progname -n "$sync" -s w -l downloadonly -d 'Only download the target packages'
complete -c $progname -n "$sync" -xa "$listall $listgroups"

# Upgrade options
complete -c $progname -n "$upgrade" -k -xa "$listfiles" -d 'Package file'
