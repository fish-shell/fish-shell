function __fish_wajig_no_subcommand -d 'Test if wajig has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i addcdrom auto-alts auto-clean auto-download auto-install \
                available bug build build-depend changelog clean commands daily-upgrade \
                dependents describe describe-new detail detail-new dist-upgrade docs download \
                file-download file-install file-remove find-file find-pkg fix-configure \
                fix-install fix-missing force help hold init install installr installrs \
                installs integrity large last-update list list-all list-alts list-cache \
                list-commands list-daemons list-files list-hold list-installed list-log \
                list-names list-orphans list-scripts list-section list-section list-status \
                list-wide local-dist-upgrade local-upgrade madison move new news new-upgrades \
                non-free orphans package policy purge purge-depend purge-orphans readme \
                recursive recommended reconfigure reinstall reload remove remove-depend \
                remove-orphans repackage reset restart rpminstall rpmtodeb search search-apt \
                setup show showdistupgrade showinstall showremove showupgrade size sizes \
                snapshot source start status status-match status-search stop suggested \
                tasksel toupgrade unhold unofficial update update-alts update-pci-ids \
                update-usb-ids upgrade versions whatis whichpkg
            return 1
        end
    end
    return 0
end

function __fish_wajig_use_package -d 'Test if wajig command should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i contains bug build build-depend changelog dependents describe \
                detail hold install installr installrs installs list list-files news \
                package purge purge-depend readme recursive recommended reconfigure \
                reinstall remove remove-depend repackage show showinstall showremove \
                showupgrade size sizes source suggested unhold upgrade versions whatis
            return 0
        end
    end
    return 1
end

