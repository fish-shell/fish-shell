#completion for port

function __fish_port_no_subcommand --description 'Test if port has yet to be given the subcommand'
	for i in (commandline -opc)
		if contains -- $i activate archive build cat cd checksum clean configure \
				contents deactivate dependents deps destroot dir distcheck dmg echo \
				edit extract fetch file gohome info install installed lint list \
				livecheck location load log logfile mirror mdmg mpkg notes outdated \
				patch pkg provides rdependents rdeps reload rev search select \
				selfupdate setrequested setunrequested sync test unarchive uninstall \
				unload unsetrequested upgrade url usage variants version work
			return 1
		end
	end
	return 0
end

function __fish_port_use_package --description 'Test if port command should have packages as potential completion'
	for i in (commandline -opc)
	if contains -- $i activate archive build cat cd checksum clean configure \
			contents deactivate dependents deps destroot dir distcheck dmg echo \
			edit extract fetch file gohome info install installed lint list \
			livecheck location load log logfile mirror mdmg mpkg notes outdated \
			patch pkg provides rdependents rdeps reload rev search select \
			selfupdate setrequested setunrequested sync test unarchive uninstall \
			unload unsetrequested upgrade url usage variants version work
		return 0
		end
	end
	return 1
end

# TODO: export to __fish_print_packages
function __fish_print_port_packages
    switch (commandline -ct)
        case '-**'
            return
    end

    #Get the word 'Package' in the current language
    set -l package (_ "Package")

    # Set up cache directory
    if test -z "$XDG_CACHE_HOME"
        set XDG_CACHE_HOME $HOME/.cache
    end
    mkdir -m 700 -p $XDG_CACHE_HOME

		if type -q -f port

        set cache_file $XDG_CACHE_HOME/.port-cache.$USER
        if test -e $cache_file
						# Delete if cache is older than 15 minutes
            find "$cache_file" -ctime +15m | awk '{$1=$1;print}' | xargs rm
            if test -f $cache_file
								cat $cache_file
                return
            end
        end

        # Remove package version information from output and pipe into cache file
        port echo all | awk '{$1=$1};1' >$cache_file
				cat $cache_file
				echo "all current active inactive installed uninstalled outdated" >>$cache_file
        return
    end
end

complete -c port -n '__fish_port_use_package' -a '(__fish_print_port_packages)' --description 'Package'

