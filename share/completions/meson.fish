# Completions for the meson build system (http://mesonbuild.com/)

function __fish_meson_needs_command
    set -l cmd (commandline -xpc)
    set -e cmd[1]
    argparse -s v/version -- $cmd 2>/dev/null
    or return 0
    not set -q argv[1]
end

function __fish_meson_using_command
    set -l cmd (commandline -xpc)
    set -e cmd[1]
    test (count $cmd) -eq 0
    and return 1
    contains -- $cmd[1] $argv
    and return 0
end

function __fish_meson_builddir
    # Consider the value of -C option to detect the build directory
    set -l cmd (commandline -xpc)
    argparse -i 'C=' -- $cmd
    if set -q _flag_C
        echo $_flag_C
    else
        echo .
    end
end

function __fish_meson_targets
    set -l python (__fish_anypython); or return
    meson introspect --targets (__fish_meson_builddir) | $python -S -c 'import json, sys
data = json.load(sys.stdin)
targets = set()
for target in data:
    targets.add(target["name"])
for name in targets:
    print(name)' 2>/dev/null
end

function __fish_meson_subprojects
    set -l python (__fish_anypython); or return
    meson introspect --projectinfo (__fish_meson_builddir) | $python -S -c 'import json, sys
data = json.load(sys.stdin)
for subproject in data["subprojects"]:
    print(subproject["name"])' 2>/dev/null
end

function __fish_meson_tests
    # --list option shows suites in a short form, e.g. if a test "gvariant"
    # is present both in "glib:glib" and "glib:slow" suites, it will be shown
    # in a list as "glib:glib+slow / gvariant". So, just filter out the first
    # part and list all of the test names.
    meson test -C (__fish_meson_builddir) --no-rebuild --list | string split -r -f1 ' / '
end

function __fish_meson_test_suites
    set -l python (__fish_anypython); or return
    meson introspect --tests (__fish_meson_builddir) | $python -S -c 'import json, sys
data = json.load(sys.stdin)
suites = set()
for test in data:
    suites.update(test["suite"])
for name in suites:
    print(name)' 2>/dev/null
end

function __fish_meson_help_commands
    meson help --help | string match -g -r '^ *{(.*)}' | string split ,
end

# Each meson command and subcommand has -h/--help option
complete -c meson -s h -l help -d 'Show help'

# In order to prevent directory completions from being mixed in with subcommand completions,
# we need to use -kxa instead of -xa and make sure we do the directory completions first.
# In order for subcommands to be sorted alphabetically, we need to make sure that we compose
# them in the reverse alphabetical order and use -kxa there as well.

# This is to support the implicit setup/configure mode, deprecated upstream but not yet removed.
complete -c meson -n __fish_meson_needs_command -kxa '(__fish_complete_directories)'

### wrap
set -l wrap_cmds list search install update info status promote update-db
complete -c meson -n __fish_meson_needs_command -kxa wrap -d 'Manage WrapDB dependencies'
complete -c meson -n "__fish_meson_using_command wrap; and not __fish_seen_subcommand_from $wrap_cmds" -xa '
list\t"Show all available projects"
search\t"Search the db by name"
install\t"Install the specified project"
update\t"Update wrap files from WrapDB"
info\t"Show available versions of a project"
status\t"Show installed and available versions of your projects"
promote\t"Bring a subsubproject up to the master project"
update-db\t"Update list of projects available in WrapDB"
'
complete -c meson -n "__fish_meson_using_command wrap; and __fish_seen_subcommand_from $wrap_cmds" -l allow-insecure -d 'Allow insecure server connections'
complete -c meson -n '__fish_meson_using_command wrap; and __fish_seen_subcommand_from update' -l force -d 'Update wraps that does not seems to come from WrapDB'
complete -c meson -n '__fish_meson_using_command wrap; and __fish_seen_subcommand_from update' -l sourcedir -xa '(__fish_complete_directories)' -d 'Source directory'
complete -c meson -n '__fish_meson_using_command wrap; and __fish_seen_subcommand_from update' -l types -x -d 'Comma-separated list of subproject types'
complete -c meson -n '__fish_meson_using_command wrap; and __fish_seen_subcommand_from update' -l num-processes -x -d 'How many parallel processes to use'
complete -c meson -n '__fish_meson_using_command wrap; and __fish_seen_subcommand_from update' -l allow-insecure -x -d 'Allow insecure server connections'
complete -c meson -n '__fish_meson_using_command wrap; and __fish_seen_subcommand_from promote' -xa '(__fish_complete_directories)' -d 'Project path'

