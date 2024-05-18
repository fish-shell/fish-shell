function __fish_opam_using_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

function __fish_opam_needs_command
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

# general flags
## ones that work without any subcommand
complete -f -c opam -l help -d "Display the manual for an OPAM command"
complete -f -c opam -l version -d 'Show version information'
## ones that require at least a subcommand (but are shared by all)
complete -f -c opam -n 'not __fish_opam_needs_command' -l color -d 'Colorize the output' -a 'always never auto'
complete -f -c opam -n 'not __fish_opam_needs_command' -l 'compat-mode-1.0' -d 'Compatibility mode with OPAM 1.0'
complete -f -c opam -n 'not __fish_opam_needs_command' -l debug -d 'Print debug message on stdout'
complete -f -c opam -n 'not __fish_opam_needs_command' -l git-version -d 'Print the git version if it exists and exit'
complete -f -c opam -n 'not __fish_opam_needs_command' -l no-aspcud -d 'Do not use the external aspcud solver, even if available'
complete -f -c opam -n 'not __fish_opam_needs_command' -l no-base-packages -d 'Do not install base packages'
complete -f -c opam -n 'not __fish_opam_needs_command' -s q -l quiet -d 'Be quiet when installing a new compiler'
complete -f -c opam -n 'not __fish_opam_needs_command' -s r -l root -d 'Use ROOT as the current root path'
complete -f -c opam -n 'not __fish_opam_needs_command' -l strict -d 'Fail whenever an error is found in a package definition or a configuration file'
complete -f -c opam -n 'not __fish_opam_needs_command' -l switch -d 'Use SWITCH as the current compiler switch'
complete -f -c opam -n 'not __fish_opam_needs_command' -s v -l verbose -d 'Be more verbose'
complete -f -c opam -n 'not __fish_opam_needs_command' -s y -l yes -d 'Disable interactive mode and answer yes to all questions'

# subcommands
## config
complete -f -c opam -n __fish_opam_needs_command -a config -d "Display configuration options for packages"
### config flags
complete -f -c opam -n '__fish_opam_using_command config' -s a -l all -d 'Enable all the global and user configuration options'
complete -f -c opam -n '__fish_opam_using_command config' -l csh -d 'Use csh-compatible mode for configuring OPAM'
complete -f -c opam -n '__fish_opam_using_command config' -l dot-profile -d 'Name of the configuration file to update instead of ~/'
complete -f -c opam -n '__fish_opam_using_command config' -s e -d 'Backward-compatible option, equivalent to opam config env'
complete -f -c opam -n '__fish_opam_using_command config' -l fish -d 'Use fish-compatible mode for configuring OPAM'
complete -f -c opam -n '__fish_opam_using_command config' -s g -l global -d 'Enable all the global configuration options'
complete -f -c opam -n '__fish_opam_using_command config' -s l -l list -d 'List the current configuration'
complete -f -c opam -n '__fish_opam_using_command config' -l no-complete -d 'Do not load the auto-completion scripts in the environment'
complete -f -c opam -n '__fish_opam_using_command config' -l no-switch-eval -d 'Do not install `opam-switch-eval`'
complete -f -c opam -n '__fish_opam_using_command config' -l ocamlinit -d 'Modify ~/'
complete -f -c opam -n '__fish_opam_using_command config' -l profile -d 'Modify ~/. profile'
complete -f -c opam -n '__fish_opam_using_command config' -s R -l rec -d 'Recursive query'
complete -f -c opam -n '__fish_opam_using_command config' -l sexp -d 'Display environment variables as an s-expression'
complete -f -c opam -n '__fish_opam_using_command config' -l sh -d 'Use sh-compatible mode for configuring OPAM'
complete -f -c opam -n '__fish_opam_using_command config' -s u -l user -d 'Enable all the user configuration options'
complete -f -c opam -n '__fish_opam_using_command config' -l zsh -d 'Use zsh-compatible mode for configuring OPAM.  DOMAINS'
### config subcommands
complete -f -c opam -n '__fish_opam_using_command config' -a env -d 'Return PATH, MANPATH, OCAML_TOPLEVEL_PATH and CAML_LD_LIBRARY_PATH for compiler'
complete -f -c opam -n '__fish_opam_using_command config' -a setup -d 'Configure global and user parameters for OPAM' #TODO
complete -f -c opam -n '__fish_opam_using_command config' -a exec -d 'Execute the shell script given in parameter with the correct environment variables'
complete -f -c opam -n '__fish_opam_using_command config' -a var -d 'Return the value associated with the given variable'
complete -f -c opam -n '__fish_opam_using_command config' -a list -d 'Return the list of all variables defined in the listed packages'
complete -f -c opam -n '__fish_opam_using_command config' -a subst -d 'Substitute %{var}% variables'
complete -f -c opam -n '__fish_opam_using_command config' -a includes -d 'returns include options'
complete -f -c opam -n '__fish_opam_using_command config' -a bytecomp -d 'returns bytecode compile options'
complete -f -c opam -n '__fish_opam_using_command config' -a asmcomp -d 'returns assembly compile options'
complete -f -c opam -n '__fish_opam_using_command config' -a bytelink -d 'returns bytecode linking options'
complete -f -c opam -n '__fish_opam_using_command config' -a report -d 'Prints a summary of your setup, useful for bug-reports'

