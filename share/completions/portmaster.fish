#completion for portmaster

# These completions are generated from the man page
complete -c portmaster -l force-config -d 'run \'make config\' for all ports (overrides -G).'
complete -c portmaster -s C -d 'prevents \'make clean\' from being run before building.'
complete -c portmaster -s G -d 'prevents \'make config\'.'
complete -c portmaster -s H -d 'hide details of the port build and install in a log file.'
complete -c portmaster -s K -d 'prevents \'make clean\' from being run after building.'
complete -c portmaster -s B -d 'prevents creation of the backup package for the installed port.'
complete -c portmaster -s b -d 'create and keep a backup package of an installed port.'
complete -c portmaster -s g -d 'create a package of the new port.'
complete -c portmaster -s n -d 'run through all steps, but do not make or install any ports.'
complete -c portmaster -s t -d 'recurse dependencies thoroughly, using all-depends-list.'
complete -c portmaster -s v -d 'verbose output.'
complete -c portmaster -s w -d 'save old shared libraries before deinstall [-R]… [See Man Page]'
complete -c portmaster -s i -d 'interactive update mode -- ask whether to rebuild ports.'
complete -c portmaster -s D -d 'no cleaning of distfiles.'
complete -c portmaster -s d -d 'always clean distfiles.'
complete -c portmaster -s m -d 'any arguments to supply to make 1.'
complete -c portmaster -s x -d 'avoid building or updating ports that match this pattern.'
complete -c portmaster -l no-confirm -d 'do not ask the user to confirm the list of port… [See Man Page]'
complete -c portmaster -l no-term-title -d 'do not update the xterm title bar.'
complete -c portmaster -l no-index-fetch -d 'skip fetching the INDEX file.'
complete -c portmaster -l index -d 'use INDEX-[7-9] exclusively to check if a port is up to date.'
complete -c portmaster -l index-first -d 'use the INDEX for status, but double-check with the port.'
complete -c portmaster -l index-only -d 'do not try to use /usr/ports.'
complete -c portmaster -l delete-build-only -d 'delete ports that are build-only dependencies a… [See Man Page]'
complete -c portmaster -l update-if-newer -d '(only for multiple ports listed on the command … [See Man Page]'
complete -c portmaster -s P -l packages -d 'use packages, but build port if not available.'
complete -c portmaster -o PP -l packages-only -d 'fail if no package is available.'
complete -c portmaster -l packages-build -d 'use packages for all build dependencies.'
complete -c portmaster -l packages-if-newer -d 'use package if newer than installed even if the… [See Man Page]'
complete -c portmaster -l always-fetch -d 'fetch package even if it already exists locally.'
complete -c portmaster -l local-packagedir -d 'where local packages can be found, will fall ba… [See Man Page]'
complete -c portmaster -l packages-local -d 'use packages from -local-packagedir only.'
complete -c portmaster -l delete-packages -d 'after installing from a package, delete it El P… [See Man Page]'
complete -c portmaster -s a -d 'check all ports, update as necessary.'
complete -c portmaster -l show-work -d 'show what dependent ports are, and are not inst… [See Man Page]'
complete -c portmaster -s o -d 'replace the installed port with a port from a d… [See Man Page]'
complete -c portmaster -s R -d 'used with the r or f options to skip ports upda… [See Man Page]'
complete -c portmaster -s l -d 'list all installed ports by category.'
complete -c portmaster -s L -d 'list all installed ports by category, and search for updates.'
complete -c portmaster -l list-origins -d 'list directories from /usr/ports for root and leaf ports.'
complete -c portmaster -s y -d 'answer yes to all user prompts for the features… [See Man Page]'
complete -c portmaster -s h -l help -d 'display help message.'
complete -c portmaster -l version -d 'display the version number El ENVIRONMENT The d… [See Man Page]'

# Grab items from the ports directory, max depth 2
complete -c portmaster -f -d 'Ports Directory' -a "
(
	string match -r '(?<=/usr/ports/)[^/]*(?:/[^/]*)?' (__fish_complete_directories /usr/ports/(commandline -ct))
)"

complete -c portmaster -f -d 'Installed package' -a "(__fish_print_port_packages)"
