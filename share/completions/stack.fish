complete -c stack -f

# Completion for 'stack' haskell build tool (http://haskellstack.org)
# (Handmade) generated from version 1.0.0

#
# Options:
#

set -l project_path "(stack path --project-root)"

complete -c stack -l help -d 'Show this help text'
complete -c stack -l version -d'Show version'
complete -c stack -l numeric-version -d 'Show only version number'
complete -c stack -l docker -d 'Run \'stack --docker-help\' for details'
complete -c stack -l nix -d 'Run \'stack --nix-help\' for details'
complete -c stack -l verbosity -a 'silent error warn info debug' -d 'Verbosity: silent, error, warn, info, debug'
complete -c stack -l verbose -d 'Enable verbose mode: verbosity level "debug"'
complete -c stack -l work-dir -a '(__fish_complete_directories)' -d 'Override work directory (default: .stack-work)'
complete -c stack -l system-ghc -d 'Enable using the system installed GHC (on the PATH) if available and a matching version'
complete -c stack -l no-system-ghc -d 'Disable using the system installed GHC (on the PATH) if available and a matching version'
complete -c stack -l install-ghc -d 'Enable downloading and installing GHC if necessary (can be done manually with stack setup)'
complete -c stack -l no-install-ghc -d 'Disable downloading and installing GHC if necessary (can be done manually with stack setup)'
complete -c stack -l arch -r -d 'System architecture, e.g. i386, x86_64'
complete -c stack -l os -r -d 'Operating system, e.g. linux, windows'
complete -c stack -l ghc-variant -d 'Specialized GHC variant, e.g. integersimple (implies --no-system-ghc)'
complete -c stack -l obs -r -d 'Number of concurrent jobs to run'
complete -c stack -l extra-include-dirs -a '(__fish_complete_directories)' -d 'Extra directories to check for C header files'
complete -c stack -l extra-lib-dirs -a '(__fish_complete_directories)' -d 'Extra directories to check for libraries'
complete -c stack -l skip-ghc-check -d 'Enable skipping the GHC version and architecture check'
complete -c stack -l no-skip-ghc-check -d 'Disable skipping the GHC version and architecture check'
complete -c stack -l skip-msys -d 'Enable skipping the local MSYS installation (Windows only)'
complete -c stack -l no-skip-msys -d 'Disable skipping the local MSYS installation (Windows only)'
complete -c stack -l local-bin-path -a '(__fish_complete_directories)' -d 'Install binaries to DIR'
complete -c stack -l modify-code-page -d 'Enable setting the codepage to support UTF-8 (Windows only)'
complete -c stack -l no-modify-code-page -d 'Disable setting the codepage to support UTF-8 (Windows only)'
complete -c stack -l resolver -d 'Override resolver in project file'
complete -c stack -l compiler -d 'Use the specified compiler'
complete -c stack -l terminal -d 'Enable overriding terminal detection in the case of running in a false terminal'
complete -c stack -l no-terminal -d 'Disable overriding terminal detection in the case of running in a false terminal'
complete -c stack -l stack-yaml -a '(__fish_complete_path)' -d 'Override project stack.yaml file (overrides any STACK_YAML environment variable)'

#
# Commands:
#

complete -c stack -a build -d 'Build the package(s) in this directory/configuration'
complete -c stack -a install -d 'Shortcut for \'build --copy-bins\''
complete -c stack -a test -d 'Shortcut for \'build --test\''
complete -c stack -a bench -d 'Shortcut for \'build --bench\''
complete -c stack -a haddock -d 'Shortcut for \'build --haddock\''
complete -c stack -a new -d 'Create a new project from a template. Run \'stack templates\' to see available templates.'
complete -c stack -a templates -d 'List the templates available for \'stack new\'.'
complete -c stack -a init -d 'Initialize a stack project based on one or more cabal packages'
complete -c stack -a solver -d 'Use a dependency solver to try and determine missing extra-deps'
complete -c stack -a setup -d 'Get the appropriate GHC for your project'
complete -c stack -a path -d 'Print out handy path information'
complete -c stack -a unpack -d 'Unpack one or more packages locally'
complete -c stack -a update -d 'Update the package index'
complete -c stack -a upgrade -d 'Upgrade to the latest stack (experimental)'
complete -c stack -a upload -d 'Upload a package to Hackage'
complete -c stack -a sdist -d 'Create source distribution tarballs'
complete -c stack -a dot -d 'Visualize your project\'s dependency graph using Graphviz dot'
complete -c stack -a exec -d 'Execute a command'
complete -c stack -a ghc -d 'Run ghc'
complete -c stack -a ghci -d 'Run ghci in the context of package(s) (experimental)'
complete -c stack -a repl -d 'Run ghci in the context of package(s) (experimental) (alias for \'ghci\')'
complete -c stack -a runghc -d 'Run runghc'
complete -c stack -a runhaskell -d 'Run runghc (alias for \'runghc\')'
complete -c stack -a eval -d 'Evaluate some haskell code inline. Shortcut for \'stack exec ghc -- -e CODE\''
complete -c stack -a clean -d 'Clean the local packages'
complete -c stack -a list-dependencies -d 'List the dependencies'
complete -c stack -a query -d 'Query general build information (experimental)'
complete -c stack -a ide -d 'IDE-specific commands'
complete -c stack -a docker -d 'Subcommands specific to Docker use'
complete -c stack -a config -d 'Subcommands specific to modifying stack.yaml files'
complete -c stack -a image -d 'Subcommands specific to imaging (experimental)'
complete -c stack -a hpc -d 'Subcommands specific to Haskell Program Coverage'
complete -c stack -a sig -d 'Subcommands specific to package signatures (experimental)'

complete -c stack -n '__fish_seen_subcommand_from exec' -a "(ls $project_path/.stack-work/install/**/bin/)"
