# completion for zypper

set -g __fish_zypper_all_commands shell sh repos lr addrepo ar removerepo rr renamerepo nr modifyrepo mr refresh ref clean services ls addservice as modifyservice ms removeservice rs refresh-services refs install in remove rm verify ve source-install si install-new-recommends inr update up list-updates lu patch list-patches lp dist-upgrade dup patch-check pchk search se info if patch-info pattern-info product-info patches pch packages pa patterns pt products pd what-provides wp addlock al removelock rl locks ll cleanlocks cl versioncmp vcmp targetos tos licenses source-download
set -g __fish_zypper_pkg_commands in install rm remove info in addlock al removelock rl source-install si

function __fish_zypper_cmd_in_array
        for i in (commandline -pco)
                # -- is used to provide no options for contains
                # (if $i is equal to --optname without -- will be error)
                if contains -- $i $argv
                        return 0
                end
        end

        return 1
end

function __fish_zypper_no_subcommand
        not __fish_zypper_cmd_in_array $__fish_zypper_all_commands
end

function __fish_zypper_use_pkg
        __fish_zypper_cmd_in_array $__fish_zypper_pkg_commands
end

complete -n '__fish_zypper_use_pkg' -c zypper -a '(__fish_print_packages)' --description 'Package'

complete -n '__fish_zypper_no_subcommand' -c zypper -a 'install in' --description 'Install packages'

complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'help ?'                     --description 'Print help'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'shell sh'                   --description 'Accept multiple commands at once'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'repos lr'                   --description 'List all defined repositories'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'addrepo ar'                 --description 'Add a new repository'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'removerepo rr'              --description 'Remove specified repository'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'renamerepo nr'              --description 'Rename specified repository'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'modifyrepo mr'              --description 'Modify specified repository'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'refresh ref'                --description 'Refresh all repositories'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'clean'                      --description 'Clean local caches'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'services ls'                --description 'List all defined services'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'addservice as'              --description 'Add a new service'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'modifyservice ms'           --description 'Modify specified service'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'removeservice rs'           --description 'Remove specified service'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'refresh-services refs'      --description 'Refresh all services'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'install in'                 --description 'Install packages'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'remove rm'                  --description 'Remove packages'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'verify ve'                  --description 'Verify integrity of package dependencies'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'source-install si'          --description 'Install source packages and their build dependencies'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'install-new-recommends inr' --description 'Install newly added packages recommended by installed packages'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'update up'                  --description 'Update installed packages with newer versions'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'list-updates lu'            --description 'List available updates'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'patch'                      --description 'Install needed patches'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'list-patches lp'            --description 'List needed patches'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'dist-upgrade dup'           --description 'Perform a distribution upgrade'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'patch-check pchk'           --description 'Check for patches'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'search se'                  --description 'Search for packages matching a pattern'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'info if'                    --description 'Show full information for specified packages'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'patch-info'                 --description 'Show full information for specified patches'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'pattern-info'               --description 'Show full information for specified patterns'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'product-info'               --description 'Show full information for specified products'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'patches pch'                --description 'List all available patches'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'packages pa'                --description 'List all available packages'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'patterns pt'                --description 'List all available patterns'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'products pd'                --description 'List all available products'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'what-provides wp'           --description 'List packages providing specified capability'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'addlock al'                 --description 'Add a package lock'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'removelock rl'              --description 'Remove a package lock'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'locks ll'                   --description 'List current package locks'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'cleanlocks cl'              --description 'Remove unused locks'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'versioncmp vcmp'            --description 'Compare two version strings'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'targetos tos'               --description 'Print the target operating system ID string'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'licenses'                   --description 'Print report about licenses and EULAs of installed packages'
complete -f -n '__fish_zypper_no_subcommand' -c zypper -a 'source-download'            --description 'Download source rpms for all installed packages to a local directory'

