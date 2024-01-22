#completion for apt-get

function __fish_apt_no_subcommand -d 'Test if apt has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i update upgrade dselect-upgrade dist-upgrade install remove purge source build-dep check clean autoclean changelog
            return 1
        end
    end
    return 0
end

function __fish_apt_use_package -d 'Test if apt command should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i contains install remove purge build-dep changelog
            return 0
        end
    end
    return 1
end

complete -c apt-get -n __fish_apt_use_package -a '(__fish_print_apt_packages)' -d Package

complete -c apt-get -s h -l help -d 'Display help and exit'
complete -f -n __fish_apt_no_subcommand -c apt-get -a update -d 'Update sources'
complete -f -n __fish_apt_no_subcommand -c apt-get -a upgrade -d 'Upgrade or install newest packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a dselect-upgrade -d 'Use with dselect front-end'
complete -f -n __fish_apt_no_subcommand -c apt-get -a dist-upgrade -d 'Distro upgrade'
complete -f -n __fish_apt_no_subcommand -c apt-get -a install -d 'Install one or more packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a changelog -d 'Display changelog of one or more packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a purge -d 'Remove and purge one or more packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a remove -d 'Remove one or more packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a source -d 'Fetch source packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a build-dep -d 'Install/remove packages for dependencies'
complete -f -n __fish_apt_no_subcommand -c apt-get -a check -d 'Update cache and check dependencies'
complete -f -n __fish_apt_no_subcommand -c apt-get -a clean -d 'Clean local caches and packages'
complete -f -n __fish_apt_no_subcommand -c apt-get -a autoclean -d 'Clean packages no longer be downloaded'
complete -f -n __fish_apt_no_subcommand -c apt-get -a autoremove -d 'Remove automatically installed packages'
complete -c apt-get -l no-install-recommends -d 'Do not install recommended packages'
complete -c apt-get -l no-install-suggests -d 'Do not install suggested packages'
complete -c apt-get -s d -l download-only -d 'Download Only'
complete -c apt-get -s f -l fix-broken -d 'Correct broken dependencies'
complete -c apt-get -s m -l fix-missing -d 'Ignore missing packages'
complete -c apt-get -l no-download -d 'Disable downloading packages'
complete -c apt-get -s q -l quiet -d 'Quiet mode'
complete -c apt-get -s s -l simulate -l just-print -l dry-run -l recon -l no-act -d 'Perform a simulation'
complete -c apt-get -s y -l yes -l assume-yes -d 'Automatic yes to prompts'
complete -c apt-get -l assume-no -d 'Automatic no to prompts'
complete -c apt-get -s u -l show-upgraded -d 'Show upgraded packages'
complete -c apt-get -s V -l verbose-versions -d 'Show full versions for packages'
complete -c apt-get -s b -l compile -l build -d 'Compile source packages'
complete -c apt-get -l install-recommends -d 'Install recommended packages'
complete -c apt-get -l install-suggests -d 'Install suggested packages'
complete -c apt-get -l ignore-hold -d 'Ignore package Holds'
complete -c apt-get -l no-upgrade -d "Do not upgrade packages"
complete -c apt-get -l force-yes -d 'Force yes'
complete -c apt-get -l print-uris -d 'Print the URIs'
complete -c apt-get -l purge -d 'Use purge instead of remove'
complete -c apt-get -l reinstall -d 'Reinstall packages'
complete -c apt-get -l list-cleanup -d 'Erase obsolete files'
complete -c apt-get -s t -l target-release -l default-release -d 'Control default input to the policy engine'
complete -c apt-get -l trivial-only -d 'Only perform operations that are trivial'
complete -c apt-get -l no-remove -d 'Abort if any packages are to be removed'
complete -c apt-get -l only-source -d 'Only accept source packages'
complete -c apt-get -l diff-only -d 'Download only diff file'
complete -c apt-get -l tar-only -d 'Download only tar file'
complete -c apt-get -l arch-only -d 'Only process arch-dependant build-dependencies'
complete -c apt-get -l allow-unauthenticated -d 'Ignore non-authenticated packages'
complete -c apt-get -s v -l version -d 'Display version and exit'
complete -r -c apt-get -s c -l config-file -d 'Specify a config file'
complete -r -c apt-get -s o -l option -d 'Set a config option'
