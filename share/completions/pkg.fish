# Completions for pkgng package manager

function __fish_pkg_is
	for option in $argv
		if contains $option (commandline -poc)
			return 0
		end
	end
	return 1
end

function __fish_pkg_subcommand
	set -l skip_next 1
	for token in (commandline -opc)
		if test $skip_next = 1
			set skip_next 0
			continue
		end
		switch $token
			# Option parser allows shortened long options
			case '--*=*'
				continue
			case -{o,j,c,r,C,R} --op\* --j\* --ch\* --ro\* --co\* --re\*
				set skip_next 1
				continue
			case '-*'
				continue
		end
		return 1
	end
	return 0
end

complete -c pkg -n __fish_pkg_subcommand    -s v -l version       -d "Display version and exit"
complete -c pkg -n __fish_pkg_subcommand    -s d -l debug         -d "Show debug information"
complete -c pkg -n __fish_pkg_subcommand    -s l -l list          -d "List subcommands"
complete -c pkg -n __fish_pkg_subcommand -x -s o -l option        -d "Set configuration option"
complete -c pkg -n __fish_pkg_subcommand    -s N                  -d "Run sanity test"
complete -c pkg -n __fish_pkg_subcommand -x -s j -l jail          -d "Run package manager within jail"
complete -c pkg -n __fish_pkg_subcommand -r -s c -l chroot        -d "Run package manager within chroot"
complete -c pkg -n __fish_pkg_subcommand -r -s r -l rootdir       -d "Install packages in specified root"
complete -c pkg -n __fish_pkg_subcommand -r -s C -l config        -d "Use configuration file"
complete -c pkg -n __fish_pkg_subcommand -r -s R -l repo-conf-dir -d "Set repository configuration directory"
complete -c pkg -n __fish_pkg_subcommand    -s 4                  -d "Use IPv4"
complete -c pkg -n __fish_pkg_subcommand    -s 6                  -d "Use IPv6"

complete -c pkg -n __fish_pkg_subcommand -xa help       -d "Display help for command"
complete -c pkg -n __fish_pkg_subcommand -xa add        -d "Install package file"
complete -c pkg -n __fish_pkg_subcommand -xa annotate   -d "Modify annotations on packages"
complete -c pkg -n __fish_pkg_subcommand -xa audit      -d "Audit installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa autoremove -d "Delete unneeded packages"
complete -c pkg -n __fish_pkg_subcommand -xa backup     -d "Dump package database"
complete -c pkg -n __fish_pkg_subcommand -xa check      -d "Check installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa clean      -d "Clean local cache"
complete -c pkg -n __fish_pkg_subcommand -xa convert    -d "Convert package from pkg_add format"
complete -c pkg -n __fish_pkg_subcommand -xa create     -d "Create a package"
complete -c pkg -n __fish_pkg_subcommand -xa delete     -d "Remove a package"
complete -c pkg -n __fish_pkg_subcommand -xa fetch      -d "Download a remote package"
complete -c pkg -n __fish_pkg_subcommand -xa info       -d "List installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa install    -d "Install packages"
complete -c pkg -n __fish_pkg_subcommand -xa lock       -d "Prevent package modification"
complete -c pkg -n __fish_pkg_subcommand -xa plugins    -d "List package manager plugins"
complete -c pkg -n __fish_pkg_subcommand -xa query      -d "Query installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa register   -d "Register a package in a database"
complete -c pkg -n __fish_pkg_subcommand -xa remove     -d "Remove a package"
complete -c pkg -n __fish_pkg_subcommand -xa repo       -d "Create package repository"
complete -c pkg -n __fish_pkg_subcommand -xa rquery     -d "Query information for remote repositories"
complete -c pkg -n __fish_pkg_subcommand -xa search     -d "Find packages"
complete -c pkg -n __fish_pkg_subcommand -xa set        -d "Modify package information in database"
complete -c pkg -n __fish_pkg_subcommand -xa shell      -d "Open a SQLite shell"
complete -c pkg -n __fish_pkg_subcommand -xa shlib      -d "Display packages linking to shared library"
complete -c pkg -n __fish_pkg_subcommand -xa stats      -d "Display package statistics"
complete -c pkg -n __fish_pkg_subcommand -xa unlock     -d "Stop preventing package modification"
complete -c pkg -n __fish_pkg_subcommand -xa update     -d "Update remote repositories"
complete -c pkg -n __fish_pkg_subcommand -xa updating   -d "Display UPDATING entries"
complete -c pkg -n __fish_pkg_subcommand -xa upgrade    -d "Upgrade packages"
complete -c pkg -n __fish_pkg_subcommand -xa version    -d "Show package versions"
complete -c pkg -n __fish_pkg_subcommand -xa which      -d "Check which package provides a file"

# add
complete -c pkg -n '__fish_pkg_is add install' -s A -l automatic -d "Mark packages as automatic"
complete -c pkg -n '__fish_pkg_is add install' -s f -l force -d "Force installation even when installed"
complete -c pkg -n '__fish_pkg_is add' -s I -l no-scripts -d "Disable installation scripts"
complete -c pkg -n '__fish_pkg_is add' -s M -l accept-missing -d "Force installation with missing dependencies"
complete -c pkg -n '__fish_pkg_is add autoremove clean delete remove install update' -s q -l quiet -d "Force quiet output"

# autoremove
complete -c pkg -n '__fish_pkg_is autoremove clean delete remove install upgrade' -s n -l dry-run -d "Do not make changes"
complete -c pkg -n '__fish_pkg_is autoremove clean delete remove install' -s y -l yes -d "Assume yes when asked for confirmation"

# clean
complete -c pkg -n '__fish_pkg_is clean' -s a -l all -d "Delete all cached packages"

# delete/remove
complete -c pkg -n '__fish_pkg_is delete remove upgrade' -xa '(pkg query "%n")'
complete -c pkg -n '__fish_pkg_is delete remove' -s a -l all -d 'Delete all installed packages'
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s C -l case-sensitive -d "Case sensitive packages"
complete -c pkg -n '__fish_pkg_is delete remove' -s D -l no-deinstall-script -d "Disable deinstallation scripts"
complete -c pkg -n '__fish_pkg_is delete remove' -s f -l force -d "Force package removal"
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s g -l glob -d "Treat the package name as shell glob"
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s i -l case-insensitive -d "Case insensitive packages"
complete -c pkg -n '__fish_pkg_is delete remove' -s R -l recursive -d "Remove recursively"
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s x -l regex -d "Treat the package name as regular expression"

# install
complete -c pkg -n '__fish_pkg_is install' -xa '(pkg rquery -U "%n")'
complete -c pkg -n '__fish_pkg_is install' -s I -l no-install-scripts -d "Disable installation scripts"
complete -c pkg -n '__fish_pkg_is install' -s M -l ignore-missing -d "Force installation with missing dependencies"
complete -c pkg -n '__fish_pkg_is install upgrade' -s F -l fetch-only -d "Do not perform actual installation"
complete -c pkg -n '__fish_pkg_is install' -s R -l from-root -d "Reinstall packages required by this package"
complete -c pkg -n '__fish_pkg_is install update' -x -s r -l repository -d "Use only a given repository"
complete -c pkg -n '__fish_pkg_is install upgrade' -s U -l no-repo-update -d "Do not automatically update database"

# update
complete -c pkg -n '__fish_pkg_is add update' -s f -l force -d "Force a full download of a repository"