complete -f -n '__fish_port_no_subcommand' -c port -a 'activate'  --description 'Set the status of an previously installed version of a port to active'
complete -f -n '__fish_port_no_subcommand' -c port -a 'archive'  --description "Create the port image (also called archive) for a port but will not actually install the port's files"
complete -f -n '__fish_port_no_subcommand' -c port -a 'build'  --description 'Run build phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'cat'  --description 'Print the Portfile of the given port(s)'
complete -f -n '__fish_port_no_subcommand' -c port -a 'cd'  --description 'Changes the current working directory to the one containing portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'checksum'  --description 'Compute the checksums of the distribution files for portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'clean'  --description 'Remove temporary files used to build a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'configure'  --description 'Run configure phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'contents'  --description 'List the files installed by a given port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'deactivate'  --description 'Set the status of a port to inactive'
complete -f -n '__fish_port_no_subcommand' -c port -a 'dependents'  --description 'List ports that depend on a given (installed) port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'deps'  --description 'Display a dependency listing for the given port(s)'
complete -f -n '__fish_port_no_subcommand' -c port -a 'destroot'  --description 'Run destroot phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'dir'  --description 'prints the directory that contains the Portfile for the given port expression'
complete -f -n '__fish_port_no_subcommand' -c port -a 'distcheck'  --description 'Check if a port can be fetched from all of its mirrors'
complete -f -n '__fish_port_no_subcommand' -c port -a 'dmg'  --description 'Create binary archives of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'echo'  --description 'Print the list of ports the argument expands to'
complete -f -n '__fish_port_no_subcommand' -c port -a 'edit'  --description 'Open the Portfile in an editor'
complete -f -n '__fish_port_no_subcommand' -c port -a 'extract'  --description 'Run extract phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'fetch'  --description 'Run fetch phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'file'  --description 'Displays the path to the Portfile for portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'help'  --description 'Get help on MacPorts commands'
complete -f -n '__fish_port_no_subcommand' -c port -a 'gohome'  --description 'Loads the home page for the given portname in the default web browser'
complete -f -n '__fish_port_no_subcommand' -c port -a 'info'  --description 'Return information about the given ports'
complete -f -n '__fish_port_no_subcommand' -c port -a 'install'  --description 'Install a new port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'installed'  --description 'List installed versions of a given port, or all installed port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'lint'  --description 'Verifies Portfile for portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'list'  --description 'List the latest available version for the given ports'
complete -f -n '__fish_port_no_subcommand' -c port -a 'livecheck'  --description 'Check if a new version of the software is available'
complete -f -n '__fish_port_no_subcommand' -c port -a 'location'  --description 'Prints the location of the archive MacPorts internally uses to be able to deactivate and activate a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'load'  --description "Provides a shortcut to using launchctl to load a port's daemon"
complete -f -n '__fish_port_no_subcommand' -c port -a 'log'  --description 'Parses and shows log files for portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'logfile'  --description 'Displays the path to the log file for portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'mirror'  --description 'Create/update a local mirror of distfiles used for ports given on the command line'
complete -f -n '__fish_port_no_subcommand' -c port -a 'mdmg'  --description 'Creates an internet-enabled disk image containing an OS X metapackage of portname and its dependencies'
complete -f -n '__fish_port_no_subcommand' -c port -a 'mpkg'  --description 'Create binary archives of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'notes'  --description 'Displays notes for portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'outdated'  --description 'List outdated ports'
complete -f -n '__fish_port_no_subcommand' -c port -a 'patch'  --description 'Run patch phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'pkg'  --description 'Create binary archives of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'provides'  --description 'Find the port that installed a file'
complete -f -n '__fish_port_no_subcommand' -c port -a 'rdependents'  --description 'Recursively lists the installed ports that depend on the port portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'rdeps'  --description 'Recursively lists the other ports that are required to build and run portname'
complete -f -n '__fish_port_no_subcommand' -c port -a 'reload'  --description 'A shortcut to launchctl, like load and unload, but reloads the daemon'
complete -f -n '__fish_port_no_subcommand' -c port -a 'rev-upgrade'  --description 'Manually check for broken binaries and rebuild ports containing broken binaries'
complete -f -n '__fish_port_no_subcommand' -c port -a 'search'  --description 'Search for a port using keywords'
complete -f -n '__fish_port_no_subcommand' -c port -a 'select'  --description 'For a given group, selects a version to be the default by creating appropriate symbolic links'
complete -f -n '__fish_port_no_subcommand' -c port -a 'selfupdate'  --description 'Upgrade MacPorts itself and update the port definition files.'
complete -f -n '__fish_port_no_subcommand' -c port -a 'setrequested'  --description 'Mark portname as requested'
complete -f -n '__fish_port_no_subcommand' -c port -a 'setunrequested'  --description 'Mark portname as unrequested'
complete -f -n '__fish_port_no_subcommand' -c port -a 'sync'  --description 'Update the port definition files'
complete -f -n '__fish_port_no_subcommand' -c port -a 'test'  --description 'Run test phase of a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'unarchive'  --description 'Extract the destroot of the given ports from a binary archive'
complete -f -n '__fish_port_no_subcommand' -c port -a 'uninstall'  --description 'Remove a previously installed port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'unload'  --description 'A shortcut to launchctl, like load, but unloads the daemon'
complete -f -n '__fish_port_no_subcommand' -c port -a 'unsetrequested'  --description 'Mark portname as unrequested'
complete -f -n '__fish_port_no_subcommand' -c port -a 'upgrade'  --description 'Upgrade a port to the latest version'
complete -f -n '__fish_port_no_subcommand' -c port -a 'url'  --description 'Displays the URL for the path of the given portname, which can be passed as port-url'
complete -f -n '__fish_port_no_subcommand' -c port -a 'usage'  --description 'Displays a condensed usage summary'
complete -f -n '__fish_port_no_subcommand' -c port -a 'variants'  --description 'Print a list of variants with descriptions provided by a port'
complete -f -n '__fish_port_no_subcommand' -c port -a 'version'  --description 'Print the MacPorts version'
complete -f -n '__fish_port_no_subcommand' -c port -a 'work'  --description 'Displays the path to the work directory for portname'

complete -c apt-get -s  --description ''

complete -c apt-get -s v --description 'Verbose mode, generates verbose messages'
complete -c apt-get -s d --description 'Debug mode, generate debugging messages, implies -v'
complete -c apt-get -s q --description 'Quiet mode, implies -N'
complete -c apt-get -s N --description 'Non-interactive mode, interactive questions are not asked'
complete -c apt-get -s n --description "Don't follow dependencies in upgrade (affects upgrade and install)"
complete -c apt-get -s R --description 'Also upgrade dependents (only for upgrade)'
complete -c apt-get -s u --description 'Uninstall inactive ports when upgrading and uninstalling'
complete -c apt-get -s y --description ' Perform a dry run'
complete -c apt-get -s s --description 'Source-only mode'
complete -c apt-get -s b --description 'Binary-only mode, abort if no archive available'
complete -c apt-get -s c --description 'Autoclean mode, execute clean after install'
complete -c apt-get -s k --description 'Keep mode, do not autoclean after install'
complete -c apt-get -s p --description 'Despite any errors encountered, proceed to process multiple ports and commands'
complete -c apt-get -s o --description 'Honor state files even if the Portfile was modified'
complete -c apt-get -s t --description 'Enable trace mode debug facilities on platforms that support it'
complete -c apt-get -s f --description 'Force mode, ignore state file'
complete -c apt-get -s D --description 'Specfiy portdir'
complete -c apt-get -s F --description 'Read and process the file of commands specified by the argument'
