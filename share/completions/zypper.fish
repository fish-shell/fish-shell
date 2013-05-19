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
