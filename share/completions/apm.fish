# Returns exit code of 0 if apm hasn't received a command yet, e.g. `config`
function __fish_apm_needs_command
    set cmd (commandline -opc)
    if test (count $cmd) -eq 1
        return 0
    else if test (count $cmd) -gt 1
        for c in $cmd[2..-1]
            switch $c
                # Exclude options that are allowed to appear before command
                case '--color' '--no-color' 'help'
                    continue
                case '*'
                    return 1
            end
        end
        return 0
    end
    return 1
end

# Returns exit code of 0 if any command (argv[1..-1]) appears once, ignores flags.
function __fish_apm_using_command
    set commands $argv
    set cmd (commandline -opc)
    if test (count $cmd) -gt 1
        set -l command_seen_once 1
        for c in $cmd[2..-1]
            switch $c
                case '-*'
                    continue
                case $commands
                    # If the command is seen more than once then return 1
                    if test $command_seen_once -eq 1
                        set command_seen_once 0
                    else
                        return 1
                    end
                case '*'
                    if test $command_seen_once -eq 0
                        return 0
                    else
                        return 1
                    end
            end
        end
        return $command_seen_once
    end
    return 1
end

# Check if `commandline` contains a set of subcommands
function __fish_apm_includes_subcommand
    set cmd (commandline -opc)
    for subcommand in $argv
        if contains $subcommand $cmd
            return 0
        end
    end
    return 1
end

# Lists all apm config items
function __fish_apm_config_items
    apm config list | string match -r -v "^\s*;|^\s*\$" | string replace "\w*=\w*" \t
end

# Lists all installed atom packages
function __fish_apm_list_packages
    apm list -b
end

if apm -h ^| string match -q "*Atom Package Manager*"
    # Completions for Atom Package Manager

    ##################
    # global options #
    ##################

    # help
    complete -f -c apm -n '__fish_apm_needs_command' -s h -l help -d "Display help"
    complete -f -c apm -n '__fish_apm_needs_command' -a help -d "Display help"
    complete -f -c apm -n '__fish_apm_using_command clean config dedupe deinstall delete dev develop docs erase featured home init install link linked links list ln lns login ls open outdated publish rebuild rebuild-module-cache remove rm search show star starred stars test uninstall unlink unpublish unstar update upgrade view' -s h -l help -d 'Display help'

    # version
    complete -f -c apm -n '__fish_apm_needs_command' -s v -l version -d "Display version"

    # color
    complete -f -c apm -n '__fish_apm_needs_command' -l color -d "Enable colored output"
    complete -f -c apm -n '__fish_apm_needs_command' -l no-color -d "Disable colored output"

    ############
    # commands #
    ############

    # clean
    complete -f -c apm -n '__fish_apm_needs_command' -a clean -d "Deletes packages in node_modules not referenced
as dependency in package.json"

    # config
    set -l config_subcommands "get set edit delete list"
    complete -f -c apm -n '__fish_apm_needs_command' -a config -d "Modify or list configuration items"
    complete -x -c apm -n "__fish_apm_using_command config; and not __fish_apm_includes_subcommand $config_subcommands" -a set -d 'Set config item'
    complete -x -c apm -n "__fish_apm_using_command config; and not __fish_apm_includes_subcommand $config_subcommands" -a get -d 'Get config item'
    complete -x -c apm -n "__fish_apm_using_command config; and not __fish_apm_includes_subcommand $config_subcommands" -a delete -d 'Delete config item'
    complete -x -c apm -n "__fish_apm_using_command config; and not __fish_apm_includes_subcommand $config_subcommands" -a list -d 'List config items'
    complete -x -c apm -n "__fish_apm_using_command config; and not __fish_apm_includes_subcommand $config_subcommands" -a edit -d 'Edit config items'
    complete -x -c apm -n '__fish_apm_using_command config set get delete; and __fish_apm_includes_subcommand set get delete' -a '(__fish_apm_config_items)'

    # dedupe
    complete -f -c apm -n '__fish_apm_needs_command' -a dedupe -d "Reduce duplication in node_modules in current directory"

    # deinstall & delete & erase & remove & rm & uninstall
    set -l uninstall_commands "deinstall delete erase remove rm uninstall"
    complete -f -c apm -n '__fish_apm_needs_command' -a $uninstall_commands -d "Delete installed package(s) from ~/.atom/packages"
    complete -f -c apm -n "__fish_apm_using_command $uninstall_commands" -a '(__fish_apm_list_packages)'
    complete -f -c apm -n "__fish_apm_using_command $uninstall_commands" -l hard -d "Unlink package from ~/.atom/packages and ~/.atom/dev/packages"
    complete -f -c apm -n "__fish_apm_using_command $uninstall_commands" -s d -l dev -d "Unlink package from ~/.atom/dev/packages"

    # dev(elop)
    set -l dev_commands "dev develop"
    complete -f -c apm -n '__fish_apm_needs_command' -a $dev_commands -d "Clone install dependencies and link a package for development"
    complete -f -c apm -n "__fish_apm_using_command $dev_commands" -a '(__fish_apm_list_packages)'

    # docs & home & open
    set -l open_commands "docs home open"
    complete -f -c apm -n '__fish_apm_needs_command' -a $open_commands -d "Open a package's homepage in the default browser"
    complete -f -c apm -n "__fish_apm_using_command $open_commands" -s p -l print -d "Print URL instead of opening"
    complete -f -c apm -n "__fish_apm_using_command $open_commands" -a '(__fish_apm_list_packages)'

    # featured
    complete -f -c apm -n '__fish_apm_needs_command' -a featured -d "List Atom packages and themes currently featured in the
