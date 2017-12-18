#
# Completions for the eopkg command
#

# Frequently used eopkg commands

# Eopkg subcommand
function __fish_eopkg_subcommand -a subcommand
    set -e argv[1]
    complete -f -c eopkg -n '__fish_use_subcommand' -a $subcommand $argv
end

function __fish_eopkg_subcommand_with_shortcut -a subcommand shortcut
    set -e argv[1..2]
    complete -f -c eopkg -n '__fish_use_subcommand' -a $subcommand $argv
    complete -f -c eopkg -n '__fish_use_subcommand' -a $shortcut $argv
end

# Eopkg subcommand's option
function __fish_eopkg_option
    set subcommand $argv[1]
    set -e argv[1]
    complete -f -c eopkg -n "__fish_seen_subcommand_from $subcommand" $argv
end

function __fish_eopkg_option_with_shortcut
    set subcommand $argv[1]
    set -e argv[1]
    set shortcut $argv[1]
    set -e argv[1]
    complete -f -c eopkg -n "__fish_seen_subcommand_from $subcommand" $argv
    complete -f -c eopkg -n "__fish_seen_subcommand_from $shortcut" $argv
end

# Print additional completion
function __fish_eopkg_print_components -d "Print list of components"
    eopkg list-components -N | cut -d ' ' -f 1
end

function __fish_eopkg_print_repos -d "Print list of repositories"
    eopkg list-repo -N | grep active | cut -d ' ' -f 1
end

# Setup additional completion
complete -f -c eopkg -n '__fish_seen_subcommand_from upgrade up remove rm install it info check' -s c -l component -a "(__fish_eopkg_print_components)" -d "Component"
complete -f -c eopkg -n '__fish_seen_subcommand_from upgrade up remove rm install it info check' -a "(__fish_print_packages)" -d "Package"
complete -f -c eopkg -n '__fish_seen_subcommand_from remove-repo rr enable-repo er disable-repo dr' -a "(__fish_eopkg_print_repos)" -d "Repository"

# Setup eopkg subcommand with shortcut
__fish_eopkg_subcommand_with_shortcut upgrade up -d "Upgrades eopkg packages"
__fish_eopkg_option_with_shortcut upgrade up -l security-only -d "Security related package upgrades only"
__fish_eopkg_option_with_shortcut upgrade up -s n -l dry-run -d "Do not perform any action, just show what would be done"
__fish_eopkg_option_with_shortcut upgrade up -s c -l component -f -d "Upgrade component's and recursive components' packages"

__fish_eopkg_subcommand_with_shortcut install it -d "Install eopkg packages"
__fish_eopkg_option_with_shortcut install it -l reinstall -d "Reinstall already installed packages"
__fish_eopkg_option_with_shortcut install it -s n -l dry-run -d "Do not perform any action, just show what would be done"
__fish_eopkg_option_with_shortcut install it -s c -l component -f -d "Install component's and recursive components' packages"

__fish_eopkg_subcommand_with_shortcut remove rm -d "Remove eopkg packages"
__fish_eopkg_option_with_shortcut remove rm -l purge -d "Removes everything including changed config files of the package"
__fish_eopkg_option_with_shortcut remove rm -s n -l dry-run -d "Do not perform any action, just show what would be done"
__fish_eopkg_option_with_shortcut remove rm -s c -l component -f -d "Remove component's and recursive components' packages"

__fish_eopkg_subcommand_with_shortcut remove-orphans rmo -d "Remove orphaned packages"
__fish_eopkg_option_with_shortcut remove-orphans rmo -l purge -d "Removes everything including changed config files of the package"
__fish_eopkg_option_with_shortcut remove-orphans rmo -s n -l dry-run -d "Do not perform any action, just show what would be done"

__fish_eopkg_subcommand_with_shortcut autoremove rmf -d "Remove packages and dependencies"
__fish_eopkg_option_with_shortcut autoremove rmf -l purge -d "Removes everything including changed config files of the package"
__fish_eopkg_option_with_shortcut autoremove rmf -s n -l dry-run -d "Do not perform any action, just show what would be done"

__fish_eopkg_subcommand_with_shortcut history hs -d "History of eopkg operations"
__fish_eopkg_option_with_shortcut history hs -s l -l last -x -d "Output only the last n operations"
__fish_eopkg_option_with_shortcut history hs -s t -l takeback -x -d "Takeback to the state after the given operation finished"

__fish_eopkg_subcommand_with_shortcut add-repo ar -r -d "Add a repository"
__fish_eopkg_subcommand_with_shortcut remove-repo rr -x -d "Remove repositories"
__fish_eopkg_subcommand_with_shortcut enable-repo er -x -d "Enable repositories"
__fish_eopkg_subcommand_with_shortcut disable-repo dr -x -d "Enable repositories"

__fish_eopkg_subcommand_with_shortcut list-available la -d "List available packages in the repositories"
__fish_eopkg_subcommand_with_shortcut list-components lc -d "List available components"
__fish_eopkg_subcommand_with_shortcut list-installed li -d "List of all installed packages"
__fish_eopkg_subcommand_with_shortcut list-upgrades lu -d "List packages to be upgraded"
__fish_eopkg_subcommand_with_shortcut list-repo lr -d "List repositories"
__fish_eopkg_subcommand_with_shortcut rebuild-db rdb -d "Rebuild the eopkg database"

# Setup eopkg subcommand
__fish_eopkg_subcommand check -d "Verify packages installation"
__fish_eopkg_option check -l config -d "Checks only changed config files of the packages"
__fish_eopkg_option check -s c -l component -x -d "Check installed packages under given component"

__fish_eopkg_subcommand info -d "Display package information"
__fish_eopkg_option info -l xml -d "Output in xml format"
__fish_eopkg_option info -s f -l files -d "Show a list of package files"
__fish_eopkg_option info -s s -l short -d "Do not show details"
__fish_eopkg_option info -s F -l files-path -d "Show only paths"
__fish_eopkg_option info -s c -l component -f -d "Info about given component"

__fish_eopkg_subcommand help -d "Prints help for given command"

# Setup eopkg general option
complete -c eopkg -s D -l destdir -r -d "Change the system root for eopkg commands"
complete -c eopkg -s L -l bandwidth-limit -r -d "Keep bandwidth usage under specified KB's"
complete -c eopkg -s y -l yes-all -d "Assume yes for all yes/no queries"
complete -c eopkg -s v -l verbose -d "Detailed output"
complete -c eopkg -s d -l debug -d "Show debugging information"
complete -c eopkg -s h -l help -d "Show help message and exit"
complete -c eopkg -l version -d "Show program's version number and exit"
complete -c eopkg -s u -l username -r
complete -c eopkg -s p -l password -r
