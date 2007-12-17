# Completions for pacman, using short options when possible
# Author: Giorgio Lando <patroclo7@gmail.com>

# Primary operations
complete -c pacman -s A -f -n '__fish_not_contain_opt -s F -s U -s Q -s R -s S -s h' -d 'Add a package to the system'
complete -c pacman -s F -f -n '__fish_not_contain_opt -s A -s U -s Q -s R -s S -s h' -d 'Upgrade a package which is already in the system'
complete -c pacman -s U -f -n '__fish_not_contain_opt -s A -s F -s Q -s R -s S -s h' -d 'Upgrade or add a package in the system'
complete -c pacman -s Q -f -n '__fish_not_contain_opt -s A -s F -s U -s R -s S -s h' -d 'Query the package database'
complete -c pacman -s R -f -n '__fish_not_contain_opt -s A -s F -s U -s Q -s S -s h' -d 'Remove packages from the system'
complete -c pacman -s S -f -n '__fish_not_contain_opt -s A -s F -s U -s Q -s R -s h' -d 'Synchronize packages'
complete -c pacman -o V -f -d 'Display version and exit'

# General options
complete -c pacman -n '__fish_contains_opt -s A -s F -s Q -s R -s S -s U' -s d -d 'Skips all dependency checks'
complete -c pacman -n '__fish_contains_opt -s A -s F -s Q -s R -s S -s U' -s f -d 'Bypass file conflict checks'
complete -c pacman -n '__fish_contains_opt -s A -s F -s Q -s R -s S -s U' -s r -d 'Specify an alternative installation root'
complete -c pacman -n '__fish_contains_opt -s A -s F -s Q -s R -s S -s U' -s v -d 'Output more status messages'
complete -c pacman -n '__fish_contains_opt -s A -s F -s Q -s R -s S -s U' -l config -d 'Specify an altenate config file'
complete -c pacman -n '__fish_contains_opt -s A -s F -s Q -s R -s S -s U' -l noconfirm -d 'Bypass any question'
complete -c pacman -s h -f -d 'Display syntax and help for the operation'

# Query options
complete -c pacman -n '__fish_contains_opt -s Q' -s c -d 'View the changelog of a package'
complete -c pacman -n '__fish_contains_opt -s Q' -s g -d 'Display all packages in the group'
complete -c pacman -n '__fish_contains_opt -s Q' -s o -d 'Search for the package that owns file'
complete -c pacman -n '__fish_contains_opt -s Q' -s e -d 'List orphan packages'
complete -c pacman -n '__fish_contains_opt -s Q' -s m -d 'List all packages which are not in the sync database'
complete -c pacman -n '__fish_contains_opt -s Q' -s u -d 'List all out of date packages in the system'
complete -c pacman -n '__fish_contains_opt -s Q' -s s -d 'Search each locally installed package for regexp'
complete -c pacman -n '__fish_contains_opt -s Q' -s i -d 'Display information on a given package'
complete -c pacman -n '__fish_contains_opt -s Q' -s l -d 'List all files owned by a given package'
complete -c pacman -n '__fish_contains_opt -s Q' -s p -d 'Apply the query to a package file and not to an installed package'

# Remove options
complete -c pacman -n '__fish_contains_opt -s R' -s c -d 'Remove also the packages that depends on the target packages' 
complete -c pacman -n '__fish_contains_opt -s R' -s k -d 'Remove the database entries only' 
complete -c pacman -n '__fish_contains_opt -s R' -s n -d 'Ignore file backup designations' 
complete -c pacman -n '__fish_contains_opt -s R' -s s -d 'Remove also the dependencies of the target packages'

# Sync options
complete -c pacman -n '__fish_contains_opt -s S' -s c -d 'Remove old packages from the cache; if iterated, remove all the packages from the cache'
complete -c pacman -n '__fish_contains_opt -s S' -s e -d 'Install only the dependencies of the target packages'
complete -c pacman -n '__fish_contains_opt -s S' -s g -d 'Display all packages in the group'
complete -c pacman -n '__fish_contains_opt -s S' -s i -d 'Display informations for the target package'
complete -c pacman -n '__fish_contains_opt -s S' -s l -d 'List all packages in the repository'
complete -c pacman -n '__fish_contains_opt -s S' -s p -d 'Display URIs for target packages and dependencies'
complete -c pacman -n '__fish_contains_opt -s S' -s s -d 'Search packages and descriptions in the repos for regexp'
complete -c pacman -n '__fish_contains_opt -s S' -s u -d 'Upgrade all packages that are out of date'
complete -c pacman -n '__fish_contains_opt -s S' -s w -d 'Only download the target packages'
complete -c pacman -n '__fish_contains_opt -s S' -s y -d 'Download a fresh copy of the master package list from the servers'
complete -c pacman -n '__fish_contains_opt -s S' -l ignore -d 'Ignore upgrades of the target packages'
complete -c pacman -n '__fish_contains_opt -s S' -l noprogressbar -d 'Do not show a progressbar when downloading files'

# Complete with local packages
complete -c pacman -n '__fish_contains_opt -s A -s F -s U' -a '(__fish_complete_suffix pkg.tar.gz)' -d 'Local package'
# Complete with installed packages
complete -c pacman -n '__fish_contains_opt -s Q -s R' -a '(pacman -Q | cut --delim " " --fields 1)' -d 'Installed package' 
# Complete with packages in the repos
complete -c pacman -n '__fish_contains_opt -s S' -a '(pacman -Sl | cut --delim " " --fields 2 | sort)' -d 'Repo package'
