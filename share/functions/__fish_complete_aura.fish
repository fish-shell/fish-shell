function __fish_complete_aura -d 'Complete Aura (ARCH/AUR package manager)' --argument-names progname
    # Completions for aura
    # Author: Eric Mrak <mail@ericmrak.info>
    # original for pacman by: Giorgio Lando <patroclo7@gmail.com>

    set -q progname[1]; or set -l progname aura

    set -l listinstalled "(pacman -Q | tr ' ' \t)"
    set -l listall       "(__fish_print_packages)"
    set -l listrepos     "(cat /etc/pacman.conf | grep '^\[.\+\]' | sed 's/[]\[]//g')"
    set -l listgroups    "(pacman -Sg | sed 's/\(.*\)/\1\tPackage group/g')"

    set -l noopt     'commandline | not __fish_sgrep -qe "-[a-z]*[ABCDLMOQRSTU]\|--aursync\|--save\|--downgrade\|--viewlog\|--abssync\|--orphans\|--database\|--query\|--sync\|--remove\|--upgrade\|--deptest"'
    set -l database  'commandline | __fish_sgrep -qe "-[a-z]*D\|--database"'
    set -l query     'commandline | __fish_sgrep -qe "-[a-z]*Q\|--query"'
    set -l remove    'commandline | __fish_sgrep -qe "-[a-z]*R\|--remove"'
    set -l sync      'commandline | __fish_sgrep -qe "-[a-z]*S\|--sync"'
    set -l upgrade   'commandline | __fish_sgrep -qe "-[a-z]*U\|--upgrade"'
    set -l aur       'commandline | __fish_sgrep -qe "-[a-z]*A\|--aursync"'
    set -l abs       'commandline | __fish_sgrep -qe "-[a-z]*M\|--abssync"'
    set -l save      'commandline | __fish_sgrep -qe "-[a-z]*B\|--save"'
    set -l downgrade 'commandline | __fish_sgrep -qe "-[a-z]*C\|--downgrade"'
    set -l orphans   'commandline | __fish_sgrep -qe "-[a-z]*O\|--orphans"'
    set -l logfile   'commandline | __fish_sgrep -qe "-[a-z]*L\|--viewlog"'
    set -l search    'commandline | __fish_sgrep -qe "-[a-zA]*s\|--search"'

    # By default fish expands the arguments with the option which is not desired
    # due to performance reasons.
    # This will make sure we are expanding an argument and not an option:
    set -l argument 'not expr -- (commandline --current-token) : "^-.*" > /dev/null'

    # Primary operations
    complete -c $progname -s A -f -l aursync   -n $noopt -d 'Synchronize AUR packages'
    complete -c $progname -s B -f -l save      -n $noopt -d 'Save and restore package state'
    complete -c $progname -s C -f -l downgrade -n $noopt -d 'Package cache actions'
    complete -c $progname -s D -f -l database  -n $noopt -d 'Modify the package database'
    complete -c $progname -s L -f -l viewlog   -n $noopt -d 'Pacman log actions'
    complete -c $progname -s M -f -l abssync   -n $noopt -d 'Build packages from ABS'
    complete -c $progname -s O -f -l orphans   -n $noopt -d 'Operate on orphan packages'
    complete -c $progname -s Q -f -l query     -n $noopt -d 'Query the package database'
    complete -c $progname -s R -f -l remove    -n $noopt -d 'Remove packages from the system'
    complete -c $progname -s S -f -l sync      -n $noopt -d 'Synchronize packages'
    complete -c $progname -s T -f -l deptest   -n $noopt -d 'Check dependencies'
    complete -c $progname -s U -f -l upgrade   -n $noopt -d 'Upgrade or add a local package'
    complete -c $progname -l auradebug         -d 'Show settings while running'
    complete -c $progname -l no-pp             -d 'Do not use powerpill'
    complete -c $progname -l languages         -d 'Show available languages'
    complete -c $progname -l viewconf          -d 'View pacman.conf'
    complete -c $progname -s V -f -l version   -d 'Display version and exit'
    complete -c $progname -s h -f -l help      -d 'Display help'

    # General options
    complete -c $progname -s b -l dbpath -d 'Alternative database location' -xa '(__fish_complete_directories)'
    complete -c $progname -s r -l root -d 'Alternative installation root'
    complete -c $progname -s v -l verbose -d 'Output more status messages'
    complete -c $progname -l arch      -d 'Alternate architecture'
    complete -c $progname -l cachedir  -d 'Alternative package cache location'
    complete -c $progname -l config    -d 'Alternate config file'
    complete -c $progname -l debug     -d 'Display debug messages'
    complete -c $progname -l gpgdir    -d 'GPG directory to verify signatures'
    complete -c $progname -l logfile   -d 'Specify alternative log file'
    complete -c $progname -l noconfirm -d 'Bypass any question'

    # Transaction options (sync, remove, upgrade)
    for condition in sync remove upgrade
        complete -c $progname -n $$condition -s d -l nodeps     -d 'Skip [all] dependency checks'
        complete -c $progname -n $$condition -l dbonly          -d 'Modify database entry only'
        complete -c $progname -n $$condition -l noprogressbar   -d 'Do not display progress bar'
        complete -c $progname -n $$condition -l noscriptlet     -d 'Do not execute install script'
        complete -c $progname -n $$condition -s p -l print      -d 'Dry run, only print targets'
        complete -c $progname -n $$condition -l print-format -x -d 'Specify printf-like format'
    end

    # Database and upgrade options (database, sync, upgrade)
    for condition in database sync upgrade
        complete -c $progname -n $$condition      -l asdeps      -d 'Mark PACKAGE as dependency'
        complete -c $progname -n $$condition      -l asexplicit  -d 'Mark PACKAGE as explicitly installed'
    end

    # Upgrade options (sync, upgrade)
    for condition in sync upgrade
        complete -c $progname -n $$condition -s f -l force       -d 'Bypass file conflict checks'
        complete -c $progname -n $$condition      -l ignore      -d 'Ignore upgrade of PACKAGE' -xa "$listinstalled"
        complete -c $progname -n $$condition      -l ignoregroup -d 'Ignore upgrade of GROUP' -xa "$listgroups"
        complete -c $progname -n $$condition      -l needed      -d 'Do not reinstall up-to-date targets'
        complete -c $progname -n $$condition      -l recursive   -d 'Recursively reinstall all dependencies'
    end

    # Query and sync options
    for condition in query sync
        complete -c $progname -n $$condition -s g -l groups     -d 'Display all packages in GROUP' -xa "$listgroups"
        complete -c $progname -n $$condition -s i -l info       -d 'Display information on PACKAGE'
        complete -c $progname -n $$condition -s q -l quiet      -d 'Show less information'
        complete -c $progname -n $$condition -s s -l search -r  -d 'Search packages for regexp'
    end

    for condition in abs aur
        complete -c $progname -n $$condition -s a -l delmakedeps  -d 'Remove packages only needed during installation'
        complete -c $progname -n $$condition -s d -l deps         -d 'View package dependencies'
        complete -c $progname -n $$condition -s i -l info         -d 'View package information'
        complete -c $progname -n $$condition -s k -l diff         -d 'Show PKGBUILD diffs'
        complete -c $progname -n $$condition -s p -l pkgbuild     -d 'View the packages\'s PKGBUILD'
        complete -c $progname -n $$condition -s x -l unsuppress   -d 'Show makepkg output'
        complete -c $progname -n $$condition -l absdeps           -d 'Build dependencies from ABS'
    end

    # AUR options
    complete -c $progname -n $aur -s q -l quiet        -d 'Show less information'
    complete -c $progname -n $aur -s s -l search -r    -d 'Search AUR by string matching'
    complete -c $progname -n $aur -s u -l sysupgrade   -d 'Upgrade all installed AUR packages'
    complete -c $progname -n $aur -s w -l downloadonly -d 'Download the source tarball'
    complete -c $progname -n $aur -l aurignore -r      -d 'Ignore given comma-separated packages'
    complete -c $progname -n $aur -l build -r          -d 'Specify a build location'
    complete -c $progname -n $aur -l builduser -r      -d 'User to build as'
    complete -c $progname -n $aur -l custom            -d 'Run customizepkg before build'
    complete -c $progname -n $aur -l devel             -d 'Include -git/-svn/etc packages'
    complete -c $progname -n $aur -l hotedit           -d 'Prompt for PKGBUILD editing'
    complete -c $progname -n $aur -l ignorearch        -d 'Ignore architecture checking'
    complete -c $progname -n "$aur; and $search" -l abc  -d 'Sort alphabetically'
    complete -c $progname -n "$aur; and $search" -l head -d 'Only show the first 10 results'
    complete -c $progname -n "$aur; and $search" -l tail -d 'Only show the last 10 results'

    # Backup options
    complete -c $progname -n $save -s c -l clean -d 'Remove all but the given number of backups'
    complete -c $progname -n $save -s r -l restore  -d 'Restores a record kept with -B'

    # Downgrade options
    complete -c $progname -n $downgrade -s b -l backup -d 'Backup to directory'
    complete -c $progname -n $downgrade -s c -l clean  -d 'Save this many versions'
    complete -c $progname -n $downgrade -s s -l search -r -d 'Search via regex'

    # Logfile options
    complete -c $progname -n $logfile -s i -l info   -d 'Show package history'
    complete -c $progname -n $logfile -s s -l search -r -d 'Search via regex'

    # ABS options
    complete -c $progname -n $abs -s s -l search -r -d 'Search ABS by regex'
    complete -c $progname -n $abs -s c -l clean    -d 'Delete local ABS tree'
    complete -c $progname -n $abs -s y -l refresh  -d 'Download fresh copy of the package list'
    complete -c $progname -n $abs -s t -l treesync -d 'Sync the given to local ABS tree'
    complete -c $progname -n $abs -l absdeps       -d 'Download fresh copy of the package list'

    # Orphan options
    complete -c $progname -n $orphans -s j -l abandon -d 'Uninstall orphan packages'

    # Query options
    complete -c $progname -n $query -s c -l changelog  -d 'View the change log of PACKAGE'
    complete -c $progname -n $query -s d -l deps       -d 'List only non-explicit packages (dependencies)'
    complete -c $progname -n $query -s e -l explicit   -d 'List only explicitly installed packages'
    complete -c $progname -n $query -s k -l check      -d 'Check if all files owned by PACKAGE are present'
    complete -c $progname -n $query -s l -l list       -d 'List all files owned by PACKAGE'
    complete -c $progname -n $query -s m -l foreign    -d 'List all packages not in the database'
    complete -c $progname -n $query -s o -l owns -r    -d 'Search for the package that owns FILE' -xa ''
    complete -c $progname -n $query -s p -l file       -d 'Apply the query to a package file, not package' -xa ''
    complete -c $progname -n $query -s t -l unrequired -d 'List only unrequired packages'
    complete -c $progname -n $query -s u -l upgrades   -d 'List only out-of-date packages'
    complete -c $progname -n "$query; and $argument" -xa $listinstalled -d 'Installed package'

    # Remove options
    complete -c $progname -n $remove -s c -l cascade   -d 'Also remove packages depending on PACKAGE'
    complete -c $progname -n $remove -s n -l nosave    -d 'Ignore file backup designations'
    complete -c $progname -n $remove -s s -l recursive -d 'Also remove dependencies of PACKAGE'
    complete -c $progname -n $remove -s u -l unneeded  -d 'Only remove targets not required by PACKAGE'
    complete -c $progname -n "$remove; and $argument" -xa $listinstalled -d 'Installed package'

    # Sync options
    complete -c $progname -n $sync -s c -l clean        -d 'Remove [all] packages from cache'
    complete -c $progname -n $sync -s l -l list -xa "$listrepos" -d 'List all packages in REPOSITORY'
    complete -c $progname -n $sync -s u -l sysupgrade   -d 'Upgrade all packages that are out of date'
    complete -c $progname -n $sync -s w -l downloadonly -d 'Only download the target packages'
    complete -c $progname -n $sync -s y -l refresh      -d 'Download fresh copy of the package list'
    complete -c $progname -n "$argument; and $sync" -xa "$listall $listgroups"

    # Upgrade options
    complete -c $progname -n "$upgrade; and $argument" -xa '(__fish_complete_suffix pkg.tar.xz)' -d 'Package file'
    complete -c $progname -n "$upgrade; and $argument" -xa '(__fish_complete_suffix pkg.tar.gz)' -d 'Package file'
end