complete -c zypper -n '__fish_zypper_no_subcommand' -l help -s h            --description 'Help'
complete -c zypper -n '__fish_zypper_no_subcommand' -l version -s V         --description 'Output the version number'
complete -c zypper -n '__fish_zypper_no_subcommand' -l config -s c          --description 'Use specified config file instead of the default'
complete -c zypper -n '__fish_zypper_no_subcommand' -l quiet -s q           --description 'Suppress normal output, print only error messages'
complete -c zypper -n '__fish_zypper_no_subcommand' -l verbose -s v         --description 'Increase verbosity'
complete -c zypper -n '__fish_zypper_no_subcommand' -l no-abbrev -s A       --description 'Do not abbreviate text in tables'
complete -c zypper -n '__fish_zypper_no_subcommand' -l table-style -s s     --description 'Table style (integer)'
complete -c zypper -n '__fish_zypper_no_subcommand' -l rug-compatible -s r  --description 'Turn on rug compatibility'
complete -c zypper -n '__fish_zypper_no_subcommand' -l non-interactive -s n --description 'Do not ask anything, use default answers automatically'
complete -c zypper -n '__fish_zypper_no_subcommand' -l xmlout -s x          --description 'Switch to XML output'
complete -c zypper -n '__fish_zypper_no_subcommand' -l ignore-unknown -s i  --description 'Ignore unknown packages'
complete -c zypper -n '__fish_zypper_no_subcommand' -l plus-repo -s p       --description 'Use an additional repository'
complete -c zypper -n '__fish_zypper_no_subcommand' -l reposd-dir -s D      --description 'Use alternative repository definition file directory'
complete -c zypper -n '__fish_zypper_no_subcommand' -l cache-dir -s C       --description 'Use alternative directory for all caches'
complete -c zypper -n '__fish_zypper_no_subcommand' -l root -s R            --description 'Operate on a different root directory'

complete -c zypper -n '__fish_zypper_no_subcommand' -l non-interactive-include-reboot-patches --description 'Do not treat patches as interactive, which have the rebootSuggested-flag set'
complete -c zypper -n '__fish_zypper_no_subcommand' -l no-gpg-checks                          --description 'Ignore GPG check failures and continue'
complete -c zypper -n '__fish_zypper_no_subcommand' -l gpg-auto-import-keys                   --description 'Automatically trust and import new repository signing keys'
complete -c zypper -n '__fish_zypper_no_subcommand' -l disable-repositories                   --description 'Do not read meta-data from repositories'
complete -c zypper -n '__fish_zypper_no_subcommand' -l no-refresh                             --description 'Do not refresh the repositories'
complete -c zypper -n '__fish_zypper_no_subcommand' -l no-cd                                  --description 'Ignore CD/DVD repositories'
complete -c zypper -n '__fish_zypper_no_subcommand' -l no-remote                              --description 'Ignore remote repositories'
complete -c zypper -n '__fish_zypper_no_subcommand' -l disable-system-resolvables             --description 'Do not read installed packages'
complete -c zypper -n '__fish_zypper_no_subcommand' -l promptids                              --description 'Output a list of zypper user prompts'
complete -c zypper -n '__fish_zypper_no_subcommand' -l userdata                               --description 'User defined transaction id used in history and plugins'
complete -c zypper -n '__fish_zypper_no_subcommand' -l raw-cache-dir                          --description 'Use alternative raw meta-data cache directory'
complete -c zypper -n '__fish_zypper_no_subcommand' -l solv-cache-dir                         --description 'Use alternative solv file cache directory'
complete -c zypper -n '__fish_zypper_no_subcommand' -l pkg-cache-dir                          --description 'Use alternative package cache directory'

function __fish_zypper_is_subcommand_lr
        __fish_zypper_cmd_in_array repos lr
end

complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l export -s e           --description 'Export all defined repositories as a single local .repo file'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l alias -s a            --description 'Show also repository alias'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l name -s n             --description 'Show also repository name'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l uri -s u              --description 'Show also base URI of repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l priority -s p         --description 'Show also repository priority'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l refresh -s r          --description 'Show also the autorefresh flag'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l details -s d          --description 'Show more information like URI, priority, type'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l service -s s          --description 'Show also alias of parent service'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l sort-by-uri -s U      --description 'Sort the list by URI'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l sort-by-priority -s P --description 'Sort the list by repository priority'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l sort-by-alias -s A    --description 'Sort the list by alias'
complete -c zypper -n '__fish_zypper_is_subcommand_lr' -l sort-by-name -s N     --description 'Sort the list by name'

function __fish_zypper_is_subcommand_ar
        __fish_zypper_cmd_in_array addrepo ar
