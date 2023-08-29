set -l progname yaourt
complete -c $progname -f

set -l listinstalled "(__fish_print_pacman_packages --installed)"
# This might be an issue if another package manager is also installed (e.g. for containers)
set -l listall "(__fish_print_pacman_packages)"
set -l listrepos "(__fish_print_pacman_repos)"
set -l listgroups "(pacman -Sg)\t'Package Group'"

set -l hasopt '__fish_contains_opt -s B backup -s C -s G getpkgbuild -s P pkgbuild -s D database -s Q query -s R remove -s S sync -s U upgrade -s F files stats'
set -l noopt "not $hasopt"

set -l backup '__fish_contains_opt -s B backup'
set -l clean '__fish_contains_opt -s C'
set -l getpkgbuild '__fish_contains_opt -s G getpkgbuild'
set -l pkgbuild '__fish_contains_opt -s P pkgbuild'
set -l database '__fish_contains_opt -s D database'
set -l query '__fish_contains_opt -s Q query'
set -l remove '__fish_contains_opt -s R remove'
set -l sync '__fish_contains_opt -s S sync'
set -l upgrade '__fish_contains_opt -s U upgrade'
set -l files '__fish_contains_opt -s F files'

# HACK: We only need these three to coerce fish to stop file completion and complete options
complete -c $progname -n $noopt -a -D -d "Modify the package database"
complete -c $progname -n $noopt -a -Q -d "Query the package database"
complete -c $progname -n $noopt -a -C -d "Manage .pac* files"
# Primary operations
complete -c $progname -s D -f -l database -n $noopt -d 'Modify the package database'
complete -c $progname -s Q -f -l query -n $noopt -d 'Query the package database'
complete -c $progname -s R -f -l remove -n $noopt -d 'Remove packages from the system'
complete -c $progname -s S -f -l sync -n $noopt -d 'Synchronize packages'
complete -c $progname -s T -f -l deptest -n $noopt -d 'Check dependencies'
complete -c $progname -s U -f -l upgrade -n $noopt -d 'Upgrade or add a local package'
complete -c $progname -s F -f -l files -n $noopt -d 'Query the files database'
complete -c $progname -s V -f -l version -d 'Display version and exit'
complete -c $progname -s h -f -l help -d 'Display help'
# $progname operations
complete -c $progname -n $noopt -s B -l backup -d "Backup or restore alpm local database"
complete -c $progname -n $noopt -s C -f -d "Manage .pac* files"
complete -c $progname -n $noopt -s G -f -l getpkgbuild -d "Get PKGBUILD from ABS or AUR"
complete -c $progname -n $noopt -s P -l pkgbuild -d "Build package from PKGBUILD found in a local directory"
complete -c $progname -n $noopt -f -l stats -d "Show some statistics about your packages"

# General options
# Only offer these once a command has been given so they get prominent display
complete -c $progname -n $noopt -s b -l dbpath -d 'Alternative database location' -xa '(__fish_complete_directories)'
complete -c $progname -n $hasopt -s r -l root -d 'Alternative installation root'
complete -c $progname -n $hasopt -s v -l verbose -d 'Output more status messages'
complete -c $progname -n $hasopt -l arch -d 'Alternate architecture' -f
complete -c $progname -n $hasopt -l cachedir -d 'Alternative package cache location'
complete -c $progname -n $hasopt -l config -d 'Alternate config file'
complete -c $progname -n $hasopt -l debug -d 'Display debug messages' -f
complete -c $progname -n $hasopt -l gpgdir -d 'GPG directory to verify signatures'
complete -c $progname -n $hasopt -l hookdir -d 'Hook file directory'
complete -c $progname -n $hasopt -l logfile -d 'Specify alternative log file'
complete -c $progname -n $hasopt -l noconfirm -d 'Bypass any question' -f
# General options (yaourt only)
complete -c $progname -n $hasopt -l color -d 'Force color'
complete -c $progname -n $hasopt -l force -d 'Force installation or updates'
complete -c $progname -n $hasopt -l insecure -d 'Allow to perform "insecure" SSL connections'
complete -c $progname -n $hasopt -l nocolor -d 'Disable color'
complete -c $progname -n $hasopt -l confirm -d 'Always ask for confirmation'
complete -c $progname -n $hasopt -l pager -d 'Use $PAGER to show search results'
complete -c $progname -n $hasopt -l export -x -a '(__fish_complete_directories)' -d 'Export built packages and their sources to DIR'
complete -c $progname -n $hasopt -l tmp -x -a '(__fish_complete_directories)' -d 'Use DIR as temporary folder'

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
    # Yaourt only
    complete -c $progname -n $$condition -l conflicts -d 'Show packages that conflict with one of the targets'
    complete -c $progname -n $$condition -l depends -d 'Show packages that depend on one of the targets'
    complete -c $progname -n $$condition -l provides -d 'Show packages that provide one of the targets'
    complete -c $progname -n $$condition -l replaces -d 'Show packages that replace one of the targets'
    complete -c $progname -n $$condition -l nameonly -d 'Query the package names only'
end