### test
complete -c meson -n __fish_meson_needs_command -kxa test -d 'Run tests for the project'
# TODO: meson allows to pass just "testname" to run all tests with that name,
# or "subprojname:testname" to run "testname" from "subprojname",
# or "subprojname:" to run all tests defined by "subprojname",
# but completion is only handled for the "testname".
complete -c meson -n '__fish_meson_using_command test' -xa '(__fish_meson_tests)'
complete -c meson -n '__fish_meson_using_command test' -s h -l help -d 'Show help'
complete -c meson -n '__fish_meson_using_command test' -s C -xa '(__fish_complete_directories)' -d 'Directory to cd into before running'
complete -c meson -n '__fish_meson_using_command test' -l maxfail -x -d 'Number of failing tests before aborting the test run'
complete -c meson -n '__fish_meson_using_command test' -l repeat -x -d 'Number of times to run the tests'
complete -c meson -n '__fish_meson_using_command test' -l no-rebuild -d 'Do not rebuild before running tests'
complete -c meson -n '__fish_meson_using_command test' -l gdb -d 'Run test under gdb'
complete -c meson -n '__fish_meson_using_command test' -l gdb-path -r -d 'Run test under gdb'
complete -c meson -n '__fish_meson_using_command test' -l list -d 'List available tests'
complete -c meson -n '__fish_meson_using_command test' -l wrapper -r -d 'Wrapper to run tests with (e.g. valgrind)'
complete -c meson -n '__fish_meson_using_command test' -l suite -xa '(__fish_meson_test_suites)' -d 'Only run tests belonging to the given suite'
complete -c meson -n '__fish_meson_using_command test' -l no-suite -xa '(__fish_meson_test_suites)' -d 'Do not run tests belonging to the given suite'
complete -c meson -n '__fish_meson_using_command test' -l no-stdsplit -d 'Do not split stderr and stdout in test logs'
complete -c meson -n '__fish_meson_using_command test' -l print-errorlogs -d 'Print logs of failing tests'
complete -c meson -n '__fish_meson_using_command test' -l benchmark -d 'Run benchmarks instead of tests'
complete -c meson -n '__fish_meson_using_command test' -l logbase -x -d 'Base name for log file'
complete -c meson -n '__fish_meson_using_command test' -l num-processes -x -d 'How many parallel processes to use'
complete -c meson -n '__fish_meson_using_command test' -s v -l verbose -d 'Do not redirect stdout and stderr'
complete -c meson -n '__fish_meson_using_command test' -s q -l quiet -d 'Produce less output to the terminal'
complete -c meson -n '__fish_meson_using_command test' -s t -l timeout-multiplier -x -d 'Multiplier for test timeout'
complete -c meson -n '__fish_meson_using_command test' -l setup -x -d 'Which test setup to use'
complete -c meson -n '__fish_meson_using_command test' -l test-args -x -d 'Arguments to pass to the test(s)'

