#
# Completions for eopkg, Solus' package manager
#

function __fish_eopkg_subcommand -a subcommand
    set -e argv[1]
    complete -f -c eopkg -n __fish_use_subcommand -a $subcommand $argv
end

function __fish_eopkg_subcommand_with_shortcut -a subcommand shortcut
    set -e argv[1..2]
    complete -f -c eopkg -n __fish_use_subcommand -a $subcommand $argv
    complete -f -c eopkg -n __fish_use_subcommand -a $shortcut $argv
end

function __fish_eopkg_option -a subcommand
    set -e argv[1]
    complete -f -c eopkg -n "__fish_seen_subcommand_from $subcommand" $argv
end

function __fish_eopkg_option_with_shortcut -a subcommand shortcut
    set -e argv[1..2]
    complete -f -c eopkg -n "__fish_seen_subcommand_from $subcommand" $argv
    complete -f -c eopkg -n "__fish_seen_subcommand_from $shortcut" $argv
end

function __fish_eopkg_print_components -d "Print list of components"
    eopkg list-components -N | string replace -r ' .*' ''
end

function __fish_eopkg_print_repos -d "Print list of repositories"
    eopkg list-repo -N | string match -e active | string replace -r ' .*' ''
end

complete -f -c eopkg -n '__fish_seen_subcommand_from remove-repo rr enable-repo er disable-repo dr list-available la' -a "(__fish_eopkg_print_repos)" -d Repository
complete -f -c eopkg -n '__fish_seen_subcommand_from upgrade up install it info' -a "(__fish_print_eopkg_packages)" -d "Available Package"
complete -f -c eopkg -n '__fish_seen_subcommand_from remove rm autoremove rmf check' -a "(__fish_print_eopkg_packages --installed)" -d "Installed package"
complete -f -c eopkg -n '__fish_seen_subcommand_from upgrade up remove rm install it info check list-available la list-installed li list-upgrades lu' -s c -l component -a "(__fish_eopkg_print_components)" -d Component

## Upgrade
__fish_eopkg_subcommand_with_shortcut upgrade up -d "Upgrades packages"
__fish_eopkg_option_with_shortcut upgrade up -l security-only -d "Security related upgrades only"
__fish_eopkg_option_with_shortcut upgrade up -s c -l component -x -d "Upgrade component's and recursive components' packages"
__fish_eopkg_option_with_shortcut upgrade up -s n -l dry-run -d "Show what would be done"

## Install
__fish_eopkg_subcommand_with_shortcut install it -d "Install packages"
__fish_eopkg_option_with_shortcut install it -l reinstall -d "Reinstall already installed packages"
__fish_eopkg_option_with_shortcut install it -s c -l component -x -d "Install component's and recursive components' packages"
__fish_eopkg_option_with_shortcut install it -s n -l dry-run -d "Show what would be done"

## Remove
__fish_eopkg_subcommand_with_shortcut remove rm -d "Remove packages"
__fish_eopkg_option_with_shortcut remove rm -l purge -d "Removes everything including changed config files of the package"
__fish_eopkg_option_with_shortcut remove rm -s c -l component -x -d "Remove component's and recursive components' packages"
__fish_eopkg_option_with_shortcut remove rm -s n -l dry-run -d "Show what would be done"

## Remove Orphans
__fish_eopkg_subcommand_with_shortcut remove-orphans rmo -d "Remove orphaned packages"
__fish_eopkg_option_with_shortcut remove-orphans rmo -l purge -d "Remove everything including changed config files of the package"
__fish_eopkg_option_with_shortcut remove-orphans rmo -s n -l dry-run -d "Show what would be done"

## Autoremove
__fish_eopkg_subcommand_with_shortcut autoremove rmf -d "Remove packages and dependencies"
__fish_eopkg_option_with_shortcut autoremove rmf -l purge -d "Remove everything including changed config files of the package"
__fish_eopkg_option_with_shortcut autoremove rmf -s n -l dry-run -d "Show what would be done"

## History
__fish_eopkg_subcommand_with_shortcut history hs -d "History of operations"
__fish_eopkg_option_with_shortcut history hs -s l -l last -x -d "Output only the last n operations"
__fish_eopkg_option_with_shortcut history hs -s t -l takeback -x -d "Takeback to the state after the given operation"