# Query options
complete -c $progname -n $query -s c -l changelog -d 'View the change log of PACKAGE' -f
complete -c $progname -n $query -s d -l deps -d 'List only non-explicit packages (dependencies)' -f
complete -c $progname -n $query -s e -l explicit -d 'List only explicitly installed packages' -f
complete -c $progname -n $query -s k -l check -d 'Check if all files owned by PACKAGE are present' -f
complete -c $progname -n $query -s l -l list -d 'List all files owned by PACKAGE' -f
complete -c $progname -n $query -s m -l foreign -d 'List all packages not in the database' -f
complete -c $progname -n $query -s o -l owns -rF -d 'Search for the package that owns FILE'
complete -c $progname -n $query -s p -l file -d 'Apply the query to a package file, not package'
complete -c $progname -n $query -s t -l unrequired -d 'List only unrequired packages' -f
complete -c $progname -n $query -s u -l upgrades -d 'List only out-of-date packages' -f
complete -c $progname -n "$query" -d 'Installed package' -xa $listinstalled -f
# Yaourt only query options
# Backup file is always saved as pacman-$date.tar.bz2
complete -c $progname -n $query -r -l backupfile -k -a '(__fish_complete_suffix tar.bz2)' -d 'Query FILE instead of alpm/aur'
complete -c $progname -n $query -l date -d 'List queries result sorted by installation date'

# Remove options
complete -c $progname -n $remove -s c -l cascade -d 'Also remove packages depending on PACKAGE' -f
complete -c $progname -n $remove -s n -l nosave -d 'Ignore file backup designations' -f
complete -c $progname -n $remove -s s -l recursive -d 'Also remove dependencies of PACKAGE' -f
complete -c $progname -n $remove -s u -l unneeded -d 'Only remove targets not required by PACKAGE' -f
complete -c $progname -n "$remove" -d 'Installed package' -xa $listinstalled -f

# Sync options
complete -c $progname -n $sync -s c -l clean -d 'Remove [all] packages from cache'
complete -c $progname -n $sync -s l -l list -xa "$listrepos" -d 'List all packages in REPOSITORY'
complete -c $progname -n "$sync; and not __fish_contains_opt -s u sysupgrade" -s u -l sysupgrade -d 'Upgrade all packages that are out of date'
complete -c $progname -n "$sync; and __fish_contains_opt -s u sysupgrade" -s u -l sysupgrade -d 'Also downgrade packages'
complete -c $progname -n $sync -s w -l downloadonly -d 'Only download the target packages'
complete -c $progname -n $sync -s y -l refresh -d 'Download fresh copy of the package list'
complete -c $progname -n "$sync" -xa "$listall $listgroups"
# Additional sync options
complete -c $progname -n $sync -s a -l aur -d 'Also search in AUR database'
complete -c $progname -n $sync -s A -l ignorearch -d 'Pass -A or --ignorearch option to makepkg'
complete -c $progname -n $sync -l aur-url -x -d 'Specify a custom AUR url'
set -l has_build_opt '__fish_contains_opt -s b build'
complete -c $progname -n "$sync; and not $has_build_opt" -s b -l build -d 'Build from sources(AUR or ABS)'
complete -c $progname -n "$sync; and     $has_build_opt" -s b -l build -d 'Also build all dependencies'
complete -c $progname -n $sync -l devel -d 'Search an update for devel packages'
complete -c $progname -n $sync -l maintainer -d 'Search packages by maintainer instead of name (AUR only)'
complete -c $progname -n $sync -l m-arg -d 'Pass additional options to makepkg'

# Database options
set -l has_db_opt '__fish_contains_opt asdeps asexplicit'
complete -c $progname -n "$database; and not $has_db_opt" -xa --asdeps -d 'Mark PACKAGE as dependency'
complete -c $progname -n "$database; and not $has_db_opt" -xa --asexplicit -d 'Mark PACKAGE as explicitly installed'
complete -c $progname -n "$database; and not $has_db_opt" -s k -l check -d 'Check database validity'
complete -c $progname -n "$has_db_opt; and $database" -xa "$listinstalled"

# File options - since pacman 5
set -l has_file_opt '__fish_contains_opt list search -s l -s s'
complete -c $progname -n "$files; and not $has_file_opt" -xa --list -d 'List files owned by given packages'
complete -c $progname -n "$files; and not $has_file_opt" -xa -l -d 'List files owned by given packages'
complete -c $progname -n "$files; and not $has_file_opt" -xa --search -d 'Search packages for matching files'
complete -c $progname -n "$files; and not $has_file_opt" -xa -s -d 'Search packages for matching files'
complete -c $progname -n "$files" -s y -l refresh -d 'Refresh the files database' -f
complete -c $progname -n "$files" -s l -l list -d 'List files owned by given packages' -xa $listall
complete -c $progname -n "$files" -s s -l search -d 'Search packages for matching files'
complete -c $progname -n "$files" -s o -l owns -d 'Search for packages that include the given files'
complete -c $progname -n "$files" -s q -l quiet -d 'Show less information' -f
complete -c $progname -n "$files" -l machinereadable -d 'Show in machine readable format' -f

# Upgrade options
# Theoretically, pacman reads packages in all formats that libarchive supports
# In practice, it's going to be tar.xz, tar.gz or tar.zst
complete -c $progname -n "$upgrade" -k -xa '(__fish_complete_suffix pkg.tar.zst pkg.tar.xz pkg.tar.gz)' -d 'Package file'

## Yaourt only stuff
# Clean options
set -l has_clean_opt '__fish_contains_opt clean'
complete -c $progname -n "$clean; and not $has_clean_opt" -xa --clean -d 'Clean all these files'

# pkgbuild options
complete -c $progname -n $pkgbuild -s i -l install -d 'Also install the package'

# getpkgbuild options
complete -c $progname -n $getpkgbuild -xa "$listall"