### subprojects
set -l subprojects_cmds update checkout download foreach purge packagefiles
complete -c meson -n __fish_meson_needs_command -kxa subprojects -d 'Manage subprojects'
complete -c meson -n "__fish_meson_using_command subprojects; and not __fish_seen_subcommand_from $subprojects_cmds" -xa '
update\t"Update all subprojects"
checkout\t"Checkout a branch (git only)"
download\t"Ensure subprojects are fetched"
foreach\t"Execute a command in each subproject"
purge\t"Remove all wrap-based subproject artifacts"
packagefiles\t"Manage the packagefiles overlay"
'
complete -c meson -n "__fish_meson_using_command subprojects; and __fish_seen_subcommand_from $subprojects_cmds" -l sourcedir -xa '(__fish_complete_directories)' -d 'Path to source directory'
complete -c meson -n "__fish_meson_using_command subprojects; and __fish_seen_subcommand_from $subprojects_cmds" -l types -xa 'file git hg svn' -d 'Comma-separated list of subproject types'
complete -c meson -n "__fish_meson_using_command subprojects; and __fish_seen_subcommand_from $subprojects_cmds" -l num-processes -x -d 'How many parallel processes to use'
complete -c meson -n "__fish_meson_using_command subprojects; and __fish_seen_subcommand_from $subprojects_cmds" -l allow-insecure -x -d 'Allow insecure server connections'
complete -c meson -n '__fish_meson_using_command subprojects; and __fish_seen_subcommand_from update' -l reset -d 'Checkout wrap\'s revision and hard reset to that commit'
complete -c meson -n '__fish_meson_using_command subprojects; and __fish_seen_subcommand_from checkout' -s b -d 'Create a new branch'
complete -c meson -n '__fish_meson_using_command subprojects; and __fish_seen_subcommand_from purge' -l include-cache -d 'Remove the package cache as well'
complete -c meson -n '__fish_meson_using_command subprojects; and __fish_seen_subcommand_from purge' -l confirm -d 'Confirm the removal of subproject artifacts'
complete -c meson -n '__fish_meson_using_command subprojects; and __fish_seen_subcommand_from packagefiles' -l apply -d 'Apply packagefiles to the subproject'
complete -c meson -n '__fish_meson_using_command subprojects; and __fish_seen_subcommand_from packagefiles' -l save -d 'Save packagefiles from the subproject'

### setup
complete -c meson -n __fish_meson_needs_command -kxa setup -d 'Configure a build directory'
# All of the setup options are also exposed to the global scope
# Use -k here for one of the cases to make sure directories come after any other top-level completions
complete -c meson -n '__fish_meson_using_command setup' -xa '(__fish_complete_directories)'
# A lot of options are shared for "setup" and "configure" commands
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l prefix -xa '(__fish_complete_directories)' -d 'Installation prefix'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l bindir -xa '(__fish_complete_directories)' -d 'Executable directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l datadir -xa '(__fish_complete_directories)' -d 'Data file directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l includedir -xa '(__fish_complete_directories)' -d 'Header file directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l infodir -xa '(__fish_complete_directories)' -d 'Info page directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l libdir -xa '(__fish_complete_directories)' -d 'Library directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l licensedir -xa '(__fish_complete_directories)' -d 'Licenses directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l libexecdir -xa '(__fish_complete_directories)' -d 'Library executable directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l localedir -xa '(__fish_complete_directories)' -d 'Locale data directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l localstatedir -xa '(__fish_complete_directories)' -d 'Localstate data directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l mandir -xa '(__fish_complete_directories)' -d 'Manual page directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l sbindir -xa '(__fish_complete_directories)' -d 'System executable directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l sharedstatedir -xa '(__fish_complete_directories)' -d 'Architecture-independent data directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l sysconfdir -xa '(__fish_complete_directories)' -d 'Sysconf data directory'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l auto-features -xa 'enabled disabled auto' -d 'Override value of all "auto" features'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l backend -xa 'ninja vs vs2010 vs2012 vs2013 vs2015 vs2017 vs2019 vs2022 xcode' -d 'Backend to use'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l buildtype -xa 'plain debug debugoptimized release minsize custom' -d 'Build type to use'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l debug -d 'Enable debug symbols and other info'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l default-library -xa 'shared static both' -d 'Default library type'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l errorlogs -d 'Print the logs from failing tests'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l install-umask -x -d 'Default umask to apply on permissions of installed files'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l layout -xa 'mirror flat' -d 'Build directory layout'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l optimization -xa 'plain 0 g 1 2 3 s' -d 'Optimization level'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l prefer-static -d 'Try static linking before shared linking'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l stdsplit -d 'Split stdout and stderr in test logs'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l strip -d 'Strip targets on install'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l unity -xa 'on off subprojects' -d 'Unity build'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l unity-size -x -d 'Unity block size'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l warnlevel -xa '0 1 2 3 everything' -d 'Compiler warning level to use'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l werror -d 'Treat warnings as errors'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l wrap-mode -xa 'default nofallback nodownload forcefallback nopromote' -d 'Wrap mode'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l force-fallback-for -x -d 'Force fallback for those subprojects'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l pkgconfig.relocatable -d 'Generate pkgconfig files as relocatable'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l python.install-env -xa 'auto prefix system venv' -d 'Which python environment to install to'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l python.platlibdir -x -d 'Directory for site-specific, platform-specific files'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l python.purelibdir -x -d 'Directory for site-specific, non-platform-specific files'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l pkg-config-path -x -d 'Additional paths for pkg-config (for host machine)'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l build.pkg-config-path -x -d 'Additional paths for pkg-config (for build machine)'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l cmake-prefix-path -x -d 'Additional prefixes for cmake (for host machine)'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -l build.cmake-prefix-path -x -d 'Additional prefixes for cmake (for build machine)'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup configure' -s D -x -d 'Set the value of an option'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -l native-file -r -d 'File with overrides for native compilation environment'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -l cross-file -r -d 'File describing cross compilation environment'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -l vsenv -d 'Force setup of Visual Studio environment'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -s v -l version -d 'Show version number and exit'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -l fatal-meson-warnings -d 'Make all Meson warnings fatal'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -l reconfigure -d 'Set options and reconfigure the project'
complete -c meson -n '__fish_meson_needs_command || __fish_meson_using_command setup' -l wipe -d 'Wipe build directory and reconfigure'