end

complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l repo -s r             --description 'Just another means to specify a .repo file to read'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l type -s t             --description 'Type of repository (yast2, rpm-md, plaindir)'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l disable -s d          --description 'Add the repository as disabled'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l check -s c            --description 'Probe URI'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l no-check -s C         --description "Don't probe URI, probe later during refresh"
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l name -s n             --description 'Specify descriptive name for the repository'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l keep-packages -s k    --description 'Enable RPM files caching'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l no-keep-packages -s K --description 'Disable RPM files caching'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l gpgcheck -s g         --description 'Enable GPG check for this repository'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l no-gpgcheck -s G      --description 'Disable GPG check for this repository'
complete -c zypper -n '__fish_zypper_is_subcommand_ar' -l refresh -s f          --description 'Enable autorefresh of the repository'

function __fish_zypper_is_subcommand_rr
        __fish_zypper_cmd_in_array removerepo rr
end

complete -c zypper -n '__fish_zypper_is_subcommand_rr' -l loose-auth  --description 'Ignore user authentication data in the URI'
complete -c zypper -n '__fish_zypper_is_subcommand_rr' -l loose-query --description 'Ignore query string in the URI'

function __fish_zypper_is_subcommand_mr
        __fish_zypper_cmd_in_array modifyrepo mr
end

complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l disable -s d          --description "Disable the repository (but don't remove it)"
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l enable -s e           --description 'Enable a disabled repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l refresh -s r          --description 'Enable auto-refresh of the repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l no-refresh -s R       --description 'Disable auto-refresh of the repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l name -s n             --description 'Set a descriptive name for the repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l priority -s p         --description 'Set priority of the repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l keep-packages -s k    --description 'Enable RPM files caching'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l no-keep-packages -s K --description 'Disable RPM files caching'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l gpgcheck -s g         --description 'Enable GPG check for this repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l no-gpgcheck -s G      --description 'Disable GPG check for this repository'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l all -s a              --description 'Apply changes to all repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l local -s l            --description 'Apply changes to all local repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l remote -s t           --description 'Apply changes to all remote repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_mr' -l medium-type -s m      --description 'Apply changes to repositories of specified type'

function __fish_zypper_is_subcommand_ref
        __fish_zypper_cmd_in_array refresh ref
end

complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l force -s f          --description 'Force a complete refresh'
complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l force-build -s b    --description 'Force rebuild of the database'
complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l force-download -s d --description 'Force download of raw metadata'
complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l build-only -s B     --description "Only build the database, don't download metadata"
complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l download-only -s D  --description "Only download raw metadata, don't build the database"
complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l repo -s r           --description 'Refresh only specified repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_ref' -l services -s s       --description 'Refresh also services before refreshing repos'

function __fish_zypper_is_subcommand_ls
        __fish_zypper_cmd_in_array services ls
end

complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l uri -s u              --description 'Show also base URI of repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l priority -s p         --description 'Show also repository priority'
complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l details -s d          --description 'Show more information like URI, priority, type'
complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l with-repos -s r       --description 'Show also repositories belonging to the services'
complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l sort-by-priority -s P --description 'Sort the list by repository priority'
complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l sort-by-uri -s U      --description 'Sort the list by URI'
complete -c zypper -n '__fish_zypper_is_subcommand_ls' -l sort-by-name -s N     --description 'Sort the list by name'

function __fish_zypper_is_subcommand_as
        __fish_zypper_cmd_in_array addservice as
end

complete -c zypper -n '__fish_zypper_is_subcommand_as' -l type -s t    --description 'Type of the service (ris)'
complete -c zypper -n '__fish_zypper_is_subcommand_as' -l disable -s d --description 'Add the service as disabled'
complete -c zypper -n '__fish_zypper_is_subcommand_as' -l name -s n    --description 'Specify descriptive name for the service'

function __fish_zypper_is_subcommand_ms
        __fish_zypper_cmd_in_array modifyservice ms
end

complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l disable -s d       --description "Disable the service (but don't remove it)"
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l enable -s e        --description 'Enable a disabled service'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l refresh -s r       --description 'Enable auto-refresh of the service'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l no-refresh -s R    --description 'Disable auto-refresh of the service'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l name -s n          --description 'Set a descriptive name for the service'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l ar-to-enable -s i  --description 'Add a RIS service repository to enable'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l ar-to-disable -s I --description 'Add a RIS service repository to disable'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l rr-to-enable -s j  --description 'Remove a RIS service repository to enable'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l rr-to-disable -s J --description 'Remove a RIS service repository to disable'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l cl-to-enable -s k  --description 'Clear the list of RIS repositories to enable'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l cl-to-disable -s K --description 'Clear the list of RIS repositories to disable'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l all -s a           --description 'Apply changes to all services'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l local -s l         --description 'Apply changes to all local services'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l remote -s t        --description 'Apply changes to all remote services'
complete -c zypper -n '__fish_zypper_is_subcommand_ms' -l medium-type -s m   --description 'Apply changes to services of specified type'

function __fish_zypper_is_subcommand_rs
        __fish_zypper_cmd_in_array removeservice rs
end

complete -c zypper -n '__fish_zypper_is_subcommand_rs' -l loose-auth  --description 'Ignore user authentication data in the URI'
complete -c zypper -n '__fish_zypper_is_subcommand_rs' -l loose-query --description 'Ignore query string in the URI'

function __fish_zypper_is_subcommand_refs
        __fish_zypper_cmd_in_array refresh-services refs
end

complete -c zypper -n '__fish_zypper_is_subcommand_refs' -l with-repos -s r --description 'Refresh also repositories'

function __fish_zypper_is_subcommand_in
        __fish_zypper_cmd_in_array install in
end

complete -c zypper -n '__fish_zypper_is_subcommand_in' -l repo -s r                     --description 'Load only the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l type -s t                     --description 'Type of package (package, patch, pattern, product, srcpackage).  Default: package'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l name -s n                     --description 'Select packages by plain name, not by capability'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l capability -s C               --description 'Select packages by capability'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l force -s f                    --description 'Install even if the item is already installed (reinstall), downgraded or changes vendor or architecture'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l auto-agree-with-licenses -s l --description "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l no-force-resolution -s R      --description 'Do not force the solver to find solution, let it ask'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l dry-run -s D                  --description 'Test the installation, do not actually install'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l download-only -s d            --description 'Only download the packages, do not install'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l oldpackage                    --description 'Allow to replace a newer item with an older one.  Handy if you are doing a rollback. Unlike --force it will not enforce a reinstall'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l debug-solver                  --description 'Create solver test case for debugging'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l no-recommends                 --description 'Do not install recommended packages, only required'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l recommends                    --description 'Install also recommended packages in addition to the required'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l force-resolution              --description 'Force the solver to find a solution (even an aggressive one)'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l from                          --description 'Select packages from the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_in' -l download                      --description 'Set the download-install mode. Available modes: only, in-advance, in-heaps, as-needed'

function __fish_zypper_is_subcommand_rm
        __fish_zypper_cmd_in_array remove rm
end

complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l repo -s r                --description 'Load only the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l type -s t                --description 'Type of package (package, patch, pattern, product). Default: package'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l name -s n                --description 'Select packages by plain name, not by capability'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l capability -s C          --description 'Select packages by capability'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l no-force-resolution -s R --description 'Do not force the solver to find solution, let it ask'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l clean-deps -s u          --description 'Automatically remove unneeded dependencies'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l no-clean-deps -s U       --description 'No automatic removal of unneeded dependencies'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l dry-run -s D             --description 'Test the removal, do not actually remove'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l debug-solver             --description 'Create solver test case for debugging'
complete -c zypper -n '__fish_zypper_is_subcommand_rm' -l force-resolution         --description 'Force the solver to find a solution (even an aggressive one)'

function __fish_zypper_is_subcommand_ve
        __fish_zypper_cmd_in_array verify ve
end

complete -c zypper -n '__fish_zypper_is_subcommand_ve' -l repo -s r          --description 'Load only the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_ve' -l dry-run -s D       --description 'Test the repair, do not actually do anything to the system'
complete -c zypper -n '__fish_zypper_is_subcommand_ve' -l download-only -s d --description 'Only download the packages, do not install'
complete -c zypper -n '__fish_zypper_is_subcommand_ve' -l no-recommends      --description 'Do not install recommended packages, only required'
complete -c zypper -n '__fish_zypper_is_subcommand_ve' -l download           --description 'Set the download-install mode. Available modes: only, in-advance, in-heaps, as-needed'
complete -c zypper -n '__fish_zypper_is_subcommand_ve' -l recommends         --description 'Install also recommended packages in addition to the required'

