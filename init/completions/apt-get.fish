#completion for apt-get

function __fish_apt_no_subcommand -d "test if apt has yet to be given the subcommand"
	for i in (commandline -opc)
		if contains -- $i update upgrade dselect-upgrade dist-upgrade install remove source build-dep check clean autoclean
			return 1
		end
	end
	return 0
end

function __fish_apt_use_package -d "Test if apt command should have packages as potential completion"
	for i in (commandline -opc)
		if contains -- $i contains install remove build-dep
			return 0
		end
	end
	return 1
end

complete -c apt-get -n "__fish_apt_use_package" -a "(__fish_print_packages)" -d "Package"

complete -c apt-get -s h -l help -d "apt-get command help"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "update" -d "update sources"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "upgrade" -d "upgrade or install newest packages"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "dselect-upgrade" -d "use with dselect front-end"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "dist-upgrade" -d "distro upgrade"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "install" -d "install one or more packages"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "remove" -d "remove one or more packages"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "source" -d "fetch source packages"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "build-dep" -d "install/remove packages for dependencies"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "check" -d "update cache and check dep"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "clean" -d "clean local caches and packages"
complete -f -n "__fish_apt_no_subcommand" -c apt-get -a "autoclean" -d "clean packages no longer be downloaded"
complete -c apt-get -s d -l download-only -d "Download Only"
complete -c apt-get -s f -l fix-broken -d "correct broken deps"
complete -c apt-get -s m -l fix-missing -d "ignore missing packages"
complete -c apt-get -l no-download -d "Disable downloading packages"
complete -c apt-get -s q -l quiet -d "quiet output"
complete -c apt-get -s s -l simulate -d "perform a siulation"
complete -c apt-get -s y -l assume-yes -d "automatic yes to prompts"
complete -c apt-get -s u -l show-upgraded -d "show upgraded packages"
complete -c apt-get -s V -l verbose-versions -d "show full versions for packages"
complete -c apt-get -s b -l compile -d "compile source packages"
complete -c apt-get -s b -l build -d "compile source packages"
complete -c apt-get -l ignore-hold -d "ignore package Holds"
complete -c apt-get -l no-upgrade -d "Do not upgrade packages"
complete -c apt-get -l force-yes -d "Force yes"
complete -c apt-get -l print-uris -d "print the URIs"
complete -c apt-get -l purge -d "use purge instead of remove"
complete -c apt-get -l reinstall -d "reinstall packages"
complete -c apt-get -l list-cleanup -d "erase obsolete files"
complete -c apt-get -s t -l target-release -d "control default input to the policy engine"
complete -c apt-get -l trivial-only -d "only perform operations that are trivial"
complete -c apt-get -l no-remove -d "abort if any packages are to be removed"
complete -c apt-get -l only-source -d "only accept source packages"
complete -c apt-get -l diff-only -d "download only diff file"
complete -c apt-get -l tar-only -d "download only tar file"
complete -c apt-get -l arch-only -d "only process arch-dep build-deps"
complete -c apt-get -l allow-unauthenticated -d "ignore non-authenticated packages"
complete -c apt-get -s v -l version -d "show program version"
complete -r -c apt-get -s c -l config-file -d "specify a config file"
complete -r -c apt-get -s o -l option -d "set a config option"