### rewrite
set -l rewrite_cmds target kwargs default-options command
complete -c meson -n __fish_meson_needs_command -kxa rewrite -d 'Modify the project'
complete -c meson -n "__fish_meson_using_command rewrite; and not __fish_seen_subcommand_from $rewrite_cmds" -s s -l sourcedir -xa '(__fish_complete_directories)' -d 'Path to source directory'
complete -c meson -n "__fish_meson_using_command rewrite; and not __fish_seen_subcommand_from $rewrite_cmds" -s V -l verbose -d 'Enable verbose output'
complete -c meson -n "__fish_meson_using_command rewrite; and not __fish_seen_subcommand_from $rewrite_cmds" -s S -l skip-errors -d 'Skip errors instead of aborting'
complete -c meson -n "__fish_meson_using_command rewrite; and not __fish_seen_subcommand_from $rewrite_cmds" -xa '
target\t"Modify a target"
kwargs\t"Modify keyword arguments"
default-options\t"Modify the project default options"
command\t"Execute a JSON array of commands"
'
# TODO: "meson rewrite target" completions are incomplete and hard to implement properly
complete -c meson -n '__fish_meson_using_command rewrite; and __fish_seen_subcommand_from target' -s s -l subdir -xa '(__fish_complete_directories)' -d 'Subdirectory of the new target'
complete -c meson -n '__fish_meson_using_command rewrite; and __fish_seen_subcommand_from target' -l type -d 'Type of the target to add' \
    -xa 'both_libraries executable jar library shared_library shared_module static_library'
complete -c meson -n '__fish_meson_using_command rewrite; and __fish_seen_subcommand_from kwargs; and __fish_is_nth_token 3' -xa 'set delete add remove remove_regex info' -d 'Action to execute'
complete -c meson -n '__fish_meson_using_command rewrite; and __fish_seen_subcommand_from kwargs; and __fish_is_nth_token 4' -xa 'dependency target project' -d 'Function type to modify'
complete -c meson -n '__fish_meson_using_command rewrite; and __fish_seen_subcommand_from default-options; and __fish_is_nth_token 3' -xa 'set delete' -d 'Action to execute'

