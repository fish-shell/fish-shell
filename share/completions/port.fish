#completion for port

set -l subcommands activate archive build cat cd checksum clean configure \
    contents deactivate dependents deps destroot dir distcheck dmg echo \
    edit extract fetch file gohome info install installed lint list \
    livecheck location load log logfile mirror mdmg mpkg notes outdated \
    patch pkg provides rdependents rdeps reload rev search select \
    selfupdate setrequested setunrequested sync test unarchive uninstall \
    unload unsetrequested upgrade url usage variants version work

complete -c port -n "__fish_seen_subcommand_from $subcommands" -a '(__fish_print_port_packages)' -d Package

complete -f -n "__fish_use_subcommand $subcommands" -c port -a activate -d 'Set  version of a port to active'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a archive -d "Create image for port without installing"
complete -f -n "__fish_use_subcommand $subcommands" -c port -a build -d 'Run build phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a cat -d 'Print the Portfile of the given port(s)'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a cd -d 'Change directory to that containing portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a checksum -d 'Compute checksums of distribution files'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a clean -d 'Remove temporary files used to build a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a configure -d 'Run configure phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a contents -d 'List the files installed by a given port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a deactivate -d 'Set the status of a port to inactive'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a dependents -d 'List ports that depend on a given port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a deps -d 'Display a dependency listing for given port(s)'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a destroot -d 'Run destroot phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a dir -d 'print directory with Portfile for port expression'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a distcheck -d 'Check if port can be fetched from all mirrors'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a dmg -d 'Create binary archives of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a echo -d 'Print the list of ports the argument expands to'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a edit -d 'Open the Portfile in an editor'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a extract -d 'Run extract phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a fetch -d 'Run fetch phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a file -d 'Display the path to the Portfile for portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a help -d 'Get help on MacPorts commands'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a gohome -d 'Load home page for given portname in web browser'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a info -d 'Return information about the given ports'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a install -d 'Install a new port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a installed -d 'List installed versions (of port)'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a lint -d 'Verifies Portfile for portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a list -d 'List latest available version for given ports'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a livecheck -d 'Check if new version of software is available'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a location -d 'Print location of archive used for (de)activation'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a load -d "load a port's daemon"
complete -f -n "__fish_use_subcommand $subcommands" -c port -a log -d 'Parse and show log files for portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a logfile -d 'Display the path to the log file for portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a mirror -d 'Create/update local mirror of distfiles'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a mdmg -d 'Create disk image of portname and dependencies'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a mpkg -d 'Create binary archives of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a notes -d 'Displays notes for portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a outdated -d 'List outdated ports'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a patch -d 'Run patch phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a pkg -d 'Create binary archives of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a provides -d 'Find the port that installed a file'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a rdependents -d 'Recursively list ports depending on given port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a rdeps -d 'Display a recursive dependency listing of port(s)'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a reload -d "reload a port's daemon"
complete -f -n "__fish_use_subcommand $subcommands" -c port -a rev-upgrade -d 'Rebuild ports containing broken binaries'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a search -d 'Search for a port using keywords'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a select -d 'Selects a version to be the default'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a selfupdate -d 'Upgrade MacPorts and update list of ports'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a setrequested -d 'Mark portname as requested'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a setunrequested -d 'Mark portname as unrequested'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a sync -d 'Update the port definition files'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a test -d 'Run test phase of a port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a unarchive -d 'Extract destroot of given ports from archive'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a uninstall -d 'Remove a previously installed port'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a unload -d "Unload a port's daemon"
complete -f -n "__fish_use_subcommand $subcommands" -c port -a unsetrequested -d 'Mark portname as unrequested'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a upgrade -d 'Upgrade a port to the latest version'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a url -d 'Display URL for path of given portname'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a usage -d 'Displays a condensed usage summary'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a variants -d 'Print list of variants with descriptions'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a version -d 'Print the MacPorts version'
complete -f -n "__fish_use_subcommand $subcommands" -c port -a work -d 'Displays path to work directory for portname'

complete -c port -s v -d 'Verbose mode, generates verbose messages'
complete -c port -s d -d 'Debug mode, implies -v'
complete -c port -s q -d 'Quiet mode, implies -N'
complete -c port -s N -d 'Non-interactive mode'
complete -c port -s n -d "Don't follow dependencies in upgrade"
complete -c port -s R -d 'Also upgrade dependents (only for upgrade)'
complete -c port -s u -d 'Uninstall inactive ports'
complete -c port -s y -d 'Perform a dry run'
complete -c port -s s -d 'Source-only mode'
complete -c port -s b -d 'Binary-only mode, abort if no archive available'
complete -c port -s c -d 'Autoclean mode, execute clean after install'
complete -c port -s k -d 'Keep mode, do not autoclean after install'
complete -c port -s p -d 'Proceed to process despite any errors'
complete -c port -s o -d 'Honor state files even if Portfile was modified'
complete -c port -s t -d 'Enable trace mode debug facilities'
complete -c port -s f -d 'Force mode, ignore state file'
complete -c port -s D -d 'Specify portdir'
complete -c port -s F -d 'Read and process file of commands'