## Search
__fish_eopkg_subcommand_with_shortcut search sr -d "Search packages"
__fish_eopkg_option_with_shortcut search sr -l description -d "Search in package name"
__fish_eopkg_option_with_shortcut search sr -l name -d "Search in package name"
__fish_eopkg_option_with_shortcut search sr -l summary -d "Search in package name"
__fish_eopkg_option_with_shortcut search sr -s i -l installdb -d "Search only installed package"
__fish_eopkg_option_with_shortcut search sr -s r -l repository -x -d "Name of the source or package repository"
complete -f -c eopkg -n '__fish_seen_subcommand_from search sr' -s r -l repository -a "(__fish_eopkg_print_repos)" -d Repository

## Search File
__fish_eopkg_subcommand_with_shortcut search-file sf -d "Search for a file in installed packages"
__fish_eopkg_option_with_shortcut search-file sf -s l -l long -d "Show in long format"
__fish_eopkg_option_with_shortcut search-file sf -s q -l quiet -d "Show only package name"

## Repo Management
__fish_eopkg_subcommand_with_shortcut add-repo ar -r -d "Add a repository"
__fish_eopkg_subcommand_with_shortcut disable-repo dr -x -d "Disable repositories"
__fish_eopkg_subcommand_with_shortcut enable-repo er -x -d "Enable repositories"
__fish_eopkg_subcommand_with_shortcut remove-repo rr -x -d "Remove repositories"

## List Available
__fish_eopkg_subcommand_with_shortcut list-available la -d "List available packages in the repositories"
__fish_eopkg_option_with_shortcut list-available la -s c -l component -x -d "List available package under given component"
__fish_eopkg_option_with_shortcut list-available la -s l -l long -d "Show in long format"
__fish_eopkg_option_with_shortcut list-available la -s U -l uninstalled -d "Show only uninstalled packages"

## List Components
__fish_eopkg_subcommand_with_shortcut list-components lc -d "List available components"
__fish_eopkg_option_with_shortcut list-components lc -s l -l long -d "Show in long format"

## List Installed
__fish_eopkg_subcommand_with_shortcut list-installed li -d "List of all installed packages"
__fish_eopkg_option_with_shortcut list-installed li -s a -l automatic -d "Show automatically installed packages and the parent dependency"
__fish_eopkg_option_with_shortcut list-installed li -s c -l component -x -d "List installed packages under given component"
__fish_eopkg_option_with_shortcut list-installed li -s i -l install-info -d "Show detailed install info"
__fish_eopkg_option_with_shortcut list-installed li -s l -l long -d "Show in long format"

## List Upgrades
__fish_eopkg_subcommand_with_shortcut list-upgrades lu -d "List packages to be upgraded"
__fish_eopkg_option_with_shortcut list-upgrades lu -s c -l component -x -d "List upgradable packages under given component"
__fish_eopkg_option_with_shortcut list-upgrades lu -s i -l install-info -d "Show detailed install info"
__fish_eopkg_option_with_shortcut list-upgrades lu -s l -l long -d "Show in long format"

## Other
__fish_eopkg_subcommand_with_shortcut list-repo lr -d "List repositories"
__fish_eopkg_subcommand_with_shortcut rebuild-db rdb -d "Rebuild the eopkg database"

## Check
__fish_eopkg_subcommand check -d "Verify packages installation"
__fish_eopkg_option check -l config -d "Checks only changed config files of the packages"
__fish_eopkg_option check -s c -l component -x -d "Check installed packages under given component"

## Info
__fish_eopkg_subcommand info -d "Display package information"
__fish_eopkg_option info -l xml -d "Output in xml format"
__fish_eopkg_option info -s c -l component -f -d "Info about given component"
__fish_eopkg_option info -s f -l files -d "Show a list of package files"
__fish_eopkg_option info -s F -l files-path -d "Show only paths"
__fish_eopkg_option info -s s -l short -d "Do not show details"

## Help
__fish_eopkg_subcommand help -d "Prints help for given command"

# General options
complete -c eopkg -l version -d "Show program's version number and exit"
complete -c eopkg -s d -l debug -d "Show debugging information"
complete -c eopkg -s D -l destdir -r -d "Change the system root for eopkg commands"
complete -c eopkg -s h -l help -d "Show help message and exit"
complete -c eopkg -s L -l bandwidth-limit -x -d "Keep bandwidth usage under specified KB's"
complete -c eopkg -s p -l password -x
complete -c eopkg -s u -l username -x
complete -c eopkg -s v -l verbose -d "Detailed output"
complete -c eopkg -s y -l yes-all -d "Assume yes for all yes/no queries"
