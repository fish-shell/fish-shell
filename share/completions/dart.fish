# global option
complete -c dart -s h -l help -d "Print this usage information"
complete -c dart -s v -l verbose -d "Show additional command output"
complete -c dart -l version -d "Print the Dart SDK version"
complete -c dart -l enable-analytics -d "Enable analytics"
complete -c dart -l disable-analytics -d "Disable analytics"

# commands
complete -f -c dart -n __fish_use_subcommand -xa analyze -d "Analyze Dart code in a directory"
complete -f -c dart -n __fish_use_subcommand -xa compile -d "Compile Dart to various formats"
complete -f -c dart -n __fish_use_subcommand -xa create -d "Create a new Dart project"
complete -f -c dart -n __fish_use_subcommand -xa fix -d "Apply automated fixes to Dart source code"
complete -f -c dart -n __fish_use_subcommand -xa format -d "Idiomatically format Dart source code"
complete -f -c dart -n __fish_use_subcommand -xa migrate -d "Perform null safety migration on a project"
complete -f -c dart -n __fish_use_subcommand -xa pub -d "Work with packages"
complete -f -c dart -n __fish_use_subcommand -xa run -d "Run a Dart program"
complete -f -c dart -n __fish_use_subcommand -xa test -d "Run tests for a project"

# analyze
complete -c dart -n '__fish_seen_subcommand_from analyze' -l fatal-infos -d 'Treat info level issues as fatal'
complete -c dart -n '__fish_seen_subcommand_from analyze' -l fatal-warnings -d 'Treat warning level issues as fatal. (default on)'
complete -c dart -n '__fish_seen_subcommand_from analyze' -l no-fatal-warnings -d 'Don\'t treat warning level issues as fatal. (default on)'

# compile
complete -c dart -n '__fish_seen_subcommand_from compile' -xa aot-snapshot -d 'Compile Dart to an AOT snapshot'
complete -c dart -n '__fish_seen_subcommand_from compile' -xa exe -d 'Compile Dart to a self-contained executable'
complete -c dart -n '__fish_seen_subcommand_from compile' -xa jit-snapshot -d 'Compile Dart to a JIT snapshot'
complete -c dart -n '__fish_seen_subcommand_from compile' -xa js -d 'Compile Dart to JavaScript'
complete -c dart -n '__fish_seen_subcommand_from compile' -xa kernel -d 'Compile Dart to a kernel snapshot'

# create
complete -c dart -n '__fish_seen_subcommand_from create' -s t -l template -d 'The project template to use' -xa 'console-simple\tdefault console-full package-simple server-shelf web-simple'
complete -c dart -n '__fish_seen_subcommand_from create' -l pub -d 'Run \'pub get\' after the project has been created (default)'
complete -c dart -n '__fish_seen_subcommand_from create' -l no-pub -d 'Don\'t run \'pub get\' after the project has been created (default yes)'
complete -c dart -n '__fish_seen_subcommand_from create' -l force -d 'Force project generation, even if the target directory already exists'

# fix
complete -c dart -n '__fish_seen_subcommand_from fix' -s n -d 'Preview the proposed changes but make no changes'
complete -c dart -n '__fish_seen_subcommand_from fix' -l dry-run -d 'Preview the proposed changes but make no changes'
complete -c dart -n '__fish_seen_subcommand_from fix' -l apply -d 'Apply the proposed changes'

# format
complete -c dart -n '__fish_seen_subcommand_from format' -s o -l output -d 'Set where to write formatted output'
complete -c dart -n '__fish_seen_subcommand_from format' -l set-exit-if-changed -d 'Return exit code 1 if there are any formatting changes'
complete -c dart -n '__fish_seen_subcommand_from format' -l fix -d 'Apply all style fixes'
complete -c dart -n '__fish_seen_subcommand_from format' -s l -l line-length -d 'Wrap lines longer than this. (default "80")'