function __fish_zypper_is_subcommand_si
        __fish_zypper_cmd_in_array source-install si
end

complete -c zypper -n '__fish_zypper_is_subcommand_si' -l build-deps-only -s d --description 'Install only build dependencies of specified packages'
complete -c zypper -n '__fish_zypper_is_subcommand_si' -l no-build-deps -s D   --description "Don't install build dependencies"
complete -c zypper -n '__fish_zypper_is_subcommand_si' -l repo -s r            --description 'Install packages only from specified repositories'

function __fish_zypper_is_subcommand_inr
        __fish_zypper_cmd_in_array install-new-recommends inr
end

complete -c zypper -n '__fish_zypper_is_subcommand_inr' -l repo -s r          --description 'Load only the specified repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_inr' -l dry-run -s D       --description 'Test the installation, do not actually install'
complete -c zypper -n '__fish_zypper_is_subcommand_inr' -l download-only -s d --description 'Only download the packages, do not install'
complete -c zypper -n '__fish_zypper_is_subcommand_inr' -l download           --description 'Set the download-install mode. Available modes: only, in-advance, in-heaps, as-needed'
complete -c zypper -n '__fish_zypper_is_subcommand_inr' -l debug-solver       --description 'Create solver test case for debugging'

function __fish_zypper_is_subcommand_up
        __fish_zypper_cmd_in_array update up
end

complete -c zypper -n '__fish_zypper_is_subcommand_up' -l type -s t                     --description 'Type of package (package, patch, pattern, product, srcpackage). Default: package'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l repo -s r                     --description 'Load only the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l auto-agree-with-licenses -s l --description "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l no-force-resolution -s R      --description 'Do not force the solver to find solution, let it ask'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l dry-run -s D                  --description 'Test the update, do not actually update'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l download-only -s d            --description 'Only download the packages, do not install'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l skip-interactive              --description 'Skip interactive updates'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l download                      --description 'Set the download-install mode. Available modes: only, in-advance, in-heaps, as-needed'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l force-resolution              --description 'Force the solver to find a solution (even an aggressive one)'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l best-effort                   --description "Do a 'best effort' approach to update. Updates to a lower than the latest version are also acceptable"
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l debug-solver                  --description 'Create solver test case for debugging'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l no-recommends                 --description 'Do not install recommended packages, only required'
complete -c zypper -n '__fish_zypper_is_subcommand_up' -l recommends                    --description 'Install also recommended packages in addition to the required'

function __fish_zypper_is_subcommand_lu
        __fish_zypper_cmd_in_array list-updates lu
end

complete -c zypper -n '__fish_zypper_is_subcommand_lu' -l type -s t   --description 'Type of package (package, patch, pattern, product). Default: package'
complete -c zypper -n '__fish_zypper_is_subcommand_lu' -l repo -s r   --description 'List only updates from the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_lu' -l all -s a    --description 'List all packages for which newer versions are available, regardless whether they are installable or not'
complete -c zypper -n '__fish_zypper_is_subcommand_lu' -l best-effort --description "Do a 'best effort' approach to update. Updates to a lower than the latest version are also acceptable"

function __fish_zypper_is_subcommand_lp
        __fish_zypper_cmd_in_array list-patches lp
end

complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l bugzilla -s b --description 'List needed patches for Bugzilla issues'
complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l category -s g --description 'List all patches in this category'
complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l all -s a      --description 'List all patches, not only the needed ones'
complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l repo -s r     --description 'List only patches from the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l date          --description 'List patches issued up to the specified date <YYYY-MM-DD>'
complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l cve           --description 'List needed patches for CVE issues'
complete -c zypper -n '__fish_zypper_is_subcommand_lp' -l issuesstring  --description 'Look for issues matching the specified string'

function __fish_zypper_is_subcommand_dup
        __fish_zypper_cmd_in_array dist-upgrade dup
end

complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l repo -s r                     --description 'Load only the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l auto-agree-with-licenses -s l --description "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l dry-run -s D                  --description 'Test the upgrade, do not actually upgrade'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l download-only -s d            --description 'Only download the packages, do not install'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l debug-solver                  --description 'Create solver test case for debugging'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l no-recommends                 --description 'Do not install recommended packages, only required'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l recommends                    --description 'Install also recommended packages in addition to the required'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l from                          --description 'Restrict upgrade to specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_dup' -l download                      --description 'Set the download-install mode. Available modes: only, in-advance, in-heaps, as-needed'

function __fish_zypper_is_subcommand_pchk
        __fish_zypper_cmd_in_array patch-check pchk
end

complete -c zypper -n '__fish_zypper_is_subcommand_pchk' -l repo -s r --description 'Check for patches only in the specified repository'

function __fish_zypper_is_subcommand_se
        __fish_zypper_cmd_in_array search se
end

complete -c zypper -n '__fish_zypper_is_subcommand_se' -l search-descriptions -s d --description 'Search also in package summaries and descriptions'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l case-sensitive -s C      --description 'Perform case-sensitive search'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l installed-only -s i      --description 'Show only packages that are already installed'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l uninstalled-only -s u    --description 'Show only packages that are not currently installed'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l type -s t                --description 'Search only for packages of the specified type'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l repo -s r                --description 'Search only in the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l details -s s             --description 'Show each available version in each repository on a separate line'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l sort-by-name             --description 'Sort packages by name (default)'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l sort-by-repo             --description 'Sort packages by repository'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l match-all                --description 'Search for a match with all search strings (default)'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l match-any                --description 'Search for a match with any of the search strings'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l match-substrings         --description 'Search for a match to partial words (default)'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l match-words              --description 'Search for a match to whole words only'
complete -c zypper -n '__fish_zypper_is_subcommand_se' -l match-exact              --description 'Searches for an exact package name'

function __fish_zypper_is_subcommand_if
        __fish_zypper_cmd_in_array info if
end

complete -c zypper -n '__fish_zypper_is_subcommand_if' -l repo -s r  --description 'Work only with the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_if' -l type -s t  --description 'Type of package (package, patch, pattern, product). Default: package'
complete -c zypper -n '__fish_zypper_is_subcommand_if' -l requires   --description 'Show also requires and prerequires'
complete -c zypper -n '__fish_zypper_is_subcommand_if' -l recommends --description 'Show also recommends'

function __fish_zypper_is_subcommand_pch
        __fish_zypper_cmd_in_array patches pch
end

complete -c zypper -n '__fish_zypper_is_subcommand_pch' -l repo -s r --description 'Just another means to specify repository'

function __fish_zypper_is_subcommand_pa
        __fish_zypper_cmd_in_array packages pa
end

complete -c zypper -n '__fish_zypper_is_subcommand_pa' -l repo -s r             --description 'Just another means to specify repository'
complete -c zypper -n '__fish_zypper_is_subcommand_pa' -l installed-only -s i   --description 'Show only installed packages'
complete -c zypper -n '__fish_zypper_is_subcommand_pa' -l uninstalled-only -s u --description 'Show only packages which are not installed'
complete -c zypper -n '__fish_zypper_is_subcommand_pa' -l sort-by-name -s N     --description 'Sort the list by package name'
complete -c zypper -n '__fish_zypper_is_subcommand_pa' -l sort-by-repo -s R     --description 'Sort the list by repository'

function __fish_zypper_is_subcommand_pt
        __fish_zypper_cmd_in_array patterns pt
end

complete -c zypper -n '__fish_zypper_is_subcommand_pt' -l repo -s r             --description 'Just another means to specify repository'
complete -c zypper -n '__fish_zypper_is_subcommand_pt' -l installed-only -s i   --description 'Show only installed patterns'
complete -c zypper -n '__fish_zypper_is_subcommand_pt' -l uninstalled-only -s u --description 'Show only patterns which are not installed'

function __fish_zypper_is_subcommand_pd
        __fish_zypper_cmd_in_array products pd
end

complete -c zypper -n '__fish_zypper_is_subcommand_pd' -l repo -s r             --description 'Just another means to specify repository'
complete -c zypper -n '__fish_zypper_is_subcommand_pd' -l installed-only -s i   --description 'Show only installed products'
complete -c zypper -n '__fish_zypper_is_subcommand_pd' -l uninstalled-only -s u --description 'Show only products which are not installed'

