# cjpm.fish - Fish completion script for Cangjie Package Manager

# Global options
complete -c cjpm -l help -s h -d "Help for cjpm"
complete -c cjpm -l version -s v -d "Version for cjpm"

# Subcommands
complete -c cjpm -n __fish_use_subcommand -f -a init -d "Init a new cangjie module"
complete -c cjpm -n __fish_use_subcommand -f -a check -d "Check the dependencies"
complete -c cjpm -n __fish_use_subcommand -f -a update -d "Update cjpm.lock"
complete -c cjpm -n __fish_use_subcommand -f -a tree -d "Display the package dependencies in the source code"
complete -c cjpm -n __fish_use_subcommand -f -a build -d "Compile the current module"
complete -c cjpm -n __fish_use_subcommand -f -a run -d "Compile and run an executable product"
complete -c cjpm -n __fish_use_subcommand -f -a test -d "Unittest a local package or module"
complete -c cjpm -n __fish_use_subcommand -f -a bench -d "Run benchmarks in a local package or module"
complete -c cjpm -n __fish_use_subcommand -f -a clean -d "Clean up the target directory"
complete -c cjpm -n __fish_use_subcommand -f -a install -d "Install a cangjie binary"
complete -c cjpm -n __fish_use_subcommand -f -a uninstall -d "Uninstall a cangjie binary"

# 'init' subcommand options
complete -c cjpm -n "__fish_seen_subcommand_from init" -f -l help -s h -d "Help for init"
complete -c cjpm -n "__fish_seen_subcommand_from init" -f -l workspace -d "Initialize a workspace's default configuration file"
complete -c cjpm -n "__fish_seen_subcommand_from init" -f -l name -d "Specify root package name (default: current directory)" -r
complete -c cjpm -n "__fish_seen_subcommand_from init" -l path -d "Specify path to create the module (default: current directory)" -r
complete -c cjpm -n "__fish_seen_subcommand_from init" -f -l type -d "Define output type of current module" -r -f -a "executable static dynamic"

# 'run' subcommand options
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -l name -d "Name of the executable product to run (default: main)" -r
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -l build-args -d "Arguments to pass to the build process" -r
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -l skip-build -d "Skip compile, only run the executable product"
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -l run-args -d "Arguments to pass to the executable product" -r
complete -c cjpm -n "__fish_seen_subcommand_from run" -l target-dir -d "Specify target directory" -r
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -s g -d "Enable debug version"
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -s h -l help -d "Help for run"
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -s V -l verbose -d "Enable verbose"
complete -c cjpm -n "__fish_seen_subcommand_from run" -f -l skip-script -d "Disable script 'build.cj'"

# 'install' subcommand options
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -s h -l help -d "Help for install"
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -s V -l verbose -d "Enable verbose"
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -s m -l member -d "Specify a member module of the workspace" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -s g -d "Enable install debug version target"
complete -c cjpm -n "__fish_seen_subcommand_from install" -l path -d "Specify path of source module (default: current path)" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -l root -d "Specify path of installed binary" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l git -d "Specify URL of installed git module" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l branch -d "Specify branch of installed git module" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l tag -d "Specify tag of installed git module" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l commit -d "Specify commit ID of installed git module" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -s j -l jobs -d "Number of jobs to spawn in parallel" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l cfg -d "Enable the customized option 'cfg'"
complete -c cjpm -n "__fish_seen_subcommand_from install" -l target-dir -d "Specify target directory" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l name -d "Specify product name to install (default: all)" -r
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l skip-build -d "Install binary in target directory without building"
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l list -d "List all installed modules and their versions"
complete -c cjpm -n "__fish_seen_subcommand_from install" -f -l skip-script -d "Disable script 'build.cj'"

# 'build' subcommand options
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s h -l help -d "Help for build"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s i -l incremental -d "Enable incremental compilation"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s j -l jobs -d "Number of jobs to spawn in parallel" -r
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s V -l verbose -d "Enable verbose"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s g -d "Enable compile debug version target"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -l coverage -d "Enable coverage"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -l cfg -d "Enable the customized option 'cfg'"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s m -l member -d "Specify a member module of the workspace" -r
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -l target -d "Generate code for the given target platform" -r
complete -c cjpm -n "__fish_seen_subcommand_from build" -l target-dir -d "Specify target directory" -r
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s o -l output -d "Specify product name when compiling an executable file" -r
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -s l -l lint -d "Enable cjlint code check"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -l mock -d "Enable support of mocking classes in tests"
complete -c cjpm -n "__fish_seen_subcommand_from build" -f -l skip-script -d "Disable script 'build.cj'"

# 'test' subcommand options
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -s h -l help -d "Help for test"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -s j -l jobs -d "Number of jobs to spawn in parallel" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -s V -l verbose -d "Enable verbose"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -s g -d "Enable compile debug version tests"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -s i -l incremental -d "Enable incremental compilation"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l no-run -d "Compile, but don't run tests"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l skip-build -d "Skip compile, only run tests"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l coverage -d "Enable coverage"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l cfg -d "Enable the customized option 'cfg'"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l module -d "Specify modules to test (default: current module)" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -s m -l member -d "Specify a member module of the workspace" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l target -d "Unittest for the given target platform" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -l target-dir -d "Specify target directory" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l dry-run -d "Print tests without execution"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l filter -d "Enable filter test" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l include-tags -d "Run tests with specified tags" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l exclude-tags -d "Run tests without specified tags" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l no-color -d "Enable colorless result output"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l random-seed -d "Enable random seed" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l timeout-each -d "Specify default timeout for test cases" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l parallel -d "Number of workers running tests" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l show-all-output -d "Show output for all test cases"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l no-capture-output -d "Disable test output capturing"
complete -c cjpm -n "__fish_seen_subcommand_from test" -l report-path -d "Specify path to directory of report" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l report-format -d "Specify format of report" -r
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l skip-script -d "Disable script 'build.cj'"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l no-progress -d "Disable progress report"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l progress-brief -d "Display brief progress report"
complete -c cjpm -n "__fish_seen_subcommand_from test" -f -l progress-entries-limit -d "Limit number of entries shown in progress report"
