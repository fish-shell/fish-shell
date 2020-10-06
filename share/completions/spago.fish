# fish completion for spago, PureScript package manager and build tool
# version v0.16.0


function __fish_spago_is_arg_n --argument-names n
    test $n -eq (count (string match -v -- '-*' (commandline -poc)))
end


function __fish_spago_using_command
    set cmd (commandline -opc)
    if [ (count $cmd) -ge (count $argv) ]
        for i in (seq (count $argv))
            if [ $cmd[$i] != $argv[$i] ]
                return 1
            end
        end
        return 0
    end
    return 1
end


function __fish_spago_pkgnames
    for line in (spago ls packages 2> /dev/null)
        set pkgname (echo $line | string match -r '[^\s]+')
        echo $pkgname
    end
end


#
# spago
#
complete -c spago -x
complete -c spago -n '__fish_no_arguments' -x -s h -l help                              -d "Show this help text"
complete -c spago -n '__fish_no_arguments' -x -s q -l quiet                             -d "Suppress all spago logging"
complete -c spago -n '__fish_no_arguments' -x -s v -l verbose                           -d "Enable additional debug logging, e.g. printing `purs` commands"
complete -c spago -n '__fish_no_arguments' -x -s V -l very-verbose                      -d "Enable more verbosity: timestamps and source locations"
complete -c spago -n '__fish_no_arguments' -x      -l no-color                          -d "Log without ANSI color escape sequences"
complete -c spago -n '__fish_no_arguments' -x -s P -l no-psa                            -d "Don't build with `psa`, but use `purs`"
complete -c spago -n '__fish_no_arguments' -x -s j -l jobs         -a '1 2 3 4 5 6 7 8' -d "Limit the amount of jobs that can run concurrently"
complete -c spago -n '__fish_no_arguments' -r -s x -l config                            -d "Optional config path to be used instead of the default spago.dhall"
complete -c spago -n '__fish_no_arguments' -x -s c -l global-cache -a 'skip update'     -d "Configure the global caching behaviour: skip it with `skip` or force update with `update`"
complete -c spago -n '__fish_no_arguments' -x      -l version                           -d "Show spago version"


#
# spago init
#
complete -c spago -n '__fish_use_subcommand' -a init -d "Initialize a new sample project, or migrate a psc-package one"

complete -c spago -n '__fish_seen_subcommand_from init' -x
complete -c spago -n '__fish_seen_subcommand_from init' -x -s f -l force       -d "Overwrite any project found in the current directory"
complete -c spago -n '__fish_seen_subcommand_from init' -x -s C -l no-comments -d "Generate package.dhall and spago.dhall files without tutorial comments"
complete -c spago -n '__fish_seen_subcommand_from init' -x -s h -l help        -d "Show this help text"


#
# spago build
#
complete -c spago -n '__fish_use_subcommand' -a build -d "Install the dependencies and compile the current package"

