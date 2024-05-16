complete -c zef -f
complete -c zef -n __fish_is_first_token -s h -l help -f -d 'Display help text'
complete -c zef -n __fish_is_first_token -l version -f -d 'Detailed version information'
complete -c zef -n 'not __fish_is_first_token' -l config-path -r -F -d 'Load a specific Zef config file'
complete -c zef -n 'not __fish_is_first_token' -l error -d 'Verbosity level'
complete -c zef -n 'not __fish_is_first_token' -l warn -d 'Verbosity level'
complete -c zef -n 'not __fish_is_first_token' -l info -d 'Verbosity level (default)'
complete -c zef -n 'not __fish_is_first_token' -s v -l verbose -d 'Verbosity level'
complete -c zef -n 'not __fish_is_first_token' -l debug -d 'Verbosity level'

# Commands
complete -c zef -n __fish_use_subcommand -a install -d 'Install specific distributions by name or path'
complete -c zef -n __fish_use_subcommand -a uninstall -f -d 'Uninstall specific distributions'
complete -c zef -n __fish_use_subcommand -a test -d 'Run tests on a given module\'s path'
complete -c zef -n __fish_use_subcommand -a fetch -d 'Fetch and extract module\'s source'
complete -c zef -n __fish_use_subcommand -a build -d 'Run the Build.pm in a given module\'s path'
complete -c zef -n __fish_use_subcommand -a look -x -d 'Download a single module and change into its directory'
complete -c zef -n __fish_use_subcommand -a update -d 'Update package indexes for repositories'
complete -c zef -n __fish_use_subcommand -a upgrade -d 'Upgrade specific distributions (or all if no arguments)'
complete -c zef -n __fish_use_subcommand -a search -d 'Show a list of possible distribution candidates for the given terms'
complete -c zef -n __fish_use_subcommand -a info -d 'Show detailed distribution information'
complete -c zef -n __fish_use_subcommand -a browse -d 'Browse support urls (bugtracker etc)'
complete -c zef -n __fish_use_subcommand -a list -d 'List available distributions'
complete -c zef -n __fish_use_subcommand -a depends -d 'List full dependencies for a given identity'
complete -c zef -n __fish_use_subcommand -a rdepends -d 'List all distributions directly depending on a given identity'
complete -c zef -n __fish_use_subcommand -a locate -d 'Lookup installed module information by short-name, name-path, sha1'
complete -c zef -n __fish_use_subcommand -a smoke -d 'Run smoke testing on available modules'
complete -c zef -n __fish_use_subcommand -a nuke -d 'Delete directory/prefix containing matching configuration path or CURLI name'

# Shared options
complete -c zef -n '__fish_seen_subcommand_from install test build' -k -a '(__fish_complete_suffix)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l fetch
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l build
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l /build -d 'Skip the building phase'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l test
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l /test -d 'Skip the testing phase'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke depends rdepends' -l depends
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke depends rdepends' -l /depends -d 'Do not fetch runtime dependencies'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke depends rdepends' -l build-depends
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke depends rdepends' -l /build-depends -d 'Do not fetch build dependencies'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke depends rdepends' -l test-depends
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke depends rdepends' -l /test-depends -d 'Do not fetch test dependencies'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke test fetch build' -l force -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l force-resolve -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke fetch' -l force-fetch -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l force-extract -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke build' -l force-build -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke test' -l force-test -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l force-install -d 'Ignore errors'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke test fetch build' -x -l timeout -d 'Set a timeout (in seconds)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke fetch' -x -l fetch-timeout -d 'Set a timeout (in seconds)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -x -l extract-timeout -d 'Set a timeout (in seconds)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke build' -x -l build-timeout -d 'Set a timeout (in seconds)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke test' -x -l test-timeout -d 'Set a timeout (in seconds)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -x -l install-timeout -d 'Set a timeout (in seconds)'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke test fetch' -x -l degree -d 'Number of simultaneous jobs to process'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke fetch' -x -l fetch-degree -d 'Number of simultaneous jobs to process'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke test' -x -l test-degree -d 'Number of simultaneous jobs to process'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l dry -d 'Run all phases except the actual installations'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l serial -d 'Install each dependency after passing testing and before building/testing the next dependency'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke fetch search info list' -l update -d 'Force a refresh for all module indexes or a specific ecosystem'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke fetch search info list' -l /update -d 'Skip refreshing all module indexes or a specific ecosystem'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l exclude
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l to -r -F -d 'Short name or spec of CompUnit::Repository to install to'
complete -c zef -n '__fish_seen_subcommand_from install upgrade smoke' -l install-to -r -F -d 'Short name or spec of CompUnit::Repository to install to'

# Install
complete -c zef -n '__fish_seen_subcommand_from install' -l upgrade
complete -c zef -n '__fish_seen_subcommand_from install' -l deps-only -d 'Install only the dependency chains of the requested distributions'
complete -c zef -n '__fish_seen_subcommand_from install' -l contained -d 'Install dependencies regardless if they are already installed globally'

# Uninstall
complete -c zef -n '__fish_seen_subcommand_from uninstall' -l from
complete -c zef -n '__fish_seen_subcommand_from uninstall' -l uninstall-from

# Search, Info
complete -c zef -n '__fish_seen_subcommand_from search info' -x -l wrap

# Browse
complete -c zef -n '__fish_seen_subcommand_from browse' -l open
complete -c zef -n '__fish_seen_subcommand_from browse' -l /open

# List
complete -c zef -n '__fish_seen_subcommand_from list' -x -l max
complete -c zef -n '__fish_seen_subcommand_from list' -s i -l installed

# Locate
complete -c zef -n '__fish_seen_subcommand_from locate' -l sha1

# Nuke
complete -c zef -n '__fish_seen_subcommand_from locate' -l confirm
