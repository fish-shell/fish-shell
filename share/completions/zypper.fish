# completion for zypper

set -g __fish_zypper_all_commands shell sh repos lr addrepo ar removerepo rr renamerepo nr modifyrepo mr refresh ref clean cc services ls addservice as modifyservice ms removeservice rs refresh-services refs install in remove rm verify ve source-install si install-new-recommends inr update up list-updates lu patch list-patches lp dist-upgrade dup patch-check pchk search se info if patch-info pattern-info product-info patches pch packages pa patterns pt products pd what-provides wp addlock al removelock rl locks ll cleanlocks cl locales lloc addlocale aloc removelocale rloc versioncmp vcmp targetos tos licenses download source-download needs-rebooting ps
set -g __fish_zypper_subcommands (zypper -q subcommand | string split ', ')
set -g __fish_zypper_pkg_commands install in remove rm info if addlock al removelock rl source-install si update up download
set -g __fish_zypper_repo_commands repos lr removerepo rr renamerepo nr modifyrepo mr refresh ref clean cc packages pa patches pch patterns pt products pd
set -g __fish_zypper_file_commands install in addrepo ar
set -g __fish_zypper_package_types package patch pattern product srcpackage application
set -g __fish_zypper_download_modes only in-advance in-heaps as-needed
set -g __fish_zypper_solver_focus_modes Job Installed Update

function __fish_zypper_cmd_in_array
    for i in (commandline -pcx)
        # -- is used to provide no options for contains
        # (if $i is equal to --optname without -- will be error)
        if contains -- $i $argv
            return 0
        end
    end

    return 1
end

function __fish_zypper_no_subcommand
    not __fish_zypper_cmd_in_array $__fish_zypper_all_commands $__fish_zypper_subcommands
end

function __fish_zypper_use_pkg
    __fish_zypper_cmd_in_array $__fish_zypper_pkg_commands
end

function __fish_zypper_use_repo
    __fish_zypper_cmd_in_array $__fish_zypper_repo_commands
end

function __fish_zypper_not_use_file
    not __fish_zypper_cmd_in_array $__fish_zypper_file_commands
end