function __fish_zypper_is_subcommand_al
        __fish_zypper_cmd_in_array addlock al
end

complete -c zypper -n '__fish_zypper_is_subcommand_al' -l repo -s r --description 'Restrict the lock to the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_al' -l type -s t --description 'Type of package (package, patch, pattern, product).  Default: package'

function __fish_zypper_is_subcommand_rl
        __fish_zypper_cmd_in_array removelock rl
end

complete -c zypper -n '__fish_zypper_is_subcommand_rl' -l repo -s r --description 'Remove only locks with specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_rl' -l type -s t --description 'Type of package (package, patch, pattern, product).  Default: package'

function __fish_zypper_is_subcommand_cl
        __fish_zypper_cmd_in_array cleanlocks cl
end

complete -c zypper -n '__fish_zypper_is_subcommand_cl' -l only-duplicates -s d --description 'Clean only duplicate locks'
complete -c zypper -n '__fish_zypper_is_subcommand_cl' -l only-empty -s e      --description "Clean only locks which doesn't lock anything"

function __fish_zypper_is_subcommand_vcmp
        __fish_zypper_cmd_in_array versioncmp vcmp
end

complete -c zypper -n '__fish_zypper_is_subcommand_vcmp' -l match -s m --description 'Takes missing release number as any release'

function __fish_zypper_is_subcommand_tos
        __fish_zypper_cmd_in_array targetos tos
end

complete -c zypper -n '__fish_zypper_is_subcommand_tos' -l label -s l --description 'Show the operating system label'

function __fish_zypper_is_subcommand_clean
        __fish_zypper_cmd_in_array clean
end

complete -c zypper -n '__fish_zypper_is_subcommand_clean' -l repo -s r         --description 'Clean only specified repositories'
complete -c zypper -n '__fish_zypper_is_subcommand_clean' -l metadata -s m     --description 'Clean metadata cache'
complete -c zypper -n '__fish_zypper_is_subcommand_clean' -l raw-metadata -s M --description 'Clean raw metadata cache'
complete -c zypper -n '__fish_zypper_is_subcommand_clean' -l all -s a          --description 'Clean both metadata and package caches'

function __fish_zypper_is_subcommand_patch
        __fish_zypper_cmd_in_array patch
end

complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l auto-agree-with-licenses -s l --description "Automatically say 'yes' to third party license confirmation prompt"
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l bugzilla -s b                 --description 'Install patch fixing the specified bugzilla issue'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l category -s g                 --description 'Install all patches in this category'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l repo -s r                     --description 'Load only the specified repository'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l dry-run -s D                  --description 'Test the update, do not actually update'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l download-only -s d            --description 'Only download the packages, do not install'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l cve                           --description 'Install patch fixing the specified CVE issue'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l date                          --description 'Install patches issued until the specified date <YYYY-MM-DD>'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l debug-solver                  --description 'Create solver test case for debugging'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l no-recommends                 --description 'Do not install recommended packages, only required'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l recommends                    --description 'Install also recommended packages in addition to the required'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l download                      --description 'Set the download-install mode. Available modes: only, in-advance, in-heaps, as-needed'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l skip-interactive              --description 'Skip interactive patches'
complete -c zypper -n '__fish_zypper_is_subcommand_patch' -l with-interactive              --description 'Do not skip interactive patches'

function __fish_zypper_is_subcommand_sourcedownload
        __fish_zypper_cmd_in_array source-download
end

complete -c zypper -n '__fish_zypper_is_subcommand_sourcedownload' -l directory -s d --description 'Download all source rpms to this directory. Default: /var/cache/zypper/source-download'
complete -c zypper -n '__fish_zypper_is_subcommand_sourcedownload' -l delete         --description 'Delete extraneous source rpms in the local directory'
complete -c zypper -n '__fish_zypper_is_subcommand_sourcedownload' -l no-delete      --description 'Do not delete extraneous source rpms'
complete -c zypper -n '__fish_zypper_is_subcommand_sourcedownload' -l status         --description "Don't download any source rpms, but show which source rpms are missing or extraneous"