### introspect
complete -c meson -n __fish_meson_needs_command -kxa introspect -d 'Display info about a project'
complete -c meson -n '__fish_meson_using_command introspect' -xa '(__fish_complete_directories)'
complete -c meson -n '__fish_meson_using_command introspect' -l ast -d 'Dump the AST of the meson file'
complete -c meson -n '__fish_meson_using_command introspect' -l benchmarks -d 'List all benchmarks'
complete -c meson -n '__fish_meson_using_command introspect' -l buildoptions -d 'List all build options'
complete -c meson -n '__fish_meson_using_command introspect' -l buildsystem-files -d 'List files that make up the build system'
complete -c meson -n '__fish_meson_using_command introspect' -l dependencies -d 'List external dependencies'
complete -c meson -n '__fish_meson_using_command introspect' -l scan-dependencies -d 'Scan for dependencies used in the meson.build file'
complete -c meson -n '__fish_meson_using_command introspect' -l installed -d 'List all installed files and directories'
complete -c meson -n '__fish_meson_using_command introspect' -l install-plan -d 'List all installed files and directories with their details'
complete -c meson -n '__fish_meson_using_command introspect' -l projectinfo -d 'Information about projects'
complete -c meson -n '__fish_meson_using_command introspect' -l targets -d 'List top level targets'
complete -c meson -n '__fish_meson_using_command introspect' -l tests -d 'List all unit tests'
complete -c meson -n '__fish_meson_using_command introspect' -l backend -xa 'ninja vs vs2010 vs2012 vs2013 vs2015 vs2017 vs2019 vs2022 xcode' -d 'The backend to use for the --buildoptions introspection'
complete -c meson -n '__fish_meson_using_command introspect' -s a -l all -d 'Print all available information'
complete -c meson -n '__fish_meson_using_command introspect' -s i -l indent -d 'Enable pretty printed JSON'
complete -c meson -n '__fish_meson_using_command introspect' -s f -l force-object-output -d 'Always use the new JSON format for multiple entries'

### install
complete -c meson -n __fish_meson_needs_command -kxa install -d 'Install the project'
complete -c meson -n '__fish_meson_using_command install' -f
complete -c meson -n '__fish_meson_using_command install' -s C -xa '(__fish_complete_directories)' -d 'Directory to cd into before running'
complete -c meson -n '__fish_meson_using_command install' -l no-rebuild -d 'Do not rebuild before installing'
complete -c meson -n '__fish_meson_using_command install' -l only-changed -d 'Only overwrite files that are older than the copied file'
complete -c meson -n '__fish_meson_using_command install' -l quiet -d 'Do not print every file that was installed'
complete -c meson -n '__fish_meson_using_command install' -l destdir -r -d 'Sets or overrides DESTDIR environment'
complete -c meson -n '__fish_meson_using_command install' -s n -l dry-run -d 'Do not actually install, but print logs'
complete -c meson -n '__fish_meson_using_command install' -l skip-subprojects -xa '(__fish_meson_subprojects)' -d 'Do not install files from given subprojects'
complete -c meson -n '__fish_meson_using_command install' -l tags -x -d 'Install only targets having one of the given tags'
complete -c meson -n '__fish_meson_using_command install' -l strip -d 'Strip targets even if strip option was not set during configure'

