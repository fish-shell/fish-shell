#completion for apt-get

function __fish_apt_no_subcommand -d (_ 'Test if apt has yet to be given the subcommand')
	for i in (commandline -opc)
		if contains -- $i update upgrade dselect-upgrade dist-upgrade install remove source build-dep check clean autoclean
			return 1
		end
	end
	return 0
end

function __fish_apt_use_package -d (_ 'Test if apt command should have packages as potential completion')
	for i in (commandline -opc)
		if contains -- $i contains install remove build-dep
			return 0
		end
	end
	return 1
end

complete -c apt-get -n '__fish_apt_use_package' -a '(__fish_print_packages)' -d (_ 'Package')

complete -c apt-get -s h -l help -d (_ 'Display help and exit')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'update' -d (_ 'Update sources')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'upgrade' -d (_ 'Upgrade or install newest packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'dselect-upgrade' -d (_ 'Use with dselect front-end')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'dist-upgrade' -d (_ 'Distro upgrade')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'install' -d (_ 'Install one or more packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'remove' -d (_ 'Remove one or more packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'source' -d (_ 'Fetch source packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'build-dep' -d (_ 'Install/remove packages for dependencies')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'check' -d (_ 'Update cache and check dependencies')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'clean' -d (_ 'Clean local caches and packages')
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'autoclean' -d (_ 'Clean packages no longer be downloaded')
complete -c apt-get -s d -l download-only -d (_ 'Download Only')
complete -c apt-get -s f -l fix-broken -d (_ 'Correct broken dependencies')
complete -c apt-get -s m -l fix-missing -d (_ 'Ignore missing packages')
complete -c apt-get -l no-download -d (_ 'Disable downloading packages')
complete -c apt-get -s q -l quiet -d (_ 'Quiet mode')
complete -c apt-get -s s -l simulate -d (_ 'Perform a simulation')
complete -c apt-get -s y -l assume-yes -d (_ 'Automatic yes to prompts')
complete -c apt-get -s u -l show-upgraded -d (_ 'Show upgraded packages')
complete -c apt-get -s V -l verbose-versions -d (_ 'Show full versions for packages')
complete -c apt-get -s b -l compile -d (_ 'Compile source packages')
complete -c apt-get -s b -l build -d (_ 'Compile source packages')
complete -c apt-get -l ignore-hold -d (_ 'Ignore package Holds')
complete -c apt-get -l no-upgrade -d (_ "Do not upgrade packages")
complete -c apt-get -l force-yes -d (_ 'Force yes')
complete -c apt-get -l print-uris -d (_ 'Print the URIs')
complete -c apt-get -l purge -d (_ 'Use purge instead of remove')
complete -c apt-get -l reinstall -d (_ 'Reinstall packages')
complete -c apt-get -l list-cleanup -d (_ 'Erase obsolete files')
complete -c apt-get -s t -l target-release -d (_ 'Control default input to the policy engine')
complete -c apt-get -l trivial-only -d (_ 'Only perform operations that are trivial')
complete -c apt-get -l no-remove -d (_ 'Abort if any packages are to be removed')
complete -c apt-get -l only-source -d (_ 'Only accept source packages')
complete -c apt-get -l diff-only -d (_ 'Download only diff file')
complete -c apt-get -l tar-only -d (_ 'Download only tar file')
complete -c apt-get -l arch-only -d (_ 'Only process arch-dependant build-dependencies')
complete -c apt-get -l allow-unauthenticated -d (_ 'Ignore non-authenticated packages')
complete -c apt-get -s v -l version -d (_ 'Display version and exit')
complete -r -c apt-get -s c -l config-file -d (_ 'Specify a config file')
complete -r -c apt-get -s o -l option -d (_ 'Set a config option')

