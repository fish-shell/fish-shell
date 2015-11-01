#completion for apt-get

function __fish_apt_no_subcommand --description 'Test if apt has yet to be given the subcommand'
	for i in (commandline -opc)
		if contains -- $i update upgrade dselect-upgrade dist-upgrade install remove purge source build-dep check clean autoclean changelog
			return 1
		end
	end
	return 0
end

function __fish_apt_use_package --description 'Test if apt command should have packages as potential completion'
	for i in (commandline -opc)
		if contains -- $i contains install remove purge build-dep changelog
			return 0
		end
	end
	return 1
end

complete -c apt-get -n '__fish_apt_use_package' -a '(__fish_print_packages)' --description 'Package'

complete -c apt-get -s h -l help --description 'Display help and exit'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'update' --description 'Update sources'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'upgrade' --description 'Upgrade or install newest packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'dselect-upgrade' --description 'Use with dselect front-end'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'dist-upgrade' --description 'Distro upgrade'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'install' --description 'Install one or more packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'changelog' --description 'Display changelog of one or more packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'purge' --description 'Remove and purge one or more packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'remove' --description 'Remove one or more packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'source' --description 'Fetch source packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'build-dep' --description 'Install/remove packages for dependencies'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'check' --description 'Update cache and check dependencies'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'clean' --description 'Clean local caches and packages'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'autoclean' --description 'Clean packages no longer be downloaded'
complete -f -n '__fish_apt_no_subcommand' -c apt-get -a 'autoremove' --description 'Remove automatically installed packages'
complete -c apt-get -l no-install-recommends --description 'Do not install recommended packages'
complete -c apt-get -s d -l download-only --description 'Download Only'
complete -c apt-get -s f -l fix-broken --description 'Correct broken dependencies'
complete -c apt-get -s m -l fix-missing --description 'Ignore missing packages'
complete -c apt-get -l no-download --description 'Disable downloading packages'
complete -c apt-get -s q -l quiet --description 'Quiet mode'
complete -c apt-get -s s -l simulate -l just-print -l dry-run -l recon -l no-act --description 'Perform a simulation'
complete -c apt-get -s y -l yes -l assume-yes --description 'Automatic yes to prompts'
complete -c apt-get -s u -l show-upgraded --description 'Show upgraded packages'
complete -c apt-get -s V -l verbose-versions --description 'Show full versions for packages'
complete -c apt-get -s b -l compile -l build --description 'Compile source packages'
complete -c apt-get -l install-recommends --description 'Install recommended packages'
complete -c apt-get -l ignore-hold --description 'Ignore package Holds'
complete -c apt-get -l no-upgrade --description "Do not upgrade packages"
complete -c apt-get -l force-yes --description 'Force yes'
complete -c apt-get -l print-uris --description 'Print the URIs'
complete -c apt-get -l purge --description 'Use purge instead of remove'
complete -c apt-get -l reinstall --description 'Reinstall packages'
complete -c apt-get -l list-cleanup --description 'Erase obsolete files'
complete -c apt-get -s t -l target-release -l default-release --description 'Control default input to the policy engine'
complete -c apt-get -l trivial-only --description 'Only perform operations that are trivial'
complete -c apt-get -l no-remove --description 'Abort if any packages are to be removed'
complete -c apt-get -l only-source --description 'Only accept source packages'
complete -c apt-get -l diff-only --description 'Download only diff file'
complete -c apt-get -l tar-only --description 'Download only tar file'
complete -c apt-get -l arch-only --description 'Only process arch-dependant build-dependencies'
complete -c apt-get -l allow-unauthenticated --description 'Ignore non-authenticated packages'
complete -c apt-get -s v -l version --description 'Display version and exit'
complete -r -c apt-get -s c -l config-file --description 'Specify a config file'
complete -r -c apt-get -s o -l option --description 'Set a config option'

