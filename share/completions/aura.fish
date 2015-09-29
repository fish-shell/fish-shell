# This can't currently be wrapped as the pacman completions rely on variables whose value this needs to change
# complete -c aura -w pacman
set -l listinstalled "(pacman -Q | tr ' ' \t)"
set -l listall "(__fish_print_packages)"
set -l listrepos "(__fish_print_pacman_repos)"
set -l listgroups "(pacman -Sg | sed 's/\(.*\)/\1\tPackage group/g')"

set -l noopt 'not __fish_contains_opt -s S -s D -s Q -s R -s U -s T -s A -s B -s C -s L -s M -s O database query sync remove upgrade deptest aursync save downgrade viewlog abssync orphans'
set -l database '__fish_contains_opt -s D database'
set -l query '__fish_contains_opt -s Q query'
set -l remove '__fish_contains_opt -s R remove'
set -l sync '__fish_contains_opt -s S sync'
set -l upgrade '__fish_contains_opt -s U upgrade'
set -l aur '__fish_contains_opt -s A aursync'
set -l abs '__fish_contains_opt -s M abssync'
set -l save '__fish_contains_opt -s B save'
set -l downgrade '__fish_contains_opts -s C downgrade'
set -l orphans '__fish_contains_opt -s O orphans'
set -l logfile '__fish_contains_opt -s L viewlog'
set -l search '__fish_contains_opt -s s search'

# By default fish expands the arguments with the option which is not desired
# due to performance reasons.
# This will make sure we are expanding an argument and not an option:
set -l argument 'not expr -- (commandline --current-token) : "^-.*" > /dev/null'

# Primary operations
complete -c aura -s A -f -l aursync -n $noopt -d 'Synchronize AUR packages'
complete -c aura -s B -f -l save -n $noopt -d 'Save and restore package state'
complete -c aura -s C -f -l downgrade -n $noopt -d 'Package cache actions'
complete -c aura -s D -f -l database -n $noopt -d 'Modify the package database'
complete -c aura -s L -f -l viewlog -n $noopt -d 'Pacman log actions'
complete -c aura -s M -f -l abssync -n $noopt -d 'Build packages from ABS'
complete -c aura -s O -f -l orphans -n $noopt -d 'Operate on orphan packages'
complete -c aura -s Q -f -l query -n $noopt -d 'Query the package database'
complete -c aura -s R -f -l remove -n $noopt -d 'Remove packages from the system'
complete -c aura -s S -f -l sync -n $noopt -d 'Synchronize packages'
complete -c aura -s T -f -l deptest -n $noopt -d 'Check dependencies'
complete -c aura -s U -f -l upgrade -n $noopt -d 'Upgrade or add a local package'
complete -c aura -l auradebug -d 'Show settings while running'
complete -c aura -l no-pp -d 'Do not use powerpill'
complete -c aura -l languages -d 'Show available languages'
complete -c aura -l viewconf -d 'View pacman.conf'
complete -c aura -s V -f -l version -d 'Display version and exit'
complete -c aura -s h -f -l help -d 'Display help'

# General options
complete -c aura -s b -l dbpath -d 'Alternative database location' -xa '(__fish_complete_directories)'
complete -c aura -s r -l root -d 'Alternative installation root'
complete -c aura -s v -l verbose -d 'Output more status messages'
complete -c aura -l arch -d 'Alternate architecture'
complete -c aura -l cachedir -d 'Alternative package cache location'
complete -c aura -l config -d 'Alternate config file'
complete -c aura -l debug -d 'Display debug messages'
complete -c aura -l gpgdir -d 'GPG directory to verify signatures'
complete -c aura -l logfile -d 'Specify alternative log file'
complete -c aura -l noconfirm -d 'Bypass any question'

# Transaction options (sync, remove, upgrade)
for condition in sync remove upgrade
    complete -c aura -n $$condition -s d -l nodeps -d 'Skip [all] dependency checks'
    complete -c aura -n $$condition -l dbonly -d 'Modify database entry only'
    complete -c aura -n $$condition -l noprogressbar -d 'Do not display progress bar'
    complete -c aura -n $$condition -l noscriptlet -d 'Do not execute install script'
    complete -c aura -n $$condition -s p -l print -d 'Dry run, only print targets'
    complete -c aura -n $$condition -l print-format -x -d 'Specify printf-like format'
end

# Database and upgrade options (database, sync, upgrade)
for condition in database sync upgrade
    complete -c aura -n $$condition -l asdeps -d 'Mark PACKAGE as dependency'
    complete -c aura -n $$condition -l asexplicit -d 'Mark PACKAGE as explicitly installed'
end

# Upgrade options (sync, upgrade)
for condition in sync upgrade
    complete -c aura -n $$condition -s f -l force -d 'Bypass file conflict checks'
    complete -c aura -n $$condition -l ignore -d 'Ignore upgrade of PACKAGE' -xa "$listinstalled"
    complete -c aura -n $$condition -l ignoregroup -d 'Ignore upgrade of GROUP' -xa "$listgroups"
    complete -c aura -n $$condition -l needed -d 'Do not reinstall up-to-date targets'
    complete -c aura -n $$condition -l recursive -d 'Recursively reinstall all dependencies'
end

# Query and sync options
for condition in query sync
    complete -c aura -n $$condition -s g -l groups -d 'Display all packages in GROUP' -xa "$listgroups"
    complete -c aura -n $$condition -s i -l info -d 'Display information on PACKAGE'
    complete -c aura -n $$condition -s q -l quiet -d 'Show less information'
    complete -c aura -n $$condition -s s -l search -r -d 'Search packages for regexp'