# migrate
complete -c dart -n '__fish_seen_subcommand_from migrate' -l apply-changes -d 'Apply the proposed null safety changes to the files on disk'
complete -c dart -n '__fish_seen_subcommand_from migrate' -l ignore-errors -d 'Attempt to perform null safety analysis even if the project has analysis errors'
complete -c dart -n '__fish_seen_subcommand_from migrate' -l skip-import-check -d 'Go ahead with migration even if some imported files have not yet been migrated'
complete -c dart -n '__fish_seen_subcommand_from migrate' -l web-preview -d 'Show preview of the proposed null safety changes in a browser window'
complete -c dart -n '__fish_seen_subcommand_from migrate' -l no-web-preview -d 'Show preview of the proposed null safety changes in the console'
complete -c dart -n '__fish_seen_subcommand_from migrate' -l preview-hostname -d 'Run the preview server on the specified hostname'
complete -c dart -n '__fish_seen_subcommand_from migrate' -l preview-port -d 'Run the preview server on the specified port'

# pub
complete -c dart -n '__fish_seen_subcommand_from pub' -s C -l directory -d 'Run the subcommand in this directory. (default "")'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa add -d 'Add a dependency to pubspec.yaml'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa cache -d 'Work with the system cache'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa deps -d 'Print package dependencies'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa downgrade -d 'Downgrade the current package\'s dependencies to oldest versions'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa get -d 'Get the current package\'s dependencies'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa global -d 'Work with global packages'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa login -d 'Log into pub.dev'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa logout -d 'Log out of pub.dev'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa outdated -d 'Analyze your dependencies to find which ones can be upgraded'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa publish -d 'Publish the current package to pub.dartlang.org'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa remove -d 'Removes a dependency from the current package'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa upgrade -d 'Upgrade the current package\'s dependencies to latest versions'
complete -c dart -n '__fish_seen_subcommand_from pub' -xa ploader -d 'Manage uploaders for a package on pub.dartlang.org'

# run
complete -c dart -n '__fish_seen_subcommand_from run' -l observe -d 'Run a program with common options for debugging'
complete -c dart -n '__fish_seen_subcommand_from run' -l enable-vm-service -d 'Enables VM service and listen on the specified port (default localhost:8181)'
complete -c dart -n '__fish_seen_subcommand_from run' -l serve-devtools -d 'Serve Dart DevTools debugger/profiler via service at <vm-service-uri>/devtools'
complete -c dart -n '__fish_seen_subcommand_from run' -l no-serve-devtools -d 'Serve Dart DevTools debugger/profiler via service at <vm-service-uri>/devtools'
complete -c dart -n '__fish_seen_subcommand_from run' -l pause-isolates-on-exit -d 'Pause isolates on exit when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l no-pause-isolates-on-exit -d 'Pause isolates on exit when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l pause-isolates-on-unhandled-exceptions -d 'Pause isolates after unhandled exception when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l no-pause-isolates-on-unhandled-exceptions -d 'Don\'t pause isolates after unhandled exception when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l warn-on-pause-with-no-debugger -d 'Warn when isolate pauses with no attached debugger when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l no-warn-on-pause-with-no-debugger -d 'Don\'t warn when isolate pauses with no attached debugger when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l pause-isolates-on-start -d 'Pause isolates on start when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l no-pause-isolates-on-start -d 'Don\'t pause isolates on start when running with --enable-vm-service'
complete -c dart -n '__fish_seen_subcommand_from run' -l enable-asserts -d 'Enable assert statements'
complete -c dart -n '__fish_seen_subcommand_from run' -l no-enable-asserts -d 'Disable assert statements'
complete -c dart -n '__fish_seen_subcommand_from run' -l verbosity -d 'Sets the verbosity level of the compilation'
complete -c dart -n '__fish_seen_subcommand_from run' -s D -l define -d 'Define an environment declaration'
