# Completions for pacman (https://man.archlinux.org/man/pacman.8)
# Author: Giorgio Lando <patroclo7@gmail.com>
# Updated by maxfl, SanskritFritz, faho, f1u77y, akiirui, marmitar

set -l progname pacman

# Checks if the given or any command is in the command line.
function __fish_pacman_has_operation
    set -l operations '-s D database' '-s Q query' '-s R remove' '-s S sync' '-s T deptest' '-s U upgrade' '-s F files'
    if test (count $argv) -gt 0
        set operations (string match -er (string join '|' -- $argv) -- $operations)
    end
    __fish_contains_opt (string split ' ' -- $operations)
end

function __fish_pacman_print_package_groups
    argparse i/installed -- $argv
    or return 1

    set -l groups
    if not set -q _flag_installed
        set -a groups (pacman -Sg)
    else
        set -a groups (pacman -Qg | string split -f 1 ' ' | uniq)
    end
    printf '%s\n' $groups\t'Package Group'
end

# Used for pacman -F, which only handles basenames and absolute paths.
function __fish_pacman_complete_absolute_paths
    set -l target (commandline -ct)

    if string match -q '/*' -- $target
        __fish_complete_path $target
    else if test -z $target
        printf '%s\n' ''\t'Filename' /\t'Absolute Path'
    end
end

# Do a normal __fish_print_pacman_packages, unless the command line has "$repo/...".
# In that case, no package would matched, so it searches for packages in that repo
# instead.
function __fish_pacman_complete_packages_from_repo
    set -l repo (commandline -ct | string replace -fr '^([^/]+)/.*$' '$1')
    __fish_print_pacman_packages --repo=$repo 2>/dev/null
    if test -z "$repo"
        printf '%s\n' (__fish_print_pacman_repos)/\t'Repository'
    end
end