complete -c wajig -n __fish_wajig_use_package -a '(__fish_print_apt_packages)' -d Packages
complete -c wajig -s q -l quiet -d 'Do system commands everything quietly.'
complete -c wajig -s n -l noauth -d 'Allow packages from unathenticated archives.'
complete -c wajig -s s -l simulate -d 'Trace but don\'t execute the sequence of underlying commands.'
complete -c wajig -s t -l teaching -d 'Trace the sequence of commands performed.'
complete -c wajig -s y -l yes -d 'Assume yes for any questions asked.'
complete -f -n __fish_wajig_no_subcommand -c wajig -a addcdrom -d 'Add a CD-ROM to the list of available sources of packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a auto-alts -d 'Mark the alternative to be auto set (using set priorities)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a auto-clean -d 'Remove superseded deb files from the download cache'
complete -f -n __fish_wajig_no_subcommand -c wajig -a auto-download -d 'Do an update followed by a download of all updated packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a auto-install -d 'Perform an install without asking questions (non-interactive)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a available -d 'List versions of packages available for installation'
complete -f -n __fish_wajig_no_subcommand -c wajig -a bug -d 'Check reported bugs in package using the Debian Bug Tracker'
complete -f -n __fish_wajig_no_subcommand -c wajig -a build -d 'Retrieve/unpack sources and build .deb for the named packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a build-depend -d 'Retrieve packages required to build listed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a changelog -d 'Retrieve latest changelog for the package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a clean -d 'Remove all deb files from the download cache'
complete -f -n __fish_wajig_no_subcommand -c wajig -a commands -d 'List all the JIG commands and one line descriptions for each'
complete -f -n __fish_wajig_no_subcommand -c wajig -a daily-upgrade -d 'Perform an update then a dist-upgrade'
complete -f -n __fish_wajig_no_subcommand -c wajig -a dependents -d 'List of packages which depend/recommend/suggest the package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a describe -d 'One line description of packages (-v and -vv for more detail)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a describe-new -d 'One line description of new packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a detail -d 'Provide a detailed description of package (describe -vv)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a detail-new -d 'Provide a detailed description of new packages (describe -vv)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a dist-upgrade -d 'Upgrade to new distribution (installed and new rqd packages)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a docs -d 'Equivalent to help with -verbose=2'
complete -f -n __fish_wajig_no_subcommand -c wajig -a download -d 'Download package files ready for an install'
complete -f -n __fish_wajig_no_subcommand -c wajig -a file-download -d 'Download packages listed in file ready for an install'
complete -f -n __fish_wajig_no_subcommand -c wajig -a file-install -d 'Install packages listed in a file'
complete -f -n __fish_wajig_no_subcommand -c wajig -a file-remove -d 'Remove packages listed in a file'
complete -f -n __fish_wajig_no_subcommand -c wajig -a find-file -d 'Search for a file within installed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a find-pkg -d 'Search for an unofficial Debian package at apt-get.org'
complete -f -n __fish_wajig_no_subcommand -c wajig -a fix-configure -d 'Perform dpkg --configure -a (to fix interrupted configure)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a fix-install -d 'Perform apt-get -f install (to fix broken dependencies)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a fix-missing -d 'Perform apt-get --fix-missing upgrade'
complete -f -n __fish_wajig_no_subcommand -c wajig -a force -d 'Install packages and ignore file overwrites and depends'
complete -f -n __fish_wajig_no_subcommand -c wajig -a help -d 'Print documentation (detail depends on --verbose)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a hold -d 'Place listed packages on hold so they are not upgraded'
complete -f -n __fish_wajig_no_subcommand -c wajig -a init -d 'Initialise or reset the JIG archive files'
complete -f -n __fish_wajig_no_subcommand -c wajig -a install -d 'Install (or upgrade) one or more packages or .deb files'
complete -f -n __fish_wajig_no_subcommand -c wajig -a installr -d 'Install package and associated recommended packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a installrs -d 'Install package and recommended and suggested packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a installs -d 'Install package and associated suggested packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a integrity -d 'Check the integrity of installed packages (through checksums)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a large -d 'List size of all large (>10MB) installed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a last-update -d 'Identify when an update was last performed'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list -d 'List the status and description of installed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-all -d 'List a one line description of given or all packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-alts -d 'List the objects that can have alternatives configured'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-cache -d 'List the contents of the download cache'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-commands -d 'List all the JIG commands and one line descriptions for each'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-daemons -d 'List the daemons that JIG can start/stop/restart'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-files -d 'List the files that are supplied by the named package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-hold -d 'List those packages on hold'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-installed -d 'List packages (with optional argument substring) installed'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-log -d 'List the contents of the install/remove log file (filtered)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-names -d 'List all known packages or those containing supplied string'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-orphans -d 'List libraries not required by any installed package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-scripts -d 'List the control scripts of the package of deb file'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-section -d 'List packages that belong to a specific section'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-section -d 'List the sections that are available'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-status -d 'Same as list but only prints first two columns, not truncated'
complete -f -n __fish_wajig_no_subcommand -c wajig -a list-wide -d 'Same as list but avoids truncating package names'
complete -f -n __fish_wajig_no_subcommand -c wajig -a local-dist-upgrade -d 'Dist-upgrade using packages already downloaded'
complete -f -n __fish_wajig_no_subcommand -c wajig -a local-upgrade -d 'Upgrade using packages already downloaded, but not any others'
complete -f -n __fish_wajig_no_subcommand -c wajig -a madison -d 'Runs the madison command of apt-cache.'
complete -f -n __fish_wajig_no_subcommand -c wajig -a move -d 'Move packages in the download cache to a local Debian mirror'
complete -f -n __fish_wajig_no_subcommand -c wajig -a new -d 'List packages that became available since last update'
complete -f -n __fish_wajig_no_subcommand -c wajig -a news -d 'Obtain the latest news about the package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a new-upgrades -d 'List packages newly available for upgrading'
complete -f -n __fish_wajig_no_subcommand -c wajig -a non-free -d 'List installed packages that do not meet the DFSG'
complete -f -n __fish_wajig_no_subcommand -c wajig -a orphans -d 'List libraries not required by any installed package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a package -d 'Generate a .deb file for an installed package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a policy -d 'From preferences file show priorities/policy (available)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a purge -d 'Remove one or more packages and configuration files'
complete -f -n __fish_wajig_no_subcommand -c wajig -a purge-depend -d 'Purge package and those it depend on and not required by others'
complete -f -n __fish_wajig_no_subcommand -c wajig -a purge-orphans -d 'Purge orphaned libraries (not required by installed packages)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a readme -d 'Display the package\'s README file from /usr/share/doc'
complete -f -n __fish_wajig_no_subcommand -c wajig -a recursive -d 'Download package and any packages it depends on'
complete -f -n __fish_wajig_no_subcommand -c wajig -a recommended -d 'Install package and associated recommended packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a reconfigure -d 'Reconfigure the named installed packages or run gkdebconf'
complete -f -n __fish_wajig_no_subcommand -c wajig -a reinstall -d 'Reinstall each of the named packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a reload -d 'Reload daemon configs, e.g., gdm, apache (see list-daemons)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a remove -d 'Remove one or more packages (see also purge)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a remove-depend -d 'Remove package and its dependees not required by others'
complete -f -n __fish_wajig_no_subcommand -c wajig -a remove-orphans -d 'Remove orphaned libraries (not required by installed packages)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a repackage -d 'Generate a .deb file for an installed package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a reset -d 'Initialise or reset the JIG archive files'
complete -f -n __fish_wajig_no_subcommand -c wajig -a restart -d 'Stop then start a daemon, e.g., gdm, apache (see list-daemons)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a rpminstall -d 'Install a RedHat .rpm package'
complete -f -n __fish_wajig_no_subcommand -c wajig -a rpmtodeb -d 'Convert a RedHat .rpm file to a Debian .deb file'
complete -f -n __fish_wajig_no_subcommand -c wajig -a search -d 'Search for packages containing listed words'
complete -f -n __fish_wajig_no_subcommand -c wajig -a search-apt -d 'Find local Debian archives suitable for sources.list'
complete -f -n __fish_wajig_no_subcommand -c wajig -a setup -d 'Configure the sources.list file which locates Debian archives'
complete -f -n __fish_wajig_no_subcommand -c wajig -a show -d 'Provide a detailed description of package [same as detail]'
complete -f -n __fish_wajig_no_subcommand -c wajig -a showdistupgrade -d 'Trace the steps that a dist-upgrade would perform'
complete -f -n __fish_wajig_no_subcommand -c wajig -a showinstall -d 'Trace the steps that an install would perform'
complete -f -n __fish_wajig_no_subcommand -c wajig -a showremove -d 'Trace the steps that a remove would perform'
complete -f -n __fish_wajig_no_subcommand -c wajig -a showupgrade -d 'Trace the steps that an upgrade would perform'
complete -f -n __fish_wajig_no_subcommand -c wajig -a size -d 'Print out the size (in K) of all, or listed, installed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a sizes -d 'Print out the size (in K) of all, or listed, installed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a snapshot -d 'Generates list of package=version for all installed packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a source -d 'Retrieve and unpack sources for the named packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a start -d 'Start a daemon, e.g., gdm, apache (see list-daemons)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a status -d 'Show the version and available version of packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a status-match -d 'Show the version and available version of matching packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a status-search -d 'Show the version and available version of matching packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a stop -d 'Stop a daemon, e.g., gdm, apache (see list-daemons)'
complete -f -n __fish_wajig_no_subcommand -c wajig -a suggested -d 'Install package and associated suggested packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a tasksel -d 'Run the Gnome task selector to install groups of packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a toupgrade -d 'List packages with newer versions available for upgrading'
complete -f -n __fish_wajig_no_subcommand -c wajig -a unhold -d 'Remove listed packages from hold so they are again upgraded'
complete -f -n __fish_wajig_no_subcommand -c wajig -a unofficial -d 'Search for an unofficial Debian package at apt-get.org'
complete -f -n __fish_wajig_no_subcommand -c wajig -a update -d 'Update the list of down-loadable packages'
complete -f -n __fish_wajig_no_subcommand -c wajig -a update-alts -d 'Update default alternative for things like x-window-manager'
complete -f -n __fish_wajig_no_subcommand -c wajig -a update-pci-ids -d 'Updates the local list of PCI ids from the internet master list'
complete -f -n __fish_wajig_no_subcommand -c wajig -a update-usb-ids -d 'Updates the local list of USB ids from the internet master list'
complete -f -n __fish_wajig_no_subcommand -c wajig -a upgrade -d 'Upgrade all of the installed packages or just those listed'
complete -f -n __fish_wajig_no_subcommand -c wajig -a versions -d 'List version and distribution of (all) packages.'
complete -f -n __fish_wajig_no_subcommand -c wajig -a whatis -d 'A synonym for describe'
complete -f -n __fish_wajig_no_subcommand -c wajig -a whichpkg -d 'Find the package that supplies the given command or file'