# TODO all subcommands other than config (and admin?)
complete -f -c opam -n __fish_opam_needs_command -a help -d "Display help about OPAM and OPAM commands"
complete -f -c opam -n __fish_opam_needs_command -a init -d "Initialize OPAM state"
complete -f -c opam -n __fish_opam_needs_command -a install -d "Install a list of packages"
complete -f -c opam -n __fish_opam_needs_command -a list -d "Display the list of available packages"
complete -f -c opam -n __fish_opam_needs_command -a pin -d "Pin a given package to a specific version"
complete -f -c opam -n __fish_opam_needs_command -a reinstall -d "Reinstall a list of packages"
complete -f -c opam -n __fish_opam_needs_command -a "remove uninstall" -d "Remove a list of packages"
complete -f -c opam -n __fish_opam_needs_command -a "repository remote" -d "Manage OPAM repositories"
complete -f -c opam -n __fish_opam_needs_command -a search -d "Search into the package list"
complete -f -c opam -n __fish_opam_needs_command -a "show info" -d "Display information about specific packages"
complete -f -c opam -n __fish_opam_needs_command -a switch -d "Manage multiple installation of compilers"
complete -f -c opam -n __fish_opam_needs_command -a update -d "Update the list of available packages"
complete -f -c opam -n __fish_opam_needs_command -a upgrade -d "Upgrade the installed package to latest version"
## admin
complete -f -c opam -n __fish_opam_needs_command -a admin -d "Administration tool for local repositories"
complete -c opam -n '__fish_opam_using_command admin' -l help -d 'Show this help in format FMT (pager, plain or groff)'
complete -c opam -n '__fish_opam_using_command admin' -l version -d 'Show version information'
complete -f -c opam -n '__fish_opam_using_command admin' -a check -d "Check a local repo for errors"
complete -f -c opam -n '__fish_opam_using_command admin' -a depexts -d "Add external dependencies"
complete -f -c opam -n '__fish_opam_using_command admin' -a make -d "Initialize a repo for serving files"
complete -f -c opam -n '__fish_opam_using_command admin' -a stats -d "Compute statistics"

# opam switch
set -l switchcommands create set remove export import reinstall list list-available show set-base set-description link
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a create -d 'Create a new switch, and install the given compiler there'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a set -d 'Set the currently active switch'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a remove -d 'Remove the given switch from disk'
complete -c opam -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a export -d 'Save the current switch state to a file'
complete -c opam -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a import -d 'Import a saved switch state'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a reinstall -d 'Reinstall the given compiler switch and all its packages'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a list -d 'Lists installed switches'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a list-available -d 'Lists base packages that can be used to create a new switch'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a show -d 'Prints the name of the current switch'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a set-base -d 'Sets the packages forming the immutable base for the selected switch'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and not __fish_seen_subcommand_from $switchcommands" -a link -d 'Sets a local alias for a given switch'

complete -c opam -f -n "__fish_seen_subcommand_from switch; and __fish_seen_subcommand_from set" -a '(opam switch list --short)'
complete -c opam -f -n "__fish_seen_subcommand_from switch; and __fish_seen_subcommand_from remove" -a '(opam switch list --short)'

complete -c opam -f -n "__fish_seen_subcommand_from switch; and __fish_seen_subcommand_from create" -a '(opam switch list-available --short)'