atom.io registry"
    complete -f -c apm -n '__fish_apm_using_command featured' -l json -d "Output featured packages as JSON array"
    complete -f -c apm -n '__fish_apm_using_command featured' -s t -l themes -d "Only list themes"
    complete -x -c apm -n '__fish_apm_using_command featured' -s c -l compatible -d "Only list packages/themes compatible with specified Atom version"

    # init
    complete -f -c apm -n '__fish_apm_needs_command' -a init -d "Generates code scaffolding for theme or package"
    complete -r -c apm -n '__fish_apm_using_command init' -l template -d "Path to the package or theme template"
    complete -x -c apm -n '__fish_apm_using_command init' -s p -l package -d "Generates a basic package"
    complete -x -c apm -n '__fish_apm_using_command init' -s t -l theme -d "Generates a basic theme"
    complete -x -c apm -n '__fish_apm_using_command init' -s l -l language -d "Generates a basic language package"
    complete -r -c apm -n '__fish_apm_using_command init' -s c -l convert -d "Convert TextMate bundle/theme"

    # install
    complete -f -c apm -n '__fish_apm_needs_command' -a install -d "Install Atom package to ~/.atom/packages/<package_name>"
    complete -f -c apm -n '__fish_apm_using_command install' -l check -d "Check native build tools are installed"
    complete -f -c apm -n '__fish_apm_using_command install' -l verbose -d "Show verbose debug information"
    complete -r -c apm -n '__fish_apm_using_command install' -l packages-file -d "Specify text file containing packages to install"
    complete -f -c apm -n '__fish_apm_using_command install' -l production -d "Do not install dev dependencies"
    complete -x -c apm -n '__fish_apm_using_command install' -s c -l compatible -d "Only install packages/themes compatible with specified Atom version"
    complete -f -c apm -n '__fish_apm_using_command install' -s s -l silent -d "Set npm log level to silent"
    complete -f -c apm -n '__fish_apm_using_command install' -s q -l quiet -d "Set npm log level to quiet"

    # link & ln
    set -l link_commands "link ln"
    complete -f -c apm -n '__fish_apm_needs_command' -a $link_commands -d "Create a symlink for the package in ~/.atom/packages"
    complete -f -c apm -n "__fish_apm_using_command $link_commands" -s d -l dev -d "Link to ~/.atom/dev/packages"

    # linked & links & lns
    set -l linked_commands "linked links lns"
    complete -f -c apm -n '__fish_apm_needs_command' -a $linked_commands -d "List all symlinked packages in ~/.atom/packages and