# See PKGEXT on https://man.archlinux.org/man/makepkg.conf.5
set -l pkgexts (string match --all --regex '\S+' '
    .pkg.tar.gz
    .pkg.tar.bz2
    .pkg.tar.xz
    .pkg.tar.zst
    .pkg.tar.lzo
    .pkg.tar.lrz
    .pkg.tar.lz4
    .pkg.tar.lz
    .pkg.tar.Z
    .pkg.tar
')

# From https://rfc.archlinux.page/0032-arch-linux-ports/
set -l linux_architectures '
    x86_64
    x86_64_v2
    x86_64_v3
    x86_64_v4
    i486
    i686
    pentium4
    aarch64
    armv7h
    riscv64
    loong64
'

# Exactly one main operation needed
complete -c $progname -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -D -d 'Modify the package database' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -Q -d 'Query the package database' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -R -d 'Remove packages from the system' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -S -d 'Synchronize packages' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -T -d 'Check dependencies' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -U -d 'Add a package from a file or URL' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -a -F -d 'Query the files database' -f

## OPERATIONS
complete -c $progname -n 'not __fish_pacman_has_operation' -s D -l database -d 'Modify the package database' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -s Q -l query -d 'Query the package database' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -s R -l remove -d 'Remove packages from the system' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -s S -l sync -d 'Synchronize packages' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -s T -l deptest -d 'Check dependencies' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -s U -l upgrade -d 'Add a package from a file or URL' -f
complete -c $progname -n 'not __fish_pacman_has_operation' -s F -l files -d 'Query the files database' -f
complete -c $progname -s V -l version -d 'Display version and exit' -f
complete -c $progname -s h -l help -d 'Display help' -f

## OPTIONS
# Only offer these once a command has been given so they get prominent display
complete -c $progname -n __fish_pacman_has_operation -s b -l dbpath -d 'Alternate database location' -xa '(__fish_complete_directories)'
complete -c $progname -n __fish_pacman_has_operation -s r -l root -d 'Alternate installation root' -xa '(__fish_complete_directories)'
complete -c $progname -n __fish_pacman_has_operation -s v -l verbose -d 'Output paths of config files' -f
complete -c $progname -n __fish_pacman_has_operation -l arch -d 'Alternate architecture' -xa $linux_architectures
complete -c $progname -n __fish_pacman_has_operation -l cachedir -d 'Alternate package cache location' -xa '(__fish_complete_directories)'
complete -c $progname -n __fish_pacman_has_operation -l color -d 'Colorize the output' -fa 'auto always never'
complete -c $progname -n __fish_pacman_has_operation -l config -d 'Alternate config file' -xka '(__fish_complete_suffix .conf)'
complete -c $progname -n __fish_pacman_has_operation -l debug -d 'Display debug messages' -f
complete -c $progname -n __fish_pacman_has_operation -l gpgdir -d 'Alternate home directory for GnuPG' -xa '(__fish_complete_directories)'
complete -c $progname -n __fish_pacman_has_operation -l hookdir -d 'Alternate hook location' -xa '(__fish_complete_directories)'
complete -c $progname -n __fish_pacman_has_operation -l logfile -d 'Alternate log file' -rF
complete -c $progname -n __fish_pacman_has_operation -l noconfirm -d 'Bypass any confirmation' -f
complete -c $progname -n __fish_pacman_has_operation -l confirm -d 'Cancels a previous --noconfirm' -f
complete -c $progname -n __fish_pacman_has_operation -l disable-download-timeout -d 'Use relaxed timeouts for download' -f
complete -c $progname -n __fish_pacman_has_operation -l sysroot -d 'Operate on a mounted guest system (root-only)' -xa '(__fish_complete_directories)'
complete -c $progname -n __fish_pacman_has_operation -l disable-sandbox -d '--disable-sandbox-filesystem and --disable-sandbox-syscalls' -f
complete -c $progname -n __fish_pacman_has_operation -l disable-sandbox-filesystem -d 'Disable the filesystem restrictions' -f
complete -c $progname -n __fish_pacman_has_operation -l disable-sandbox-syscalls -d 'Disable the syscall filtering' -f

## TRANSACTION OPTIONS (APPLY TO -S, -R AND -U)
complete -c $progname -n '__fish_pacman_has_operation S R U' -s d -l nodeps -d 'Skip [all] dependency checks' -f
complete -c $progname -n '__fish_pacman_has_operation S R U' -l assume-installed -d 'Add a virtual package to satisfy dependencies' -xa '(__fish_print_pacman_packages)'
complete -c $progname -n '__fish_pacman_has_operation S R U' -l dbonly -d 'Modify database entry only' -f
complete -c $progname -n '__fish_pacman_has_operation S R U' -l noprogressbar -d 'Do not display progress bar' -f
complete -c $progname -n '__fish_pacman_has_operation S R U' -l noscriptlet -d 'Do not execute install script' -f
complete -c $progname -n '__fish_pacman_has_operation S R U' -s p -l print -d 'Dry run, only print targets' -f
complete -c $progname -n '__fish_pacman_has_operation S R U' -l print-format -d 'Specify printf-like format' -x

## UPGRADE OPTIONS (APPLY TO -S AND -U)
complete -c $progname -n '__fish_pacman_has_operation S U' -s w -l downloadonly -d 'Retrieve packages but do not install' -f
complete -c $progname -n '__fish_pacman_has_operation S U' -l asdeps -d 'Install packages as non-explicitly installed' -f
complete -c $progname -n '__fish_pacman_has_operation S U' -l asexplicit -d 'Install packages as explicitly installed' -f
complete -c $progname -n '__fish_pacman_has_operation S U' -l ignore -d 'Ignore a package upgrade (can be used more than once)' -xa '(__fish_print_pacman_packages)'
complete -c $progname -n '__fish_pacman_has_operation S U' -l ignoregroup -d 'Ignore a group upgrade (can be used more than once)' -xa '(__fish_pacman_print_package_groups)'
complete -c $progname -n '__fish_pacman_has_operation S U' -l needed -d 'Do not reinstall up to date packages' -f
complete -c $progname -n '__fish_pacman_has_operation S U' -l overwrite -d 'Overwrite conflicting files (can be used more than once)' -rF

## QUERY OPTIONS (APPLY TO -Q)
complete -c $progname -n '__fish_pacman_has_operation Q' -s c -l changelog -d 'View the ChangeLog of PACKAGE' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s d -l deps -d 'List only non-explicit packages (dependencies)' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s e -l explicit -d 'List only explicitly installed packages' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s g -l groups -d 'List only packages that are part of a group' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s i -l info -d 'View PACKAGE [backup files] information' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s k -l check -d 'Check that PACKAGE files exist' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s l -l list -d 'List the files owned by PACKAGE' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s m -l foreign -d 'List installed packages not found in sync database' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s n -l native -d 'List installed packages only found in sync database' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s o -l owns -d 'Query for packages that own FILE' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s p -l file -d 'Query a package file instead of the database' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s q -l quiet -d 'Show less information' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s s -l search -d 'Search locally-installed packages for regexp' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s t -l unrequired -d 'List only unrequired packages [and optdepends]' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -s u -l upgrades -d 'List only out-of-date packages' -f
complete -c $progname -n '__fish_pacman_has_operation Q' -n 'not __fish_contains_opt -s g groups -s o owns -s p file' -d Package -xa '(__fish_print_pacman_packages --installed)'
complete -c $progname -n '__fish_pacman_has_operation Q' -n '__fish_contains_opt -s g groups' -d 'Package Group' -xa '(__fish_pacman_print_package_groups --installed)'
complete -c $progname -n '__fish_pacman_has_operation Q' -n '__fish_contains_opt -s o owns' -d File -rF
complete -c $progname -n '__fish_pacman_has_operation Q' -n '__fish_contains_opt -s p file' -d 'Package File' -xka "(__fish_complete_suffix $pkgexts)"

## REMOVE OPTIONS (APPLY TO -R)
complete -c $progname -n '__fish_pacman_has_operation R' -s c -l cascade -d 'Also remove packages depending on PACKAGE' -f
complete -c $progname -n '__fish_pacman_has_operation R' -s n -l nosave -d 'Ignore file backup designations' -f
complete -c $progname -n '__fish_pacman_has_operation R' -s s -l recursive -d 'Also remove dependencies of PACKAGE' -f
complete -c $progname -n '__fish_pacman_has_operation R' -s u -l unneeded -d 'Remove targets not required by any package' -f
complete -c $progname -n '__fish_pacman_has_operation R' -d Package -xa '(__fish_print_pacman_packages --installed)'
complete -c $progname -n '__fish_pacman_has_operation R' -d 'Package Group' -xa '(__fish_pacman_print_package_groups --installed)'

## SYNC OPTIONS (APPLY TO -S)
complete -c $progname -n '__fish_pacman_has_operation S' -s c -l clean -d 'Remove [all] packages from cache' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s g -l groups -d 'Display members of [all] package GROUP' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s i -l info -d 'View PACKAGE [extended] information' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s l -l list -d 'List all packages in REPOSITORY' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s q -l quiet -d 'Show less information' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s s -l search -d 'Search remote repositories for regexp' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s u -l sysupgrade -d 'Upgrade all packages that are out of date' -f
complete -c $progname -n '__fish_pacman_has_operation S' -s y -l refresh -d 'Download fresh package databases [force]' -f
complete -c $progname -n '__fish_pacman_has_operation S' -n 'not __fish_contains_opt -s g groups -s l list' -d Package -xa '(__fish_pacman_complete_packages_from_repo)'
complete -c $progname -n '__fish_pacman_has_operation S' -n '__fish_contains_opt -s g groups' -d 'Package Group' -xa '(__fish_pacman_print_package_groups)'
complete -c $progname -n '__fish_pacman_has_operation S' -n '__fish_contains_opt -s l list' -d Repository -xa '(__fish_print_pacman_repos)'

## DATABASE OPTIONS (APPLY TO -D)
complete -c $progname -n '__fish_pacman_has_operation D' -l asdeps -d 'Mark PACKAGE as dependency' -f
complete -c $progname -n '__fish_pacman_has_operation D' -l asexplicit -d 'Mark PACKAGE as explicitly installed' -f
complete -c $progname -n '__fish_pacman_has_operation D' -s k -l check -d 'Check database validity' -f
complete -c $progname -n '__fish_pacman_has_operation D' -s q -l quiet -d 'Suppress output of success messages' -f
complete -c $progname -n '__fish_pacman_has_operation D' -n '__fish_contains_opt asdeps asexplicit' -d Package -xa '(__fish_print_pacman_packages --installed)'

## FILE OPTIONS (APPLY TO -F)
complete -c $progname -n '__fish_pacman_has_operation F' -s y -l refresh -d 'Download fresh package databases [force]' -f
complete -c $progname -n '__fish_pacman_has_operation F' -s l -l list -d 'List the files owned by PACKAGE' -f
complete -c $progname -n '__fish_pacman_has_operation F' -s x -l regex -d 'Interpret each query as a regular expression' -f
complete -c $progname -n '__fish_pacman_has_operation F' -s q -l quiet -d 'Show less information' -f
complete -c $progname -n '__fish_pacman_has_operation F' -l machinereadable -d 'Print each match in a machine readable output format' -f
complete -c $progname -n '__fish_pacman_has_operation F' -n '__fish_contains_opt -s l list' -d Package -xa '(__fish_print_pacman_packages)'
complete -c $progname -n '__fish_pacman_has_operation F' -n 'not __fish_contains_opt -s l list' -d File -xa '(__fish_pacman_complete_absolute_paths)'

# No extra options (-U)
complete -c $progname -n '__fish_pacman_has_operation U' -d 'Package File' -xka "(__fish_complete_suffix $pkgexts)"