end

for condition in abs aur
    complete -c aura -n $$condition -s a -l delmakedeps -d 'Remove packages only needed during installation'
    complete -c aura -n $$condition -s d -l deps -d 'View package dependencies'
    complete -c aura -n $$condition -s i -l info -d 'View package information'
    complete -c aura -n $$condition -s k -l diff -d 'Show PKGBUILD diffs'
    complete -c aura -n $$condition -s p -l pkgbuild -d 'View the packages\'s PKGBUILD'
    complete -c aura -n $$condition -s x -l unsuppress -d 'Show makepkg output'
    complete -c aura -n $$condition -l absdeps -d 'Build dependencies from ABS'
end

# AUR options
complete -c aura -n $aur -s q -l quiet -d 'Show less information'
complete -c aura -n $aur -s s -l search -r -d 'Search AUR by string matching'
complete -c aura -n $aur -s u -l sysupgrade -d 'Upgrade all installed AUR packages'
complete -c aura -n $aur -s w -l downloadonly -d 'Download the source tarball'
complete -c aura -n $aur -l aurignore -r -d 'Ignore given comma-separated packages'
complete -c aura -n $aur -l build -r -d 'Specify a build location'
complete -c aura -n $aur -l builduser -r -d 'User to build as'
complete -c aura -n $aur -l custom -d 'Run customizepkg before build'
complete -c aura -n $aur -l devel -d 'Include -git/-svn/etc packages'
complete -c aura -n $aur -l hotedit -d 'Prompt for PKGBUILD editing'
complete -c aura -n $aur -l ignorearch -d 'Ignore architecture checking'
complete -c aura -n "$aur; and $search" -l abc -d 'Sort alphabetically'
complete -c aura -n "$aur; and $search" -l head -d 'Only show the first 10 results'
complete -c aura -n "$aur; and $search" -l tail -d 'Only show the last 10 results'

# Backup options
complete -c aura -n $save -s c -l clean -d 'Remove all but the given number of backups'
complete -c aura -n $save -s r -l restore -d 'Restores a record kept with -B'

# Downgrade options
complete -c aura -n $downgrade -s b -l backup -d 'Backup to directory'
complete -c aura -n $downgrade -s c -l clean -d 'Save this many versions'
complete -c aura -n $downgrade -s s -l search -r -d 'Search via regex'

# Logfile options
complete -c aura -n $logfile -s i -l info -d 'Show package history'
complete -c aura -n $logfile -s s -l search -r -d 'Search via regex'

# ABS options
complete -c aura -n $abs -s s -l search -r -d 'Search ABS by regex'
complete -c aura -n $abs -s c -l clean -d 'Delete local ABS tree'
complete -c aura -n $abs -s y -l refresh -d 'Download fresh copy of the package list'
complete -c aura -n $abs -s t -l treesync -d 'Sync the given to local ABS tree'
complete -c aura -n $abs -l absdeps -d 'Download fresh copy of the package list'

# Orphan options
complete -c aura -n $orphans -s j -l abandon -d 'Uninstall orphan packages'

# Query options
complete -c aura -n $query -s c -l changelog -d 'View the change log of PACKAGE'
complete -c aura -n $query -s d -l deps -d 'List only non-explicit packages (dependencies)'
complete -c aura -n $query -s e -l explicit -d 'List only explicitly installed packages'
complete -c aura -n $query -s k -l check -d 'Check if all files owned by PACKAGE are present'
complete -c aura -n $query -s l -l list -d 'List all files owned by PACKAGE'
complete -c aura -n $query -s m -l foreign -d 'List all packages not in the database'
complete -c aura -n $query -s o -l owns -r -d 'Search for the package that owns FILE' -xa ''
complete -c aura -n $query -s p -l file -d 'Apply the query to a package file, not package' -xa ''
complete -c aura -n $query -s t -l unrequired -d 'List only unrequired packages'
complete -c aura -n $query -s u -l upgrades -d 'List only out-of-date packages'
complete -c aura -n "$query; and $argument" -xa $listinstalled -d 'Installed package'

# Remove options
complete -c aura -n $remove -s c -l cascade -d 'Also remove packages depending on PACKAGE'
complete -c aura -n $remove -s n -l nosave -d 'Ignore file backup designations'
complete -c aura -n $remove -s s -l recursive -d 'Also remove dependencies of PACKAGE'
complete -c aura -n $remove -s u -l unneeded -d 'Only remove targets not required by PACKAGE'


# Sync options
complete -c aura -n $sync -s c -l clean -d 'Remove [all] packages from cache'
complete -c aura -n $sync -s l -l list -xa "$listrepos" -d 'List all packages in REPOSITORY'
complete -c aura -n $sync -s u -l sysupgrade -d 'Upgrade all packages that are out of date'
complete -c aura -n $sync -s w -l downloadonly -d 'Only download the target packages'
complete -c aura -n $sync -s y -l refresh -d 'Download fresh copy of the package list'
complete -c aura -n "$argument; and $sync" -xa "$listall $listgroups"

# Upgrade options
complete -c aura -n "$upgrade; and $argument" -xa '(__fish_complete_suffix pkg.tar.xz)' -d 'Package file'
complete -c aura -n "$upgrade; and $argument" -xa '(__fish_complete_suffix pkg.tar.gz)' -d 'Package file'
