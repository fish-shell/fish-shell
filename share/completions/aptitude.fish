#completion for aptitude

function __fish_apt_no_subcommand -d 'Test if aptitude has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i autoclean clean forget-new keep-all update safe-upgrade changelog full-upgrade download forbid-version hold install keep markauto purge reinstall remove show unhold unmarkauto search help
            return 1
        end
    end
    return 0
end

function __fish_apt_use_package -d 'Test if aptitude command should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i changelog full-upgrade download forbid-version hold install keep-all markauto purge reinstall remove show unhold unmarkauto
            return 0
        end
    end
    return 1
end

complete -c aptitude -n __fish_apt_use_package -a '(__fish_print_apt_packages)' -d Package

complete -c aptitude -s h -l help -d 'Display a brief help message. Identical to the help action'
complete -f -n __fish_apt_no_subcommand -c aptitude -a autoclean -d 'Remove any cached packages which can no longer be downloaded'
complete -f -n __fish_apt_no_subcommand -c aptitude -a clean -d 'Remove all downloaded .deb files from the package cache directory'
complete -f -n __fish_apt_no_subcommand -c aptitude -a forget-new -d 'Forget all internal information about what packages are \'new\''
complete -f -n __fish_apt_no_subcommand -c aptitude -a keep-all -d 'Cancel all scheduled actions on all packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a update -d 'Update the list of available packages from the apt sources'
complete -f -n __fish_apt_no_subcommand -c aptitude -a safe-upgrade -d 'Upgrade installed packages to their most recent version'
complete -f -n __fish_apt_no_subcommand -c aptitude -a changelog -d 'Download and displays the Debian changelog for the packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a full-upgrade -d 'Upgrade, removing or installing packages as necessary'
complete -f -n __fish_apt_no_subcommand -c aptitude -a download -d 'Download the packages to the current directory'
complete -f -n __fish_apt_no_subcommand -c aptitude -a forbid-version -d 'Forbid the upgrade to a particular version'
complete -f -n __fish_apt_no_subcommand -c aptitude -a hold -d 'Ignore the packages by future upgrade commands'
complete -f -n __fish_apt_no_subcommand -c aptitude -a install -d 'Install the packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a keep -d 'Cancel any scheduled actions on the packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a markauto -d 'Mark packages as automatically installed'
complete -f -n __fish_apt_no_subcommand -c aptitude -a purge -d 'Remove and delete all associated configuration and data files'
complete -f -n __fish_apt_no_subcommand -c aptitude -a reinstall -d 'Reinstall the packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a remove -d 'Remove the packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a show -d 'Display detailed information about the packages'
complete -f -n __fish_apt_no_subcommand -c aptitude -a unhold -d 'Consider the packages by future upgrade commands'
complete -f -n __fish_apt_no_subcommand -c aptitude -a unmarkauto -d 'Mark packages as manually installed'
complete -f -n __fish_apt_no_subcommand -c aptitude -a search -d 'Search for packages matching one of the patterns'
complete -f -n __fish_apt_no_subcommand -c aptitude -a help -d 'Display brief summary of the available commands and options'

complete -c aptitude -s D -l show-deps -d 'Show explanations of automatic installations and removals'
complete -c aptitude -s d -l download-only -d 'Download Only'
complete -c aptitude -s f -l fix-broken -d 'Correct broken dependencies'
complete -c aptitude -l purge-unused -d 'Purge packages that are not required by any installed package'
complete -c aptitude -s P -l prompt -d 'Always display a prompt'
complete -c aptitude -s R -l without-recommends -d 'Do not treat recommendations as dependencies'
complete -c aptitude -s r -l with-recommends -d 'Treat recommendations as dependencies'
complete -c aptitude -s s -l simulate -d 'Don\'t perform the actions. Just show them'
complete -c aptitude -l schedule-only -d 'Schedule operations to be performed in the future'
complete -c aptitude -s q -l quiet -d 'Suppress incremental progress indicators'
complete -c aptitude -s V -l show-versions -d 'Show which versions of packages will be installed'
complete -c aptitude -s v -l verbose -d 'Display extra information'
complete -c aptitude -l version -d 'Display the version of aptitude and compile information'
complete -c aptitude -l visual-preview -d 'Start up the visual interface and display its preview screen'
complete -c aptitude -s y -l assume-yes -d 'Assume the answer yes for all question prompts'
complete -c aptitude -s Z -d 'Show how much disk space will be used or freed'
complete -r -c aptitude -s F -l display-format -d 'Specify the format to be used by the search command'
complete -r -c aptitude -s t -l target-release -d 'Set the release from which packages should be installed'
complete -r -c aptitude -s O -l sort -d 'Specify the order for the output from the search command'
complete -r -c aptitude -s o -d 'Set a configuration file option directly'
complete -r -c aptitude -s w -l width -d 'Specify the display width for the output from the search command'