### init
complete -c meson -n __fish_meson_needs_command -kxa init -d 'Create a project from template'
complete -c meson -n '__fish_meson_using_command init' -s C -xa '(__fish_complete_directories)' -d 'Directory to cd into before running'
complete -c meson -n '__fish_meson_using_command init' -s n -l name -x -d 'Project name'
complete -c meson -n '__fish_meson_using_command init' -s e -l executable -x -d 'Executable name'
complete -c meson -n '__fish_meson_using_command init' -s d -l deps -x -d 'Dependencies, comma-separated'
complete -c meson -n '__fish_meson_using_command init' -s l -l language -xa 'c cpp cs cuda d fortran java objc objcpp rust vala' -d 'Project language'
complete -c meson -n '__fish_meson_using_command init' -s b -l build -d 'Build after generation'
complete -c meson -n '__fish_meson_using_command init' -l builddir -r -d 'Directory for build'
complete -c meson -n '__fish_meson_using_command init' -s f -l force -d 'Force overwrite of existing files and directories'
complete -c meson -n '__fish_meson_using_command init' -l type -xa 'executable library' -d 'Project type'
complete -c meson -n '__fish_meson_using_command init' -l version -x -d 'Project version'

### help
complete -c meson -n __fish_meson_needs_command -kxa help -d 'Show help for a command'
complete -c meson -n '__fish_meson_using_command help' -xa "(__fish_meson_help_commands)"

### dist
complete -c meson -n __fish_meson_needs_command -kxa dist -d 'Generate a release archive'
complete -c meson -n '__fish_meson_using_command dist' -f
complete -c meson -n '__fish_meson_using_command dist' -s C -xa '(__fish_complete_directories)' -d 'Directory to cd into before running'
complete -c meson -n '__fish_meson_using_command dist' -l allow-dirty -d 'Allow even when repository contains uncommitted changes'
complete -c meson -n '__fish_meson_using_command dist' -l formats -xa 'xztar gztar zip' -d 'Comma separated list of archive types to create'
complete -c meson -n '__fish_meson_using_command dist' -l include-subprojects -d 'Include source code of subprojects'
complete -c meson -n '__fish_meson_using_command dist' -l no-tests -d 'Do not build and test generated packages'

### devenv
complete -c meson -n __fish_meson_needs_command -kxa devenv -d 'Run a command from the build directory'
complete -c meson -n '__fish_meson_using_command devenv' -s h -l help -d 'Show help'
complete -c meson -n '__fish_meson_using_command devenv' -s C -xa '(__fish_complete_directories)' -d 'Path to build directory'
complete -c meson -n '__fish_meson_using_command devenv' -s w -l workdir -xa '(__fish_complete_directories)' -d 'Directory to cd into before running'
complete -c meson -n '__fish_meson_using_command devenv' -l dump -d 'Only print required environment'
complete -c meson -n '__fish_meson_using_command devenv' -l dump-format -xa 'sh export vscode' -d 'Format used with --dump'

### configure
complete -c meson -n __fish_meson_needs_command -kxa configure -d 'Change project options'
complete -c meson -n '__fish_meson_using_command configure' -xa '(__fish_complete_directories)'
complete -c meson -n '__fish_meson_using_command configure' -l clearcache -d 'Clear cached state'
complete -c meson -n '__fish_meson_using_command configure' -l no-pager -d 'Do not redirect output to a pager'

### compile
complete -c meson -n __fish_meson_needs_command -kxa compile -d 'Build the configured project'
complete -c meson -n '__fish_meson_using_command compile' -xa '(__fish_meson_targets)'
complete -c meson -n '__fish_meson_using_command compile' -l clean -d 'Clean the build directory'
complete -c meson -n '__fish_meson_using_command compile' -s C -r -d 'Directory to cd into before running'
complete -c meson -n '__fish_meson_using_command compile' -s j -l jobs -x -d 'The number of worker jobs to run'
complete -c meson -n '__fish_meson_using_command compile' -s l -l load-average -x -d 'The system load average to try to maintain'
complete -c meson -n '__fish_meson_using_command compile' -s v -l verbose -d 'Show more verbose output'
complete -c meson -n '__fish_meson_using_command compile' -l ninja-args -x -d 'Arguments to pass to `ninja`'
complete -c meson -n '__fish_meson_using_command compile' -l vs-args -x -d 'Arguments to pass to `msbuild`'
complete -c meson -n '__fish_meson_using_command compile' -l xcode-args -x -d 'Arguments to pass to `xcodebuild`'

# tag: k_reverse_order
