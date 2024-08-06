# This can't currently be wrapped as the pacman completions rely on variables whose value this needs to change
# complete -c aura -w pacman
set -l listinstalled "(pacman -Q | tr ' ' \t)"
set -l listall "(__fish_print_pacman_packages)"
set -l listrepos "(__fish_print_pacman_repos)"
set -l listgroups "(pacman -Sg | sed 's/\(.*\)/\1\tPackage group/g')"

set -l noopt 'not __fish_contains_opt -s S -s D -s Q -s R -s U -s T -s A -s B -s C -s L -s O database query sync remove upgrade deptest aursync save downgrade viewlog orphans check conf deps free stats thanks'
set -l database '__fish_contains_opt -s D database'
set -l query '__fish_contains_opt -s Q query'
set -l remove '__fish_contains_opt -s R remove'
set -l sync '__fish_contains_opt -s S sync'
set -l upgrade '__fish_contains_opt -s U upgrade'
set -l aur '__fish_contains_opt -s A aursync'
set -l save '__fish_contains_opt -s B save'
set -l downgrade '__fish_contains_opt -s C downgrade'
set -l orphans '__fish_contains_opt -s O orphans'
set -l logfile '__fish_contains_opt -s L viewlog'
set -l check '__fish_contains_opt check'
set -l conf '__fish_contains_opt conf'
set -l deps '__fish_contains_opt deps'
set -l free '__fish_contains_opt stats'
set -l thanks '__fish_contains_opt thanks'
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
complete -c aura -s O -f -l orphans -n $noopt -d 'Operate on orphan packages'
complete -c aura -s Q -f -l query -n $noopt -d 'Query the package database'
complete -c aura -s R -f -l remove -n $noopt -d 'Remove packages from the system'
complete -c aura -s S -f -l sync -n $noopt -d 'Synchronize packages'
complete -c aura -s T -f -l deptest -n $noopt -d 'Check dependencies'
complete -c aura -s U -f -l upgrade -n $noopt -d 'Upgrade or add a local package'
complete -c aura -l check -n $noopt -d 'Validate your system'
complete -c aura -l conf -n $noopt -d 'View various configuration settings and files'
complete -c aura -l deps -n $noopt -d 'Output a dependency graph'
complete -c aura -l free -n $noopt -d 'State of Free Software install on the system'
complete -c aura -l stats -n $noopt -d 'View statistics about your machine or about Aura itself'
complete -c aura -l thanks -n $noopt -d 'The people behind Aura'
complete -c aura -s V -f -l version -d 'Display version and exit'
complete -c aura -s h -f -l help -d 'Display help'

# General options
complete -c aura -l log-level -d 'Minimum level of Aura log messages to display'
complete -c aura -l noconfirm -d 'Do not ask for any confirmation'

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

for condition in aur
    complete -c aura -n $$condition -s a -l delmakedeps -d 'Remove packages only needed during installation'
    complete -c aura -n $$condition -s c -l clean -d 'Clear the build directory after a package is built'
    complete -c aura -n $$condition -s d -l dryrun -d 'Show available upgrades, but do not perform them'
    complete -c aura -n $$condition -s k -l diff -d 'Show PKGBUILD diffs'
    complete -c aura -n $$condition -l git -d 'Include -git/-svn/etc packages'
end

# AUR options
complete -c aura -n $aur -s i -l info -d 'View package information'
complete -c aura -n $aur -s o -l open -d 'Open a given package\'s AUR package'
complete -c aura -n $aur -s p -l pkgbuild -d 'View the packages\'s PKGBUILD'
complete -c aura -n $aur -s s -l search -r -d 'Search the AUR for matching terms'
complete -c aura -n $aur -s u -l sysupgrade -d 'Upgrade all installed AUR packages'
complete -c aura -n $aur -s v -l provides -d 'Search for packages that "provide" some package identity'
complete -c aura -n $aur -s w -l clone -d 'Download the source tarball'
complete -c aura -n $aur -s y -l refresh -d 'Pull the latest changes for every local package clone'
complete -c aura -n $aur -l build -r -d 'Specify a build location'
complete -c aura -n $aur -l builduser -r -d 'User to build as'
complete -c aura -n $aur -l hotedit -d 'Prompt for PKGBUILD editing'
complete -c aura -n $aur -l ignore -r -d 'Ignore a package upgrade'
complete -c aura -n $aur -l nocheck -d 'Do not run the check() functin of the PKGBUILD'
complete -c aura -n $aur -l shellcheck -d 'Run shellcheck on PKGBUILDs before building'
complete -c aura -n $aur -l skipdepcheck -d 'Perform no dependency resolution'
complete -c aura -n $aur -l skipinteg -d 'Do not perform any verifcation checks on source files'
complete -c aura -n $aur -l skippgpcheck -d 'Do not verify source files with PGP signatures'
complete -c aura -n "$aur; and $search" -l abc -d 'Sort alphabetically'
complete -c aura -n "$aur; and $search" -l limit -d 'Limit the results to N results'
complete -c aura -n "$aur; and $search" -s r -l reverse -d 'Reverse the search results'
complete -c aura -n "$aur; and $search" -s q -l quiet -d 'Only print matching package names'

