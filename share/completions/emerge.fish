#Completions for emerge

function __fish_emerge_use_package -d (_ 'Test if emerge command should have packages as potential completion')
	for i in (commandline -opc)
		if contains -- $i -a --ask -p --pretend --oneshot -O --nodeps -f --fetchonly
			return 0
		end
	end
	return 1
end

function __fish_emerge_manipulate_package -d (_ 'Tests if emerge command should have package as potential completion for removal')
	for i in (commandline -opc)
		if contains -- $i -u --update -C --unmerge -P --prune
			return 0
		end
	end
	return 1
end

function __fish_emerge_print_installed_pkgs -d (_ 'Prints completions for installed packages on the system from /var/db/pkg')
	if test -d /var/db/pkg 
		find /var/db/pkg/ -type d | cut -d'/' -f6 | sort | uniq | sed 's/-[0-9]\{1,\}\..*$//'
		return
	end
end

complete -f -c emerge -n '__fish_emerge_use_package' -a '(__fish_print_packages)' -d (_ 'Package')
complete -f -c emerge -n '__fish_emerge_manipulate_package' -a '(__fish_emerge_print_installed_pkgs)' -d (_ 'Package')
complete -c emerge -s h -l help -d (_ "Display help and exit")
complete -c emerge -s c -l clean -d (_ "Cleans the system by removing outdated packages")
complete -c emerge -l depclean -d (_ "Cleans the system by removing packages that are not associated with explicitly merged packages")
complete -c emerge -l info -d (_ "Displays important portage variables that will be exported to ebuild.sh when performing merges")
complete -c emerge -l metadata -d (_ "Causes portage to process all the metacache files as is normally done on the tail end of an rsync update using emerge --sync")
complete -c emerge -s P -l prune -d (_ "Removes all but the most recently installed version of a package from your system")
complete -c emerge -l regen -d (_ "Causes portage to check and update the dependency cache of all ebuilds in the portage tree")
complete -c emerge -s s -l search -d (_ "Searches for matches of the supplied string in the current local portage tree")
complete -c emerge -s C -l unmerge -d (_ "Removes all matching packages completely from your system")
complete -c emerge -s a -l ask -d (_ "Before performing the merge, display what ebuilds and tbz2s will be installed, in the same format as when using --pretend")
complete -c emerge -s b -l buildpkg -d (_ "Tell emerge to build binary packages for all ebuilds processed in addition to actually merging the packages")
complete -c emerge -s B -l buildpkgonly -d (_ "Creates a binary package, but does not merge it to the system")
complete -c emerge -s l -l changelog -d (_ "When pretending, also display the ChangeLog entries for packages that will be upgraded")
complete -c emerge -l columns -d (_ "Display the pretend output in a tabular form")
complete -c emerge -s d -l debug -d (_ "Tell emerge to run the ebuild command in --debug mode")
complete -c emerge -s D -l deep -d (_ "When used in conjunction with --update, this flag forces emerge to consider the entire dependency tree of packages, instead of checking only the immediate dependencies of the packages")
complete -c emerge -s e -l emptytree -d (_ "Virtually tweaks the tree of installed packages to contain nothing")
complete -c emerge -s f -l fetchonly -d (_ "Instead of doing any package building, just perform fetches for all packages (main package as well as all dependencies)")
complete -c emerge -l fetch-all-uri -d (_ "Same as --fetchonly except that all package files, including those not required to build the package, will be processed")
complete -c emerge -s g -l getbinpkg -d (_ "Using the server and location defined in PORTAGE_BINHOST, portage will download the information from each binary file there and it will use that information to help build the dependency list")
complete -c emerge -s G -l getbinpkgonly -d (_ "This option is identical to -g, except it will not use ANY information from the local machine")
complete -c emerge -s N -l newuse -d (_ "Tells emerge to include installed packages where USE flags have changed since installation")
complete -c emerge -l noconfmem -d (_ "Merge files in CONFIG_PROTECT to the live fs instead of silently dropping them")
complete -c emerge -s O -l nodeps -d (_ "Merge specified packages, but don't merge any dependencies")
complete -c emerge -s n -l noreplace -d (_ "Skip the packages specified on the command-line that have already been installed")
complete -c emerge -l nospinner -d (_ "Disables the spinner regardless of terminal type")
complete -c emerge -l oneshot -d (_ "Emerge as normal, but don't add packages to the world profile")
complete -c emerge -s o -l onlydeps -d (_ "Only merge (or pretend to merge) the dependencies of the specified packages, not the packages themselves")
complete -c emerge -s p -l pretend -d (_ "Do not merge, display what ebuilds and tbz2s would have been installed")
complete -c emerge -s q -l quiet -d (_ "Reduced output from portage's displays")
complete -c emerge -l resume -d (_ "Resumes the last merge operation")
complete -c emerge -s S -l searchdesc -d (_ "Matches the search string against the description field as well the package's name")
complete -c emerge -l skipfirst -d (_ "Remove the first package in the resume list so that a merge may continue in the presence of an uncorrectable or inconsequential error")
complete -c emerge -s t -l tree -d (_ "Shows the dependency tree using indentation for dependencies")
complete -c emerge -s u -l update -d (_ "Updates packages to the best version available")
complete -c emerge -s k -l usepkg -d (_ "Tell emerge to use binary packages (from $PKGDIR) if they are available, thus possibly avoiding some time-consuming compiles")
complete -c emerge -s K -l usepkgonly -d (_ "Like --usepkg, except this only allows the use of binary packages, and it will abort the emerge if the package is not available at the time of dependency calculation")
complete -c emerge -s v -l verbose -d (_ "Increased or expanded display of content in portage's displays")
complete -c emerge -s V -l version -d (_ "Displays the currently installed version of portage along with other information useful for quick reference on a system")