function __fish_zypper_print_repos
    argparse e/enabled -- $argv
    set -q _flag_e; and set -l enabled -E
    set -l zypper_lr (zypper -t lr $enabled)
    # Because spaces and special characters are allowed in repo aliases (bad
    # practice though, but allowed), it's impossible to parse the aliases from
    # zypper's output correctly. So we fetch them from the repo files.
    set -l repos (cat /etc/zypp/repos.d/*.repo | string replace -rf '^\[(.+)\]$' '$1')
    # Then use the aliases to match their names from zypper's output.
    string replace -rf '^[\d\s]+\| ('(string escape -n $repos | string join \|)') +\| (.+)\s+\|.*\|.*\|.*$' '$1\t$2' -- $zypper_lr
end

function __fish_zypper_print_packages
    set -l repo
    set -l type
    set -l idx
    set -l args (commandline -pxc)
    while set -q args[1]
        switch $args[1]
            case -t --type
                set -e args[1]
                if contains -- $args[1] $__fish_zypper_package_types
                    set type $args[1]
                end
            case -r --repo --from
                set -e args[1]
                if contains -- $args[1] (__fish_zypper_print_repos -e | string replace -r '^(.+)\t.*$' '$1')
                    set repo $repo $args[1]
                end
        end
        set -e args[1]
    end

    if __fish_zypper_is_subcommand_rm
        set idx /var/cache/zypp/solv/@System/solv.idx
    else if test -z "$repo"
        set idx /var/cache/zypp/solv/*/solv.idx
    else
        set idx /var/cache/zypp/solv/$repo/solv.idx
    end

    if test -z "$type"
        cat $idx | string replace -rf '^([^\t]+)\t.*\t.*$' '$1'
    else if test "$type" = package
        cat $idx | string match -rv '^('(string join \| $__fish_zypper_package_types)'):' | string replace -rf '^([^\t]+)\t.*\t.*$' '$1'
    else
        cat $idx | string replace -rf "^$type"':([^\t]+)\t.*\t.*$' '$1\t'"$type"
    end
end

complete -n __fish_zypper_use_pkg -c zypper -a '(__fish_zypper_print_packages)' -d Package
complete -f -n __fish_zypper_use_repo -c zypper -a '(__fish_zypper_print_repos)' -d Repo
complete -f -n __fish_zypper_not_use_file -c zypper

# Commands:

complete -f -n __fish_zypper_no_subcommand -c zypper -a 'help ?' -d 'Print help'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'shell sh' -d 'Accept multiple commands at once'

#  Repository Management:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'repos lr' -d 'List all defined repositories'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'addrepo ar' -d 'Add a new repository'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'removerepo rr' -d 'Remove specified repository'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'renamerepo nr' -d 'Rename specified repository'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'modifyrepo mr' -d 'Modify specified repository'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'refresh ref' -d 'Refresh all repositories'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'clean cc' -d 'Clean local caches'

#  Service Management:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'services ls' -d 'List all defined services'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'addservice as' -d 'Add a new service'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'modifyservice ms' -d 'Modify specified service'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'removeservice rs' -d 'Remove specified service'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'refresh-services refs' -d 'Refresh all services'

#  Software Management:
complete -n __fish_zypper_no_subcommand -c zypper -a 'install in' -d 'Install packages'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'remove rm' -d 'Remove packages'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'verify ve' -d 'Verify integrity of package dependencies'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'source-install si' -d 'Install source packages and their build dependencies'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'install-new-recommends inr' -d 'Install newly added packages recommended by installed packages'

#  Update Management:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'update up' -d 'Update installed packages with newer versions'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'list-updates lu' -d 'List available updates'
complete -f -n __fish_zypper_no_subcommand -c zypper -a patch -d 'Install needed patches'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'list-patches lp' -d 'List needed patches'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'dist-upgrade dup' -d 'Perform a distribution upgrade'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'patch-check pchk' -d 'Check for patches'

#  Querying:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'search se' -d 'Search for packages matching a pattern'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'info if' -d 'Show full information for specified packages'
complete -f -n __fish_zypper_no_subcommand -c zypper -a patch-info -d 'Show full information for specified patches'
complete -f -n __fish_zypper_no_subcommand -c zypper -a pattern-info -d 'Show full information for specified patterns'
complete -f -n __fish_zypper_no_subcommand -c zypper -a product-info -d 'Show full information for specified products'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'patches pch' -d 'List all available patches'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'packages pa' -d 'List all available packages'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'patterns pt' -d 'List all available patterns'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'products pd' -d 'List all available products'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'what-provides wp' -d 'List packages providing specified capability'

#  Package Locks:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'addlock al' -d 'Add a package lock'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'removelock rl' -d 'Remove a package lock'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'locks ll' -d 'List current package locks'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'cleanlocks cl' -d 'Remove unused locks'

#  Locale Management:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'locales lloc' -d 'List requested locales (languages codes)'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'addlocale aloc' -d 'Add locale(s) to requested locales'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'removelocale rloc' -d 'Remove locale(s) from requested locales'

#  Other Commands:
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'versioncmp vcmp' -d 'Compare two version strings'
complete -f -n __fish_zypper_no_subcommand -c zypper -a 'targetos tos' -d 'Print the target operating system ID string'
complete -f -n __fish_zypper_no_subcommand -c zypper -a licenses -d 'Print report about licenses and EULAs of installed packages'
complete -f -n __fish_zypper_no_subcommand -c zypper -a download -d 'Download rpms specified on the commandline to a local directory'
complete -f -n __fish_zypper_no_subcommand -c zypper -a source-download -d 'Download source rpms for all installed packages to a local directory'
complete -f -n __fish_zypper_no_subcommand -c zypper -a needs-rebooting -d 'Check if the needs-reboot flag was set'
complete -f -n __fish_zypper_no_subcommand -c zypper -a ps -d 'List running processes still using deleted files'

#  Subcommands:

complete -f -n __fish_zypper_no_subcommand -c zypper -a subcommand -d 'Lists available subcommands'
complete -f -n __fish_zypper_no_subcommand -c zypper -a "$__fish_zypper_subcommands"

# Global Options:

complete -c zypper -n __fish_zypper_no_subcommand -l help -s h -d Help
complete -c zypper -n __fish_zypper_no_subcommand -l version -s V -d 'Output the version number'
complete -c zypper -n __fish_zypper_no_subcommand -l promptids -d 'Output a list of zypper user prompts'
complete -c zypper -n __fish_zypper_no_subcommand -l config -r -s c -d 'Use specified config file instead of the default'
complete -c zypper -n __fish_zypper_no_subcommand -l userdata -d 'User defined transaction id used in history and plugins'
complete -c zypper -n __fish_zypper_no_subcommand -l quiet -s q -d 'Suppress normal output, print only error messages'
complete -c zypper -n __fish_zypper_no_subcommand -l verbose -s v -d 'Increase verbosity'
complete -c zypper -n __fish_zypper_no_subcommand -l no-abbrev -s A -d 'Do not abbreviate text in tables'
complete -c zypper -n __fish_zypper_no_subcommand -l table-style -s s -d 'Table style (integer)'
complete -c zypper -n __fish_zypper_no_subcommand -l rug-compatible -s r -d 'Turn on rug compatibility'
complete -c zypper -n __fish_zypper_no_subcommand -l non-interactive -s n -d 'Do not ask anything, use default answers automatically'
complete -c zypper -n __fish_zypper_no_subcommand -l non-interactive-include-reboot-patches -d 'Do not treat patches as interactive, which have the rebootSuggested-flag set'
complete -c zypper -n __fish_zypper_no_subcommand -l xmlout -s x -d 'Switch to XML output'
complete -c zypper -n __fish_zypper_no_subcommand -l ignore-unknown -s i -d 'Ignore unknown packages'
complete -c zypper -n __fish_zypper_no_subcommand -l terse -s t -d 'Terse output for machine consumption. Implies --no-abbrev and --no-color'

complete -c zypper -n __fish_zypper_no_subcommand -l reposd-dir -s D -r -d 'Use alternative repository definition file directory'
complete -c zypper -n __fish_zypper_no_subcommand -l cache-dir -s C -r -d 'Use alternative directory for all caches'
complete -c zypper -n __fish_zypper_no_subcommand -l raw-cache-dir -r -d 'Use alternative raw meta-data cache directory'
complete -c zypper -n __fish_zypper_no_subcommand -l solv-cache-dir -r -d 'Use alternative solv file cache directory'
complete -c zypper -n __fish_zypper_no_subcommand -l pkg-cache-dir -r -d 'Use alternative package cache directory'

#  Repository Options
complete -c zypper -n __fish_zypper_no_subcommand -l no-gpg-checks -d 'Ignore GPG check failures and continue'
complete -c zypper -n __fish_zypper_no_subcommand -l gpg-auto-import-keys -d 'Automatically trust and import new repository signing keys'
complete -c zypper -n __fish_zypper_no_subcommand -l plus-repo -s p -rf -d 'Use an additional repository'
complete -c zypper -n __fish_zypper_no_subcommand -l plus-content -s p -rf -d 'Additionally use disabled repositories providing a specific keyword'
complete -c zypper -n __fish_zypper_no_subcommand -l disable-repositories -d 'Do not read meta-data from repositories'
complete -c zypper -n __fish_zypper_no_subcommand -l no-refresh -d 'Do not refresh the repositories'
complete -c zypper -n __fish_zypper_no_subcommand -l no-cd -d 'Ignore CD/DVD repositories'
complete -c zypper -n __fish_zypper_no_subcommand -l no-remote -d 'Ignore remote repositories'
complete -c zypper -n __fish_zypper_no_subcommand -l releasever -d 'Set the value of $releasever in all .repo files'

#  Target Options
complete -c zypper -n __fish_zypper_no_subcommand -l root -s R -r -d 'Operate on a different root directory'
complete -c zypper -n __fish_zypper_no_subcommand -l installroot -r -d 'Operate on a different root directory, but share repositories with the host'
complete -c zypper -n __fish_zypper_no_subcommand -l disable-system-resolvables -d 'Do not read installed packages'

function __fish_zypper_is_subcommand_lr
    __fish_zypper_cmd_in_array repos lr
end

complete -c zypper -n __fish_zypper_is_subcommand_lr -l export -r -s e -d 'Export all defined repositories as a single local .repo file'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l alias -s a -d 'Show also repository alias'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l name -s n -d 'Show also repository name'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l refresh -s r -d 'Show also the autorefresh flag'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l uri -s u -d 'Show also base URI of repositories'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l priority -s p -d 'Show also repository priority'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l details -s d -d 'Show more information like URI, priority, type'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l service -s s -d 'Show also alias of parent service'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l show-enabled-only -s E -d 'Show enabled repos only'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l sort-by-uri -s U -d 'Sort the list by URI'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l sort-by-name -s N -d 'Sort the list by name'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l sort-by-priority -s P -d 'Sort the list by repository priority'
complete -c zypper -n __fish_zypper_is_subcommand_lr -l sort-by-alias -s A -d 'Sort the list by alias'

function __fish_zypper_is_subcommand_ar
    __fish_zypper_cmd_in_array addrepo ar
end

complete -c zypper -n __fish_zypper_is_subcommand_ar -l repo -r -s r -d 'Just another means to specify a .repo file to read'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l check -s c -d 'Probe URI'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l no-check -s C -d "Don't probe URI, probe later during refresh"
complete -c zypper -n __fish_zypper_is_subcommand_ar -l priority -s p -d 'Set the priority of the repository'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l keep-packages -s k -d 'Enable RPM files caching'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l no-keep-packages -s K -d 'Disable RPM files caching'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l gpgcheck -s g -d 'Enable GPG check for this repository'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l gpgcheck-strict -d 'Enable strict GPG check for this repository'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l gpgcheck-allow-unsigned -d 'Short hand for --gpgcheck-allow-unsigned-repo --gpgcheck-allow-unsigned-package'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l gpgcheck-allow-unsigned-repo -d 'Enable GPG check but allow the repository metadata to be unsigned'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l gpgcheck-allow-unsigned-package -d 'Enable GPG check but allow installing unsigned packages from this repository.'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l no-gpgcheck -s G -d 'Disable GPG check for this repository'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l default-gpgcheck -d 'Use the global GPG check setting defined in /etc/zypp/zypp.conf'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l name -r -s n -d 'Specify descriptive name for the repository'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l enable -s d -d 'Add the repository as enabled'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l disable -s d -d 'Add the repository as disabled'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l refresh -s f -d 'Enable autorefresh of the repository'
complete -c zypper -n __fish_zypper_is_subcommand_ar -l no-refresh -s F -d 'Disable autorefresh of the repository'

function __fish_zypper_is_subcommand_rr
    __fish_zypper_cmd_in_array removerepo rr
end

complete -c zypper -n __fish_zypper_is_subcommand_rr -l loose-auth -d 'Ignore user authentication data in the URI'
complete -c zypper -n __fish_zypper_is_subcommand_rr -l loose-query -d 'Ignore query string in the URI'
complete -c zypper -n __fish_zypper_is_subcommand_rr -l all -s a -d 'Apply changes to all repositories'
complete -c zypper -n __fish_zypper_is_subcommand_rr -l local -s l -d 'Apply changes to all local repositories'
complete -c zypper -n __fish_zypper_is_subcommand_rr -l remote -s t -d 'Apply changes to all remote repositories'
complete -c zypper -n __fish_zypper_is_subcommand_rr -l medium-type -r -s m -d 'Apply changes to repositories of specified type'

function __fish_zypper_is_subcommand_mr
    __fish_zypper_cmd_in_array modifyrepo mr
end

complete -c zypper -n __fish_zypper_is_subcommand_mr -l name -r -s n -d 'Set a descriptive name for the repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l disable -s d -d "Disable the repository"
complete -c zypper -n __fish_zypper_is_subcommand_mr -l enable -s e -d 'Enable the repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l refresh -s f -d 'Enable auto-refresh of the repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l no-refresh -s F -d 'Disable auto-refresh of the repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l priority -r -s p -d 'Set the priority of the repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l keep-packages -s k -d 'Enable RPM files caching'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l no-keep-packages -s K -d 'Disable RPM files caching'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l gpgcheck -s g -d 'Enable GPG check for this repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l gpgcheck-strict -d 'Enable strict GPG check for this repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l gpgcheck-allow-unsigned -d 'Short hand for --gpgcheck-allow-unsigned-repo --gpgcheck-allow-unsigned-package'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l gpgcheck-allow-unsigned-repo -d 'Enable GPG check but allow the repository metadata to be unsigned'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l gpgcheck-allow-unsigned-package -d 'Enable GPG check but allow installing unsigned packages from this repository.'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l no-gpgcheck -s G -d 'Disable GPG check for this repository'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l default-gpgcheck -d 'Use the global GPG check setting defined in /etc/zypp/zypp.conf'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l all -s a -d 'Apply changes to all repositories'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l local -s l -d 'Apply changes to all local repositories'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l remote -s t -d 'Apply changes to all remote repositories'
complete -c zypper -n __fish_zypper_is_subcommand_mr -l medium-type -r -s m -d 'Apply changes to repositories of specified type'

function __fish_zypper_is_subcommand_ref
    __fish_zypper_cmd_in_array refresh ref
end

complete -c zypper -n __fish_zypper_is_subcommand_ref -l force -s f -d 'Force a complete refresh'
complete -c zypper -n __fish_zypper_is_subcommand_ref -l force-build -s b -d 'Force rebuild of the database'
complete -c zypper -n __fish_zypper_is_subcommand_ref -l force-download -s d -d 'Force download of raw metadata'
complete -c zypper -n __fish_zypper_is_subcommand_ref -l build-only -s B -d "Only build the database, don't download metadata"
complete -c zypper -n __fish_zypper_is_subcommand_ref -l download-only -s D -d "Only download raw metadata, don't build the database"
complete -c zypper -n __fish_zypper_is_subcommand_ref -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Refresh only specified repositories'
complete -c zypper -n __fish_zypper_is_subcommand_ref -l services -s s -d 'Refresh also services before refreshing repos'

function __fish_zypper_is_subcommand_ls
    __fish_zypper_cmd_in_array services ls
end

complete -c zypper -n __fish_zypper_is_subcommand_ls -l uri -s u -d 'Show also base URI of repositories'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l priority -s p -d 'Show also repository priority'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l details -s d -d 'Show more information like URI, priority, type'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l with-repos -s r -d 'Show also repositories belonging to the services'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l show-enabled-only -s E -d 'Show enabled repos only'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l sort-by-priority -s P -d 'Sort the list by repository priority'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l sort-by-uri -s U -d 'Sort the list by URI'
complete -c zypper -n __fish_zypper_is_subcommand_ls -l sort-by-name -s N -d 'Sort the list by name'

function __fish_zypper_is_subcommand_as
    __fish_zypper_cmd_in_array addservice as
end

complete -c zypper -n __fish_zypper_is_subcommand_as -l name -r -s n -d 'Set a descriptive name for the service'
complete -c zypper -n __fish_zypper_is_subcommand_as -l enable -s d -d 'Add the service as enabled'
complete -c zypper -n __fish_zypper_is_subcommand_as -l disable -s d -d 'Add the service as disabled'
complete -c zypper -n __fish_zypper_is_subcommand_as -l refresh -s f -d 'Enable autorefresh of the repository'
complete -c zypper -n __fish_zypper_is_subcommand_as -l no-refresh -s F -d 'Disable autorefresh of the repository'

function __fish_zypper_is_subcommand_ms
    __fish_zypper_cmd_in_array modifyservice ms
end

complete -c zypper -n __fish_zypper_is_subcommand_ms -l name -r -s n -d 'Set a descriptive name for the service'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l enable -s e -d 'Enable a disabled service'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l disable -s d -d "Disable the service (but don't remove it)"
complete -c zypper -n __fish_zypper_is_subcommand_ms -l refresh -s r -d 'Enable auto-refresh of the service'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l no-refresh -s R -d 'Disable auto-refresh of the service'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l all -s a -d 'Apply changes to all services'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l local -s l -d 'Apply changes to all local services'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l remote -s t -d 'Apply changes to all remote services'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l medium-type -s m -d 'Apply changes to services of specified type'

complete -c zypper -n __fish_zypper_is_subcommand_ms -l ar-to-enable -r -s i -d 'Add a RIS service repository to enable'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l ar-to-disable -r -s I -d 'Add a RIS service repository to disable'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l rr-to-enable -r -s j -d 'Remove a RIS service repository to enable'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l rr-to-disable -r -s J -d 'Remove a RIS service repository to disable'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l cl-to-enable -s k -d 'Clear the list of RIS repositories to enable'
complete -c zypper -n __fish_zypper_is_subcommand_ms -l cl-to-disable -s K -d 'Clear the list of RIS repositories to disable'

function __fish_zypper_is_subcommand_rs
    __fish_zypper_cmd_in_array removeservice rs
end

complete -c zypper -n __fish_zypper_is_subcommand_rs -l loose-auth -d 'Ignore user authentication data in the URI'
complete -c zypper -n __fish_zypper_is_subcommand_rs -l loose-query -d 'Ignore query string in the URI'

function __fish_zypper_is_subcommand_refs
    __fish_zypper_cmd_in_array refresh-services refs
end

complete -c zypper -n __fish_zypper_is_subcommand_refs -l force -s f -d 'Force a complete refresh'
complete -c zypper -n __fish_zypper_is_subcommand_refs -l with-repos -s r -d 'Refresh also repositories'
complete -c zypper -n __fish_zypper_is_subcommand_refs -l restore-status -s R -d 'Also restore service repositories enabled/disabled state'

function __fish_zypper_subcommand_has_solver_expert_options
    __fish_zypper_cmd_in_array install in verify ve source-install si install-new-recommends inr update up dist-upgrade dup patch
end

complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l debug-solver -d 'Create solver test case for debugging'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l force-resolution -d 'Force the solver to find a solution (even an aggressive one)'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l no-force-resolution -s R -d 'Do not force the solver to find solution, let it ask'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l solver-focus -rf -a "$__fish_zypper_solver_focus_modes" -d 'Create solver test case for debugging'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l recommends -d 'Install also recommended packages in addition to the required'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l no-recommends -d 'Do not install recommended packages, only required'

complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l allow-downgrade -l no-allow-downgrade -d 'Whether to allow downgrading installed resolvables'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l allow-name-change -l no-allow-name-change -d 'Whether to allow changing the names of installed resolvables'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l allow-arch-change -l no-allow-arch-change -d 'Whether to allow changing the architecture of installed resolvables'
complete -c zypper -n __fish_zypper_subcommand_has_solver_expert_options -l allow-vendor-change -l no-allow-vendor-change -d 'Whether to allow changing the vendor of installed resolvables'

function __fish_zypper_is_subcommand_in
    __fish_zypper_cmd_in_array install in
end

complete -c zypper -n __fish_zypper_is_subcommand_in -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'
complete -c zypper -n __fish_zypper_is_subcommand_in -l name -s n -d 'Select packages by plain name, not by capability'
complete -c zypper -n __fish_zypper_is_subcommand_in -l capability -s C -d 'Select packages by capability'
complete -c zypper -n __fish_zypper_is_subcommand_in -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_in -l from -rf -a "(__fish_zypper_print_repos -e)" -d 'Select packages from the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_in -l oldpackage -d 'Allow to replace a newer item with an older one.'
complete -c zypper -n __fish_zypper_is_subcommand_in -l allow-unsigned-rpm -d 'Silently install unsigned rpm packages given as commandline parameters'
complete -c zypper -n __fish_zypper_is_subcommand_in -l force -s f -d 'Install even if the item is installed, downgraded or changes vendor/architecture'
complete -c zypper -n __fish_zypper_is_subcommand_in -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_in -l no-confirm -s y -d 'Don\'t require user interaction'
complete -c zypper -n __fish_zypper_is_subcommand_in -l dry-run -s D -d 'Test the installation, do not actually install'
complete -c zypper -n __fish_zypper_is_subcommand_in -l replacefiles -d 'Install even if it replaces files from other installed packages'
complete -c zypper -n __fish_zypper_is_subcommand_in -l auto-agree-with-licenses -s l -d "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n __fish_zypper_is_subcommand_in -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_in -l download-only -s d -d 'Only download the packages, do not install'

function __fish_zypper_is_subcommand_rm
    __fish_zypper_cmd_in_array remove rm
end

complete -c zypper -n __fish_zypper_is_subcommand_rm -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l name -s n -d 'Select packages by plain name, not by capability'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l capability -s C -d 'Select packages by capability'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l no-confirm -s y -d 'Don\'t require user interaction'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l dry-run -s D -d 'Test the removal, do not actually remove'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l clean-deps -s u -d 'Automatically remove unneeded dependencies'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l no-clean-deps -s U -d 'No automatic removal of unneeded dependencies'

complete -c zypper -n __fish_zypper_is_subcommand_rm -l debug-solver -d 'Create solver test case for debugging'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l force-resolution -d 'Force the solver to find a solution (even an aggressive one)'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l no-force-resolution -s R -d 'Do not force the solver to find solution, let it ask'
complete -c zypper -n __fish_zypper_is_subcommand_rm -l solver-focus -rf -a "$__fish_zypper_solver_focus_modes" -d 'Create solver test case for debugging'

function __fish_zypper_is_subcommand_ve
    __fish_zypper_cmd_in_array verify ve
end

complete -c zypper -n __fish_zypper_is_subcommand_ve -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_ve -l dry-run -s D -d 'Test the repair, do not actually do anything to the system'
complete -c zypper -n __fish_zypper_is_subcommand_ve -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_ve -l download-only -s d -d 'Only download the packages, do not install'
complete -c zypper -n __fish_zypper_is_subcommand_ve -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_ve -l no-confirm -s y -d 'Don\'t require user interaction'

function __fish_zypper_is_subcommand_si
    __fish_zypper_cmd_in_array source-install si
end

complete -c zypper -n __fish_zypper_is_subcommand_si -l build-deps-only -s d -d 'Install only build dependencies of specified packages'
complete -c zypper -n __fish_zypper_is_subcommand_si -l no-build-deps -s D -d "Don't install build dependencies"
complete -c zypper -n __fish_zypper_is_subcommand_si -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Install packages only from specified repositories'
complete -c zypper -n __fish_zypper_is_subcommand_si -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_si -l download-only -s d -d 'Only download the packages, do not install'

function __fish_zypper_is_subcommand_inr
    __fish_zypper_cmd_in_array install-new-recommends inr
end

complete -c zypper -n __fish_zypper_is_subcommand_inr -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_inr -l dry-run -s D -d 'Test the installation, do not actually install'
complete -c zypper -n __fish_zypper_is_subcommand_inr -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_inr -l download-only -s d -d 'Only download the packages, do not install'
complete -c zypper -n __fish_zypper_is_subcommand_inr -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repositories'

function __fish_zypper_is_subcommand_up
    __fish_zypper_cmd_in_array update up
end

complete -c zypper -n __fish_zypper_is_subcommand_up -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'
complete -c zypper -n __fish_zypper_is_subcommand_up -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_up -l best-effort -d "Accept updates to a lower than the latest version"
complete -c zypper -n __fish_zypper_is_subcommand_up -l replacefiles -d 'Install even if it replaces files from other installed packages'
complete -c zypper -n __fish_zypper_is_subcommand_up -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_up -l no-confirm -s y -d 'Don\'t require user interaction'
complete -c zypper -n __fish_zypper_is_subcommand_up -l with-interactive -d 'Do not skip interactive updates'
complete -c zypper -n __fish_zypper_is_subcommand_up -l skip-interactive -d 'Skip interactive updates'
complete -c zypper -n __fish_zypper_is_subcommand_up -l auto-agree-with-licenses -s l -d "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n __fish_zypper_is_subcommand_up -l dry-run -s D -d 'Test the update, do not actually update'
complete -c zypper -n __fish_zypper_is_subcommand_up -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_up -l download-only -s d -d 'Only download the packages, do not install'

function __fish_zypper_is_subcommand_lu
    __fish_zypper_cmd_in_array list-updates lu
end

complete -c zypper -n __fish_zypper_is_subcommand_lu -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'
complete -c zypper -n __fish_zypper_is_subcommand_lu -l best-effort -d "Accept updates to a lower than the latest version"
complete -c zypper -n __fish_zypper_is_subcommand_lu -l all -s a -d 'List all packages with newer versions, even uninstalled'
complete -c zypper -n __fish_zypper_is_subcommand_lu -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'List only updates from the specified repository'

complete -c zypper -n __fish_zypper_is_subcommand_lu -l allow-downgrade -l no-allow-downgrade -d 'Whether to allow downgrading installed resolvables'
complete -c zypper -n __fish_zypper_is_subcommand_lu -l allow-name-change -l no-allow-name-change -d 'Whether to allow changing the names of installed resolvables'
complete -c zypper -n __fish_zypper_is_subcommand_lu -l allow-arch-change -l no-allow-arch-change -d 'Whether to allow changing the architecture of installed resolvables'
complete -c zypper -n __fish_zypper_is_subcommand_lu -l allow-vendor-change -l no-allow-vendor-change -d 'Whether to allow changing the vendor of installed resolvables'

function __fish_zypper_is_subcommand_lp
    __fish_zypper_cmd_in_array list-patches lp
end

set -g __fish_zypper_patch_category security recommended optional feature document yast
set -g __fish_zypper_patch_severity critical important moderate low unspecified

complete -c zypper -n __fish_zypper_is_subcommand_lp -l all -s a -d 'List all patches, not only the needed ones'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'List only patches from the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l category -s g -rf -a "$__fish_zypper_patch_category" -d 'Select patches with the specified category'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l severity -rf -a "$__fish_zypper_patch_severity" -d 'Select patches with the specified severity'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l date -rf -d 'Select patches issued up to, but not including, the specified date'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l bugzilla -s b -d 'Select applicable patches for the specified Bugzilla issues'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l cve -d 'Select applicable patches for the specified CVE issues'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l issues -d 'Select issues matching the specified string'
complete -c zypper -n __fish_zypper_is_subcommand_lp -l with-optional -l without-optional -d 'Whether applicable optional patches should be treated as needed or be excluded'

function __fish_zypper_is_subcommand_dup
    __fish_zypper_cmd_in_array dist-upgrade dup
end

complete -c zypper -n __fish_zypper_is_subcommand_dup -l from -rf -a "(__fish_zypper_print_repos -e)" -d 'Restrict upgrade to specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l replacefiles -d 'Install even if it replaces files from other installed packages'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l auto-agree-with-licenses -s l -d "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n __fish_zypper_is_subcommand_dup -l dry-run -s D -d 'Test the upgrade, do not actually upgrade'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l no-confirm -s y -d 'Don\'t require user interaction'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_dup -l download-only -s d -d 'Only download the packages, do not install'

function __fish_zypper_is_subcommand_pchk
    __fish_zypper_cmd_in_array patch-check pchk
end

complete -c zypper -n __fish_zypper_is_subcommand_pchk -l updatestack-only -d 'Consider only patches which affect the package management itself'
complete -c zypper -n __fish_zypper_is_subcommand_pchk -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Check for patches only in the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_pchk -l with-optional -l without-optional -d 'Whether applicable optional patches should be treated as needed or be excluded'

function __fish_zypper_is_subcommand_se
    __fish_zypper_cmd_in_array search se
end

complete -c zypper -n __fish_zypper_is_subcommand_se -l match-substrings -d 'Search for a match to partial words (default)'
complete -c zypper -n __fish_zypper_is_subcommand_se -l match-words -d 'Search for a match to whole words only'
complete -c zypper -n __fish_zypper_is_subcommand_se -l match-exact -s x -d 'Searches for an exact package name'
complete -c zypper -n __fish_zypper_is_subcommand_se -l provides -d 'Search for packages which provide the search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l requires -d 'Search for packages which require the search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l recommends -d 'Search for packages which recommend the search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l supplements -d 'Search for packages which supplements the search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l conflicts -d 'Search packages conflicting with search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l obsoletes -d 'Search for packages which obsolete the search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l suggests -d 'Search for packages which suggest the search strings'
complete -c zypper -n __fish_zypper_is_subcommand_se -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Search only for packages of the specified type'
complete -c zypper -n __fish_zypper_is_subcommand_se -l name -s n -d 'Search package name even with dependency options'
complete -c zypper -n __fish_zypper_is_subcommand_se -l file-list -s f -d 'Search for a match in the file list of packages'
complete -c zypper -n __fish_zypper_is_subcommand_se -l search-descriptions -s d -d 'Search also in package summaries and descriptions'
complete -c zypper -n __fish_zypper_is_subcommand_se -l case-sensitive -s C -d 'Perform case-sensitive search'
complete -c zypper -n __fish_zypper_is_subcommand_se -l details -s s -d 'Show each available version in each repository on a separate line'
complete -c zypper -n __fish_zypper_is_subcommand_se -l verbose -s s -d 'Like --details, with additional info (useful for search in dependencies)'
complete -c zypper -n __fish_zypper_is_subcommand_se -l sort-by-name -d 'Sort packages by name (default)'
complete -c zypper -n __fish_zypper_is_subcommand_se -l sort-by-repo -d 'Sort packages by repository'
complete -c zypper -n __fish_zypper_is_subcommand_se -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Search only in the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_se -l installed-only -s i -d 'Show only packages that are already installed'
complete -c zypper -n __fish_zypper_is_subcommand_se -l not-installed-only -s u -d 'Show only packages that are not currently installed'

function __fish_zypper_is_subcommand_if
    __fish_zypper_cmd_in_array info if
end

complete -c zypper -n __fish_zypper_is_subcommand_if -l match-substrings -s s -d 'Print information for packages partially matching name'
complete -c zypper -n __fish_zypper_is_subcommand_if -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'
complete -c zypper -n __fish_zypper_is_subcommand_if -l provides -d 'Show provides'
complete -c zypper -n __fish_zypper_is_subcommand_if -l requires -d 'Show requires and prerequires'
complete -c zypper -n __fish_zypper_is_subcommand_if -l conflicts -d 'Show conflicts'
complete -c zypper -n __fish_zypper_is_subcommand_if -l obsoletes -d 'Show obsoletes'
complete -c zypper -n __fish_zypper_is_subcommand_if -l recommends -d 'Show recommends'
complete -c zypper -n __fish_zypper_is_subcommand_if -l supplements -d 'Show supplements'
complete -c zypper -n __fish_zypper_is_subcommand_if -l suggests -d 'Show suggests'
complete -c zypper -n __fish_zypper_is_subcommand_if -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Work only with the specified repository'

function __fish_zypper_is_subcommand_pch
    __fish_zypper_cmd_in_array patches pch
end

complete -c zypper -n __fish_zypper_is_subcommand_pch -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Just another means to specify repository'

function __fish_zypper_is_subcommand_pa
    __fish_zypper_cmd_in_array packages pa
end

complete -c zypper -n __fish_zypper_is_subcommand_pa -l orphaned -d 'Show packages which are orphaned (without repository)'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l suggested -d 'Show packages which are suggested'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l recommended -d 'Show packages which are recommended'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l unneeded -d 'Show packages which are unneeded'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l sort-by-name -s N -d 'Sort the list by package name'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l sort-by-repo -s R -d 'Sort the list by repository'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Just another means to specify repository'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l installed-only -s i -d 'Show only installed packages'
complete -c zypper -n __fish_zypper_is_subcommand_pa -l not-installed-only -s u -d 'Show only packages which are not installed'

function __fish_zypper_is_subcommand_pt
    __fish_zypper_cmd_in_array patterns pt
end

complete -c zypper -n __fish_zypper_is_subcommand_pt -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Just another means to specify repository'
complete -c zypper -n __fish_zypper_is_subcommand_pt -l installed-only -s i -d 'Show only installed patterns'
complete -c zypper -n __fish_zypper_is_subcommand_pt -l not-installed-only -s u -d 'Show only patterns which are not installed'

function __fish_zypper_is_subcommand_pd
    __fish_zypper_cmd_in_array products pd
end

complete -c zypper -n __fish_zypper_is_subcommand_pd -l xmlfwd -r -d 'XML output only: Literally forward the XML tags found in a product file'
complete -c zypper -n __fish_zypper_is_subcommand_pd -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Just another means to specify repository'
complete -c zypper -n __fish_zypper_is_subcommand_pd -l installed-only -s i -d 'Show only installed products'
complete -c zypper -n __fish_zypper_is_subcommand_pd -l not-installed-only -s u -d 'Show only products which are not installed'

function __fish_zypper_is_subcommand_al
    __fish_zypper_cmd_in_array addlock al
end

complete -c zypper -n __fish_zypper_is_subcommand_al -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Restrict the lock to the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_al -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'

function __fish_zypper_is_subcommand_rl
    __fish_zypper_cmd_in_array removelock rl
end

complete -c zypper -n __fish_zypper_is_subcommand_rl -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Remove only locks with specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_rl -l type -s t -rf -a "$__fish_zypper_package_types" -d 'Type of package'

function __fish_zypper_is_subcommand_cl
    __fish_zypper_cmd_in_array cleanlocks cl
end

complete -c zypper -n __fish_zypper_is_subcommand_cl -l only-duplicates -s d -d 'Clean only duplicate locks'
complete -c zypper -n __fish_zypper_is_subcommand_cl -l only-empty -s e -d "Clean only locks which doesn't lock anything"

function __fish_zypper_is_subcommand_lloc
    __fish_zypper_cmd_in_array locales lloc
end

complete -c zypper -n __fish_zypper_is_subcommand_lloc -l packages -s p -d 'Show corresponding packages'
complete -c zypper -n __fish_zypper_is_subcommand_lloc -l all -s a -d 'List all available locales'

function __fish_zypper_is_subcommand_aloc
    __fish_zypper_cmd_in_array addlocale aloc
end

complete -c zypper -n __fish_zypper_is_subcommand_aloc -l no-packages -s n -d 'Do not install corresponding packages for given locale(s)'

function __fish_zypper_is_subcommand_rloc
    __fish_zypper_cmd_in_array removelocale rloc
end

complete -c zypper -n __fish_zypper_is_subcommand_rloc -l no-packages -s n -d 'Do not remove corresponding packages for given locale(s)'

function __fish_zypper_is_subcommand_vcmp
    __fish_zypper_cmd_in_array versioncmp vcmp
end

complete -c zypper -n __fish_zypper_is_subcommand_vcmp -l match -s m -d 'Takes missing release number as any release'

function __fish_zypper_is_subcommand_tos
    __fish_zypper_cmd_in_array targetos tos
end

complete -c zypper -n __fish_zypper_is_subcommand_tos -l label -s l -d 'Show the operating system label'

function __fish_zypper_is_subcommand_clean
    __fish_zypper_cmd_in_array clean
end

complete -c zypper -n __fish_zypper_is_subcommand_clean -l repo -s r -rf -a "(__fish_zypper_print_repos)" -d 'Clean only specified repositories'
complete -c zypper -n __fish_zypper_is_subcommand_clean -l metadata -s m -d 'Clean metadata cache'
complete -c zypper -n __fish_zypper_is_subcommand_clean -l raw-metadata -s M -d 'Clean raw metadata cache'
complete -c zypper -n __fish_zypper_is_subcommand_clean -l all -s a -d 'Clean both metadata and package caches'

function __fish_zypper_is_subcommand_patch
    __fish_zypper_cmd_in_array patch
end

complete -c zypper -n __fish_zypper_is_subcommand_patch -l updatestack-only -d 'Consider only patches which affect the package management itself'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l with-update -d 'Additionally try to update all packages not covered by patches'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l details -d 'Show the detailed installation summary'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l replacefiles -d 'Install even if it replaces files from other installed packages'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Load only the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l category -s g -rf -a "$__fish_zypper_patch_category" -d 'Select patches with the specified category'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l severity -rf -a "$__fish_zypper_patch_severity" -d 'Select patches with the specified severity'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l date -r -d 'Select patches issued up to, but not including, the specified date'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l bugzilla -s b -d 'Select applicable patches for the specified Bugzilla issues'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l cve -d 'Select applicable patches for the specified CVE issues'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l no-confirm -s y -d 'Don\'t require user interaction'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l with-optional -l without-optional -d 'Whether applicable optional patches should be treated as needed or be excluded'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l with-interactive -d 'Do not skip interactive patches'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l skip-interactive -d 'Skip interactive patches'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l auto-agree-with-licenses -s l -d "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n __fish_zypper_is_subcommand_patch -l dry-run -s D -d 'Test the update, do not actually update'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l download -rf -a "$__fish_zypper_download_modes" -d 'Set the download-install mode'
complete -c zypper -n __fish_zypper_is_subcommand_patch -l download-only -s d -d 'Only download the packages, do not install'

function __fish_zypper_is_subcommand_sourcedownload
    __fish_zypper_cmd_in_array source-download
end

complete -c zypper -n __fish_zypper_is_subcommand_sourcedownload -l directory -r -s d -d 'Download all source rpms to this directory'
complete -c zypper -n __fish_zypper_is_subcommand_sourcedownload -l delete -d 'Delete extraneous source rpms in the local directory'
complete -c zypper -n __fish_zypper_is_subcommand_sourcedownload -l no-delete -d 'Do not delete extraneous source rpms'
complete -c zypper -n __fish_zypper_is_subcommand_sourcedownload -l status -d "Show which source rpms are missing or extraneous"

function __fish_zypper_is_subcommand_download
    __fish_zypper_cmd_in_array download
end

complete -c zypper -n __fish_zypper_is_subcommand_download -l all-matches -d 'Download all versions matching the commandline arguments'
complete -c zypper -n __fish_zypper_is_subcommand_download -l from -rf -a "(__fish_zypper_print_repos -e)" -d 'Select packages from the specified repository'
complete -c zypper -n __fish_zypper_is_subcommand_download -l dry-run -s D -d 'Don\'t change anything, just report what would be done'
complete -c zypper -n __fish_zypper_is_subcommand_download -l repo -s r -rf -a "(__fish_zypper_print_repos -e)" -d 'Work only with the specified repository'

function __fish_zypper_is_subcommand_ps
    __fish_zypper_cmd_in_array ps
end

complete -c zypper -n __fish_zypper_is_subcommand_ps -l short -s s -d 'Create a short table not showing the deleted files'
complete -c zypper -n __fish_zypper_is_subcommand_ps -l print -rf -d 'For each associated system service print <format>'
complete -c zypper -n __fish_zypper_is_subcommand_ps -l debugFile -r -s d -d 'Write debug output to file'
