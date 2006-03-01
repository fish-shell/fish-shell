#completion for apt-get

function __fish_apt_no_subcommand -d (N_ 'Test if apt has yet to be given the subcommand')
	for i in (commandline -opc)
		if contains -- $i update upgrade dselect-upgrade dist-upgrade install remove source build-dep check clean autoclean
			return 1
		end
	end
	return 0
end

function __fish_apt_use_package -d (N_ 'Test if apt command should have packages as potential completion')
	for i in (commandline -opc)
		if contains -- $i contains install remove build-dep
			return 0
		end
	end
	return 1
end

complete -c apt-get -n '__fish_apt_use_package' -a '(__fish_print_packages)' -d (N_ 'Package')

complete -c apt-get -s h -l help -d (N_ 'Display help and exit')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'update' -d (N_ 'Update sources')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'upgrade' -d (N_ 'Upgrade or install newest packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'dselect-upgrade' -d (N_ 'Use with dselect front-end')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'dist-upgrade' -d (N_ 'Distro upgrade')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'install' -d (N_ 'Install one or more packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'remove' -d (N_ 'Remove one or more packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'source' -d (N_ 'Fetch source packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'build-dep' -d (N_ 'Install/remove packages for dependencies')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'check' -d (N_ 'Update cache and check dependencies')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'clean' -d (N_ 'Clean local caches and packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'autoclean' -d (N_ 'Clean packages no longer be downloaded')
complete -c apt-get -s d -l download-only -d (N_ 'Download Only')
complete -c apt-get -s f -l fix-broken -d (N_ 'Correct broken dependencies')
complete -c apt-get -s m -l fix-missing -d (N_ 'Ignore missing packages')
complete -c apt-get -l no-download -d (N_ 'Disable downloading packages')
complete -c apt-get -s q -l quiet -d (N_ 'Quiet mode')
complete -c apt-get -s s -l simulate -d (N_ 'Perform a simulation')
complete -c apt-get -s y -l assume-yes -d (N_ 'Automatic yes to prompts')
complete -c apt-get -s u -l show-upgraded -d (N_ 'Show upgraded packages')
complete -c apt-get -s V -l verbose-versions -d (N_ 'Show full versions for packages')
complete -c apt-get -s b -l compile -d (N_ 'Compile source packages')
complete -c apt-get -s b -l build -d (N_ 'Compile source packages')
complete -c apt-get -l ignore-hold -d (N_ 'Ignore package Holds')
complete -c apt-get -l no-upgrade -d (N_ "Do not upgrade packages")
complete -c apt-get -l force-yes -d (N_ 'Force yes')
complete -c apt-get -l print-uris -d (N_ 'Print the URIs')
complete -c apt-get -l purge -d (N_ 'Use purge instead of remove')
complete -c apt-get -l reinstall -d (N_ 'Reinstall packages')
complete -c apt-get -l list-cleanup -d (N_ 'Erase obsolete files')
complete -c apt-get -s t -l target-release -d (N_ 'Control default input to the policy engine')
complete -c apt-get -l trivial-only -d (N_ 'Only perform operations that are trivial')
complete -c apt-get -l no-remove -d (N_ 'Abort if any packages are to be removed')
complete -c apt-get -l only-source -d (N_ 'Only accept source packages')
complete -c apt-get -l diff-only -d (N_ 'Download only diff file')
complete -c apt-get -l tar-only -d (N_ 'Download only tar file')
complete -c apt-get -l arch-only -d (N_ 'Only process arch-dependant build-dependencies')
complete -c apt-get -l allow-unauthenticated -d (N_ 'Ignore non-authenticated packages')
complete -c apt-get -s v -l version -d (N_ 'Display version and exit')
complete -r -c apt-get -s c -l config-file -d (N_ 'Specify a config file')
complete -r -c apt-get -s o -l option -d (N_ 'Set a config option')

