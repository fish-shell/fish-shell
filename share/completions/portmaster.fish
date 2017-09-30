#completion for portmaster

# These completions are generated from the man page
complete -c portmaster -l force-config --description 'run \'make config\' for all ports (overrides -G).'
complete -c portmaster -s C --description 'prevents \'make clean\' from being run before building.'
complete -c portmaster -s G --description 'prevents \'make config\'.'
complete -c portmaster -s H --description 'hide details of the port build and install in a log file.'
complete -c portmaster -s K --description 'prevents \'make clean\' from being run after building.'
complete -c portmaster -s B --description 'prevents creation of the backup package for the installed port.'
complete -c portmaster -s b --description 'create and keep a backup package of an installed port.'
complete -c portmaster -s g --description 'create a package of the new port.'
complete -c portmaster -s n --description 'run through all steps, but do not make or install any ports.'
complete -c portmaster -s t --description 'recurse dependencies thoroughly, using all-depends-list.'
complete -c portmaster -s v --description 'verbose output.'
complete -c portmaster -s w --description 'save old shared libraries before deinstall [-R]… [See Man Page]'
complete -c portmaster -s i --description 'interactive update mode -- ask whether to rebuild ports.'
complete -c portmaster -s D --description 'no cleaning of distfiles.'
complete -c portmaster -s d --description 'always clean distfiles.'
complete -c portmaster -s m --description 'any arguments to supply to make 1.'
complete -c portmaster -s x --description 'avoid building or updating ports that match this pattern.'
complete -c portmaster -l no-confirm --description 'do not ask the user to confirm the list of port… [See Man Page]'
complete -c portmaster -l no-term-title --description 'do not update the xterm title bar.'
complete -c portmaster -l no-index-fetch --description 'skip fetching the INDEX file.'
complete -c portmaster -l index --description 'use INDEX-[7-9] exclusively to check if a port is up to date.'
complete -c portmaster -l index-first --description 'use the INDEX for status, but double-check with the port.'
complete -c portmaster -l index-only --description 'do not try to use /usr/ports.'
complete -c portmaster -l delete-build-only --description 'delete ports that are build-only dependencies a… [See Man Page]'
complete -c portmaster -l update-if-newer --description '(only for multiple ports listed on the command … [See Man Page]'
complete -c portmaster -s P -l packages --description 'use packages, but build port if not available.'
complete -c portmaster -o PP -l packages-only --description 'fail if no package is available.'
complete -c portmaster -l packages-build --description 'use packages for all build dependencies.'
complete -c portmaster -l packages-if-newer --description 'use package if newer than installed even if the… [See Man Page]'
complete -c portmaster -l always-fetch --description 'fetch package even if it already exists locally.'
complete -c portmaster -l local-packagedir --description 'where local packages can be found, will fall ba… [See Man Page]'
complete -c portmaster -l packages-local --description 'use packages from -local-packagedir only.'
complete -c portmaster -l delete-packages --description 'after installing from a package, delete it El P… [See Man Page]'
complete -c portmaster -s a --description 'check all ports, update as necessary.'
complete -c portmaster -l show-work --description 'show what dependent ports are, and are not inst… [See Man Page]'
complete -c portmaster -s o --description 'replace the installed port with a port from a d… [See Man Page]'
complete -c portmaster -s R --description 'used with the r or f options to skip ports upda… [See Man Page]'
complete -c portmaster -s l --description 'list all installed ports by category.'
complete -c portmaster -s L --description 'list all installed ports by category, and search for updates.'
complete -c portmaster -l list-origins --description 'list directories from /usr/ports for root and leaf ports.'
complete -c portmaster -s y --description 'answer yes to all user prompts for the features… [See Man Page]'
complete -c portmaster -s h -l help --description 'display help message.'
complete -c portmaster -l version --description 'display the version number El ENVIRONMENT The d… [See Man Page]'

# Grab items from the ports directory, max depth 2
complete -c portmaster -f --description 'Ports Directory' -a "
(
	string match -r '(?<=/usr/ports/)[^/]*(?:/[^/]*)?' (__fish_complete_directories /usr/ports/(commandline -ct))
)"

complete -c portmaster -f --description 'Installed Package' -a "(__fish_print_packages)"