~/.atom/dev/packages"

    # list & ls
    set -l list_commands "list ls"
    complete -f -c apm -n '__fish_apm_needs_command' -a $list_commands -d "List all installed and bundled packages"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s b -l bare -d "Print packages one per line with no formatting"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s d -l dev -d "Include dev packages"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s i -l installed -d "Only list installed packages/themes"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s j -l json -d "Output all packages as a JSON object"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s l -l links -d "Include linked packages"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s t -l themes -d "Only list themes"
    complete -f -c apm -n "__fish_apm_using_command $list_commands" -s p -l packages -d "Only list packages"

    # login
    complete -f -c apm -n '__fish_apm_needs_command' -a login -d "Save Atom API token to keychain"
    complete -x -c apm -n "__fish_apm_using_command login" -l token -d "Specify API token"

    # outdated & upgrade & update
    set  -l upgrade_commands "outdated upgrade update"
    complete -f -c apm -n '__fish_apm_needs_command' -a $upgrade_commands -d "Upgrade out of date packages"
    complete -f -c apm -n "__fish_apm_using_command $upgrade_commands" -l json -d "Output outdated packages as JSON array"
    complete -x -c apm -n "__fish_apm_using_command $upgrade_commands" -l compatible -d "Only install packages/themes compatible with specified Atom version"
    complete -f -c apm -n "__fish_apm_using_command $upgrade_commands" -l verbose -d "Show verbose debug information"
    complete -f -c apm -n "__fish_apm_using_command $upgrade_commands" -s c -l confirm -d "Confirm before installing updates"
    complete -f -c apm -n "__fish_apm_using_command $upgrade_commands" -s l -l list -d "List but don't install the outdated packages"

    # publish
    set -l publish_subcommands "major minor patch build"
    complete -f -c apm -n '__fish_apm_needs_command' -a publish -d "Publish new version of package in current working directory"
    complete -f -c apm -n "__fish_apm_using_command publish; and not __fish_apm_includes_subcommand $publish_subcommands" -a $publish_subcommands -d "Semantic version category for new version"
    complete -x -c apm -n '__fish_apm_using_command publish' -s t -l tag -d "Specify a tag to publish"
    complete -x -c apm -n '__fish_apm_using_command publish' -s r -l rename -d "Specify a new name for the package"

    # rebuild
    complete -f -c apm -n '__fish_apm_needs_command' -a rebuild -d "Rebuild modules installed in node_modules in current working directory"

    # rebuild-module-cache
    complete -f -c apm -n '__fish_apm_needs_command' -a rebuild-module-cache -d "Rebuild module cache for installed packages"

    # search
    complete -f -c apm -n '__fish_apm_needs_command' -a search -d "Search for Atom packages/themes in the atom.io registry"
    complete -f -c apm -n '__fish_apm_using_command search' -l json -d "Output matching packages as JSON array"
    complete -f -c apm -n '__fish_apm_using_command search' -s p -l packages -d "Search only non-theme packages"
    complete -f -c apm -n '__fish_apm_using_command search' -s t -l themes -d "Search only themes"

    # show & view
    set -l show_commands "show view"
    complete -f -c apm -n '__fish_apm_needs_command' -a $show_commands -d "View information about package/theme in the atom.io registry"
    complete -f -c apm -n "__fish_apm_using_command $show_commands" -l json -d "Output as JSON array"
    complete -x -c apm -n "__fish_apm_using_command $show_commands" -l compatible -d "Show latest version compatible with specified Atom version"

    # star
    complete -f -c apm -n '__fish_apm_needs_command' -a star -d "Star the given packages on https://atom.io"
    complete -f -c apm -n '__fish_apm_using_command star' -l installed -d "Star all packages in ~/.atom/packages"

    # starred & stars
    set -l stars_commands "starred stars"
    complete -f -c apm -n '__fish_apm_needs_command' -a $stars_commands -d "List or install starred Atom packages and themes"
    complete -f -c apm -n "__fish_apm_using_command $stars_commands" -l json -d "Output packages as JSON array"
    complete -f -c apm -n "__fish_apm_using_command $stars_commands" -s i -l install -d "Install the starred packages"
    complete -f -c apm -n "__fish_apm_using_command $stars_commands" -s t -l themes -d "Only list themes"
    complete -x -c apm -n "__fish_apm_using_command $stars_commands" -s u -l user -d "GitHub username to show starred packages for"

    # test
    complete -f -c apm -n '__fish_apm_needs_command' -a test -d "Runs the package's tests contained within the spec directory"
    complete -r -c apm -n '__fish_apm_using_command test' -s p -l path -d "Path to atom command"

    # unlink
    complete -f -c apm -n '__fish_apm_needs_command' -a unlink -d "Delete the symlink in ~/.atom/packages for the package"
    complete -f -c apm -n '__fish_apm_using_command unlink' -l hard -d "Unlink package from ~/.atom/packages and ~/.atom/dev/packages"
    complete -f -c apm -n '__fish_apm_using_command unlink' -s d -l dev -d "Unlink package from ~/.atom/dev/packages"
    complete -f -c apm -n '__fish_apm_using_command unlink' -s a -l all -d "Unlink all packages in ~/.atom/packages and ~/.atom/dev/packages"

    # unpublish
    complete -f -c apm -n '__fish_apm_needs_command' -a unpublish -d "Remove published package or version from the atom.io registry"
    complete -f -c apm -n '__fish_apm_using_command unpublish' -s f -l force -d "Do not prompt for confirmation"

    # unstar
    complete -f -c apm -n '__fish_apm_needs_command' -a unstar -d "Unstar given packages on https://atom.io"

else
    # Completions for the Advanced Power Management client

    complete -f -c apm -s V -l version --description "Display version and exit"
    complete -f -c apm -s v -l verbose --description "Print APM info"
    complete -f -c apm -s m -l minutes --description "Print time remaining"
    complete -f -c apm -s M -l monitor --description "Monitor status info"
    complete -f -c apm -s S -l standby --description "Request APM standby mode"
    complete -f -c apm -s s -l suspend --description "Request APM suspend mode"
    complete -f -c apm -s d -l debug --description "APM status debugging info"
end