complete -c spago -n '__fish_seen_subcommand_from build' -x
complete -c spago -n '__fish_seen_subcommand_from build' -x -s w -l watch         -d "Watch for changes in local files and automatically rebuild"
complete -c spago -n '__fish_seen_subcommand_from build' -x -s l -l clear-screen  -d "Clear the screen on rebuild (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from build' -x -s I -l allow-ignored -d "Allow files ignored via .gitignore to trigger rebuilds (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from build' -r -s p -l path          -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from build' -x -s x -l source-maps   -d "Whether to generate source maps for the bundle"
complete -c spago -n '__fish_seen_subcommand_from build' -x -s n -l no-install    -d "Don't run the automatic installation of packages"
complete -c spago -n '__fish_seen_subcommand_from build' -x -s u -l purs-args     -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from build' -x -s d -l deps-only     -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from build' -x      -l before        -d "Commands to run before a build."
complete -c spago -n '__fish_seen_subcommand_from build' -x      -l then          -d "Commands to run following a successful build."
complete -c spago -n '__fish_seen_subcommand_from build' -x      -l else          -d "Commands to run following an unsuccessful build."
complete -c spago -n '__fish_seen_subcommand_from build' -x -s h -l help          -d "Show this help text"


#
# spago repl
#
complete -c spago -n '__fish_use_subcommand' -a repl -d "Start a REPL"

complete -c spago -n '__fish_seen_subcommand_from repl' -x
complete -c spago -n '__fish_seen_subcommand_from repl' -x -s D -l dependency -d "Package name to add to the REPL as dependency"
complete -c spago -n '__fish_seen_subcommand_from repl' -r -s p -l path       -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from repl' -x -s u -l purs-args  -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from repl' -x -s d -l deps-only  -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from repl' -x -s h -l help       -d "Show this help text"


#
# spago test
#
complete -c spago -n '__fish_use_subcommand' -a test -d "Test the project with some module, default Test.Main"

complete -c spago -n '__fish_seen_subcommand_from test' -x
complete -c spago -n '__fish_seen_subcommand_from test' -x -s m -l main          -d "Module to be used as the application's entry point"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s w -l watch         -d "Watch for changes in local files and automatically rebuild"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s l -l clear-screen  -d "Clear the screen on rebuild (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s I -l allow-ignored -d "Allow files ignored via .gitignore to trigger rebuilds (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from test' -r -s p -l path          -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s x -l source-maps   -d "Whether to generate source maps for the bundle"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s n -l no-install    -d "Don't run the automatic installation of packages"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s u -l purs-args     -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from test' -x -s d -l deps-only     -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from test' -x      -l before        -d "Commands to run before a build."
complete -c spago -n '__fish_seen_subcommand_from test' -x      -l then          -d "Commands to run following a successful build."
complete -c spago -n '__fish_seen_subcommand_from test' -x      -l else          -d "Commands to run following an unsuccessful build."
complete -c spago -n '__fish_seen_subcommand_from test' -x -s a -l node-args     -d "Argument to pass to node (run/test only)"
complete -c spago -n '__fish_seen_subcommand_from test' -x -s h -l help          -d "Show this help text"


#
# spago run
#
complete -c spago -n '__fish_use_subcommand' -a run -d "Runs the project with some module, default Main"

complete -c spago -n '__fish_seen_subcommand_from run' -x
complete -c spago -n '__fish_seen_subcommand_from run' -x -s m -l main          -d "Module to be used as the application's entry point"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s w -l watch         -d "Watch for changes in local files and automatically rebuild"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s l -l clear-screen  -d "Clear the screen on rebuild (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s I -l allow-ignored -d "Allow files ignored via .gitignore to trigger rebuilds (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from run' -r -s p -l path          -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s x -l source-maps   -d "Whether to generate source maps for the bundle"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s n -l no-install    -d "Don't run the automatic installation of packages"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s u -l purs-args     -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from run' -x -s d -l deps-only     -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from run' -x      -l before        -d "Commands to run before a build."
complete -c spago -n '__fish_seen_subcommand_from run' -x      -l then          -d "Commands to run following a successful build."
complete -c spago -n '__fish_seen_subcommand_from run' -x      -l else          -d "Commands to run following an unsuccessful build."
complete -c spago -n '__fish_seen_subcommand_from run' -x -s a -l node-args     -d "Argument to pass to node (run/test only)"
complete -c spago -n '__fish_seen_subcommand_from run' -x -s h -l help          -d "Show this help text"


#
# spago bundle-app
#
complete -c spago -n '__fish_use_subcommand' -a bundle-app -d "Bundle the project into an executable"

complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s m -l main          -d "Module to be used as the application's entry point"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -r -s t -l to            -d "The target file path"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s s -l no-build      -d "Skip build step"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s w -l watch         -d "Watch for changes in local files and automatically rebuild"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s l -l clear-screen  -d "Clear the screen on rebuild (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s I -l allow-ignored -d "Allow files ignored via .gitignore to trigger rebuilds (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -r -s p -l path          -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s x -l source-maps   -d "Whether to generate source maps for the bundle"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s n -l no-install    -d "Don't run the automatic installation of packages"
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s u -l purs-args     -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s d -l deps-only     -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x      -l before        -d "Commands to run before a build."
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x      -l then          -d "Commands to run following a successful build."
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x      -l else          -d "Commands to run following an unsuccessful build."
complete -c spago -n '__fish_seen_subcommand_from bundle-app' -x -s h -l help          -d "Show this help text"


#
# spago bundle-module
#
complete -c spago -n '__fish_use_subcommand' -a bundle-module -d "Bundle the project into a CommonJS module"

complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s m -l main          -d "Module to be used as the application's entry point"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -r -s t -l to            -d "The target file path"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s s -l no-build      -d "Skip build step"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s w -l watch         -d "Watch for changes in local files and automatically rebuild"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s l -l clear-screen  -d "Clear the screen on rebuild (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s I -l allow-ignored -d "Allow files ignored via .gitignore to trigger rebuilds (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -r -s p -l path          -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s x -l source-maps   -d "Whether to generate source maps for the bundle"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s n -l no-install    -d "Don't run the automatic installation of packages"
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s u -l purs-args     -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s d -l deps-only     -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x      -l before        -d "Commands to run before a build."
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x      -l then          -d "Commands to run following a successful build."
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x      -l else          -d "Commands to run following an unsuccessful build."
complete -c spago -n '__fish_seen_subcommand_from bundle-module' -x -s h -l help          -d "Show this help text"


#
# spago docs
#
complete -c spago -n '__fish_use_subcommand' -a docs -d "Generate docs for the project and its dependencies"

complete -c spago -n '__fish_seen_subcommand_from docs' -x
complete -c spago -n '__fish_seen_subcommand_from docs' -x -s f -l format    -a 'markdown html etags ctags' -d "Docs output format (markdown | html | etags | ctags)"
complete -c spago -n '__fish_seen_subcommand_from docs' -r -s p -l path                                     -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from docs' -x -s d -l deps-only                                -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from docs' -x -s S -l no-search                                -d "Do not make the documentation searchable"
complete -c spago -n '__fish_seen_subcommand_from docs' -x -s o -l open                                     -d "Open generated documentation in browser (for HTML format only)"
complete -c spago -n '__fish_seen_subcommand_from docs' -x -s h -l help                                     -d "Show this help text"


#
# spago search
#
complete -c spago -n '__fish_use_subcommand' -a search -d "Start a search REPL to find definitions matching names and types"

complete -c spago -n '__fish_seen_subcommand_from search' -x
complete -c spago -n '__fish_seen_subcommand_from search' -x -s h -l help -d "Show this help text"


#
# spago path
#
complete -c spago -n '__fish_use_subcommand' -a path -d "Display paths used by the project"

complete -c spago -n '__fish_seen_subcommand_from path' -x
complete -c spago -n '__fish_seen_subcommand_from path' -x -s w -l watch         -d "Watch for changes in local files and automatically rebuild"
complete -c spago -n '__fish_seen_subcommand_from path' -x -s l -l clear-screen  -d "Clear the screen on rebuild (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from path' -x -s I -l allow-ignored -d "Allow files ignored via .gitignore to trigger rebuilds (watch mode only)"
complete -c spago -n '__fish_seen_subcommand_from path' -r -s p -l path          -d "Source path to include"
complete -c spago -n '__fish_seen_subcommand_from path' -x -s x -l source-maps   -d "Whether to generate source maps for the bundle"
complete -c spago -n '__fish_seen_subcommand_from path' -x -s n -l no-install    -d "Don't run the automatic installation of packages"
complete -c spago -n '__fish_seen_subcommand_from path' -x -s u -l purs-args     -d "Arguments to pass to purs compile. Wrap in quotes."
complete -c spago -n '__fish_seen_subcommand_from path' -x -s d -l deps-only     -d "Only use sources from dependencies, skipping the project sources."
complete -c spago -n '__fish_seen_subcommand_from path' -x      -l before        -d "Commands to run before a build."
complete -c spago -n '__fish_seen_subcommand_from path' -x      -l then          -d "Commands to run following a successful build."
complete -c spago -n '__fish_seen_subcommand_from path' -x      -l else          -d "Commands to run following an unsuccessful build."
complete -c spago -n '__fish_seen_subcommand_from path' -x -s h -l help          -d "Show this help text"


#
# spago path output
#
complete -c spago -n '__fish_spago_using_command spago path; and __fish_spago_is_arg_n 2' -a output -d "Output path for compiled code"


#
# spago path global-cache
#
complete -c spago -n '__fish_spago_using_command spago path; and __fish_spago_is_arg_n 2' -a global-cache -d "Location of the global cache"


#
# spago sources
#
complete -c spago -n '__fish_use_subcommand' -a sources -d "List all the source paths (globs) for the dependencies of the project"

complete -c spago -n '__fish_seen_subcommand_from sources' -x
complete -c spago -n '__fish_seen_subcommand_from sources' -x -s h -l help -d "Show this help text"


#
# spago install
#
complete -c spago -n '__fish_use_subcommand' -a install -d "Install (download) all dependencies listed in spago.dhall"

complete -c spago -n '__fish_seen_subcommand_from install' -x
complete -c spago -n '__fish_seen_subcommand_from install' -a '(__fish_spago_pkgnames)'
complete -c spago -n '__fish_seen_subcommand_from install' -x -s h -l help -d "Show this help text"


#
# spago ls
#
complete -c spago -n '__fish_use_subcommand' -a ls -d "List command. Supports: `packages`, `deps`"

complete -c spago -n '__fish_seen_subcommand_from ls' -x
complete -c spago -n '__fish_seen_subcommand_from ls' -x -s h -l help -d "Show this help text"


#
# spago ls packages
#
complete -c spago -n '__fish_spago_using_command spago ls; and __fish_spago_is_arg_n 2' -a packages -d "List packages available in the local package set"

complete -c spago -n '__fish_spago_using_command spago ls packages' -x -s j -l json -d "Produce JSON output"
complete -c spago -n '__fish_spago_using_command spago ls packages' -x -s h -l help -d "Show this help text"


#
# spago ls deps
#
complete -c spago -n '__fish_spago_using_command spago ls; and __fish_spago_is_arg_n 2' -a deps -d "List dependencies of the project"

complete -c spago -n '__fish_spago_using_command spago ls deps' -x -s j -l json       -d "Produce JSON output"
complete -c spago -n '__fish_spago_using_command spago ls deps' -x -s t -l transitive -d "Include transitive dependencies"
complete -c spago -n '__fish_spago_using_command spago ls deps' -x -s h -l help       -d "Show this help text"


#
# spago verify
#
complete -c spago -n '__fish_use_subcommand' -a verify -d "Verify that a single package is consistent with the Package Set"

complete -c spago -n '__fish_seen_subcommand_from verify' -x
complete -c spago -n '__fish_seen_subcommand_from verify' -x -s h -l help -d "Show this help text"


#
# spago verify-set
#
complete -c spago -n '__fish_use_subcommand' -a verify-set -d "Verify that the whole Package Set builds correctly"

complete -c spago -n '__fish_seen_subcommand_from verify-set' -x
complete -c spago -n '__fish_seen_subcommand_from verify-set' -x -s M -l no-check-modules-unique -d "Skip checking whether modules names are unique across all packages."
complete -c spago -n '__fish_seen_subcommand_from verify-set' -x -s h -l help                    -d "Show this help text"


#
# spago upgrade-set
#
complete -c spago -n '__fish_use_subcommand' -a upgrade-set -d "Upgrade the upstream in packages.dhall to the latest package-sets release"

complete -c spago -n '__fish_seen_subcommand_from upgrade-set' -x
complete -c spago -n '__fish_seen_subcommand_from upgrade-set' -x -s h -l help -d "Show this help text"


#
# spago freeze
#
complete -c spago -n '__fish_use_subcommand' -a freeze -d "Recompute the hashes for the package-set"

complete -c spago -n '__fish_seen_subcommand_from freeze' -x
complete -c spago -n '__fish_seen_subcommand_from freeze' -x -s h -l help -d "Show this help text"


#
# spago login
#
complete -c spago -n '__fish_use_subcommand' -a login -d "Save the GitHub token to the global cache - set it with the SPAGO_GITHUB_TOKEN env variable"

complete -c spago -n '__fish_seen_subcommand_from login' -x
complete -c spago -n '__fish_seen_subcommand_from login' -x -s h -l help -d "Show this help text"


#
# spago bump-version
#
complete -c spago -n '__fish_use_subcommand' -a bump-version -d "Bump and tag a new version, and generate bower.json, in preparation for release."

complete -c spago -n '__fish_seen_subcommand_from bump-version' -x
complete -c spago -n '__fish_seen_subcommand_from bump-version' -x -s f -l no-dry-run       -d "Actually perform side-effects (the default is to describe what would be done)"
complete -c spago -n '__fish_seen_subcommand_from bump-version' -x -a 'major minor patch v' -d "How to bump the version. Acceptable values: 'major', 'minor', 'patch', or a version (e.g. 'v1.2.3')."
complete -c spago -n '__fish_seen_subcommand_from bump-version' -x -s h -l help             -d "Show this help text"


#
# spago version
#
complete -c spago -n '__fish_use_subcommand' -a version -d "Show spago version"

complete -c spago -n '__fish_seen_subcommand_from version' -x
complete -c spago -n '__fish_seen_subcommand_from version' -x -s h -l help -d "Show this help text"