# Backup options
complete -c aura -n $save -s c -l clean -d 'Remove all but the given number of backups'
complete -c aura -n $save -s l -l list -d 'Show all saved package snapshot filenames'
complete -c aura -n $save -s r -l restore -d 'Restores a record kept with -B'

# Downgrade options
complete -c aura -n $downgrade -s b -l backup -d 'Backup to directory'
complete -c aura -n $downgrade -s c -l clean -d 'Save this many versions'
complete -c aura -n $downgrade -s i -l info -d 'Look up specific packages for info on their cache entries'
complete -c aura -n $downgrade -s l -l list -d 'Print the contents of the package cache'
complete -c aura -n $downgrade -s m -l missing -d 'Display packages that don\'t have a tarball in the cache'
complete -c aura -n $downgrade -s n -l notsaved -d 'Delete only those tarballs which aren\'t present in a snapshot'
complete -c aura -n $downgrade -s s -l search -r -d 'Search via regex'
complete -c aura -n $downgrade -s t -l invalid -r -d 'Delete invalid tarballs from the cache'
complete -c aura -n $downgrade -s y -l refresh -r -d 'Download missing tarballs of installed packages'

# Logfile options
complete -c aura -n $logfile -s i -l info -d 'Show package history'
complete -c aura -n $logfile -s s -l search -r -d 'Search via regex'

# Orphan options
complete -c aura -n $orphans -s a -l adopt -d 'Mark a package as being explicitly installed'
complete -c aura -n $orphans -s e -l elderly -d 'Display all explicitly installed, top-level packages'
complete -c aura -n $orphans -s j -l abandon -d 'Uninstall orphan packages'

# Query options
complete -c aura -n $query -s c -l changelog -d 'View the change log of PACKAGE'
complete -c aura -n $query -s d -l deps -d 'List only non-explicit packages (dependencies)'
complete -c aura -n $query -s e -l explicit -d 'List only explicitly installed packages'
complete -c aura -n $query -s k -l check -d 'Check if all files owned by PACKAGE are present'
complete -c aura -n $query -s l -l list -d 'List all files owned by PACKAGE'
complete -c aura -n $query -s m -l foreign -d 'List all packages not in the database'
complete -c aura -n $query -s o -l owns -r -d 'Search for the package that owns FILE'
complete -c aura -n $query -s p -l file -d 'Apply the query to a package file, not package'
complete -c aura -n $query -s t -l unrequired -d 'List only unrequired packages'
complete -c aura -n $query -s u -l upgrades -d 'List only out-of-date packages'
complete -c aura -n "$query; and $argument" -xa $listinstalled -d 'Installed package'

# Remove options
complete -c aura -n $remove -s c -l cascade -d 'Also remove packages depending on PACKAGE'
complete -c aura -n $remove -s n -l nosave -d 'Ignore file backup designations'
complete -c aura -n $remove -s s -l recursive -d 'Also remove dependencies of PACKAGE'
complete -c aura -n $remove -s u -l unneeded -d 'Only remove targets not required by PACKAGE'
complete -c aura -n "$remove; and $argument" -xa $listinstalled -d 'Installed package'

# Sync options
complete -c aura -n $sync -s c -l clean -d 'Remove [all] packages from cache'
complete -c aura -n $sync -s l -l list -xa "$listrepos" -d 'List all packages in REPOSITORY'
complete -c aura -n $sync -s u -l sysupgrade -d 'Upgrade all packages that are out of date'
complete -c aura -n $sync -s w -l downloadonly -d 'Only download the target packages'
complete -c aura -n $sync -s y -l refresh -d 'Download fresh copy of the package list'
complete -c aura -n "$sync; and $argument" -xa "$listall $listgroups"

# Upgrade options
complete -c aura -n "$upgrade; and $argument" -k -a '(__fish_complete_suffix pkg.tar.xz pkg.tar.gz pkg.tar.zst)' -d 'Package file'

# Conf options
complete -c aura -n $conf -s a -l aura -d 'View the contents of your Aura config'
complete -c aura -n $conf -s g -l gen -d 'Output your full current config as legal TOML'
complete -c aura -n $conf -s m -l makepkg -d 'View the Makepkg conf'
complete -c aura -n $conf -s p -l pacman -d 'View the Pacman conf'

# Deps options
complete -c aura -n $deps -s l -l limit -d 'The number of layers up or down to allow'
complete -c aura -n $deps -s o -l optional -d 'Include optional dependencies'
complete -c aura -n $deps -s r -l reverse -d 'Display packages that depend on the given args'
complete -c aura -n $deps -l open -d 'Open the output image automatically'
complete -c aura -n $deps -l raw -d 'Print the raw DOT output'

# Free options
complete -c aura -n $free -l copyleft -d 'Consider only Copyleft licenses'
complete -c aura -n $free -l lenient -d 'Allow FOSS-derived custom licenses'
