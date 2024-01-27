# Completion for bundler

function __fish_bundle_no_command -d 'Test if bundle has been given no subcommand'
    set -l cmd (commandline -xpc)
    if test (count $cmd) -eq 1
        return 0
    end
    return 1
end

function __fish_bundle_using_command -d 'Test if bundle has been given a specific subcommand'
    set -l cmd (commandline -xpc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

function __fish_bundled_gems
    bundle list | sed '1 d' | sed -e 's/\*//g' -e 's/(.*)//g' -e 's/^ *//g' -e 's/ *$//g'
end

# Options for all commands
complete -c bundle -s r -l retry -d 'Specify the number of times you wish to attempt network commands'
complete -c bundle -l no-color -d 'Disable colorization in output'
complete -c bundle -s V -l verbose -d 'Enable verbose output mode'

# No command
complete -f -n __fish_bundle_no_command -c bundle -l help -d 'Display a help page'
complete -c bundle -s v -l version -d 'Prints version information'

##
# Primary Commands
##

# Install
complete -f -n __fish_bundle_no_command -c bundle -a install -d 'Install the gems specified by the Gemfile or Gemfile.lock'
complete -f -n '__fish_bundle_using_command install' -c bundle -l gemfile -d 'The location of the Gemfile bundler should use'
complete -f -n '__fish_bundle_using_command install' -c bundle -l path -d 'The location to install the gems in the bundle to'
complete -f -n '__fish_bundle_using_command install' -c bundle -l system -d 'Installs the gems in the bundle to the system location'
complete -f -n '__fish_bundle_using_command install' -c bundle -l without -d 'A space-separated list of groups to skip installing'
complete -f -n '__fish_bundle_using_command install' -c bundle -l local -d 'Use cached gems instead of connecting to rubygems.org'
complete -f -n '__fish_bundle_using_command install' -c bundle -l deployment -d "Switches bundler's defaults into deployment mode."
complete -f -n '__fish_bundle_using_command install' -c bundle -l binstubs -d 'Create a directory containing executabes that run in the context of the bundle'
complete -f -n '__fish_bundle_using_command install' -c bundle -l shebang -d 'Specify a ruby executable to use with generated binstubs'
complete -f -n '__fish_bundle_using_command install' -c bundle -l standalone -d 'Make a bundle that can work without RubyGems or Bundler at run-time'
complete -f -n '__fish_bundle_using_command install' -c bundle -s P -l trust-policy -d 'Apply a RubyGems security policy: {High,Medium,Low,No}Security'
complete -f -n '__fish_bundle_using_command install' -c bundle -s j -l jobs -d 'Install gems parallelly by starting size number of parallel workers'
complete -f -n '__fish_bundle_using_command install' -c bundle -l no-cache -d 'Do not update the cache in vendor/cache with the newly bundled gems'
complete -f -n '__fish_bundle_using_command install' -c bundle -l quiet -d 'Do not print progress information to stdout'
complete -f -n '__fish_bundle_using_command install' -c bundle -l clean -d 'Run bundle clean automatically after install'
complete -f -n '__fish_bundle_using_command install' -c bundle -l full-index -d 'Use the rubygems modern index instead of the API endpoint'
complete -f -n '__fish_bundle_using_command install' -c bundle -l no-prune -d 'Do not remove stale gems from the cache'
complete -f -n '__fish_bundle_using_command install' -c bundle -l frozen -d 'Do not allow the Gemfile.lock to be updated after this install'

# Update
complete -f -n __fish_bundle_no_command -c bundle -a update -d 'Update dependencies to their latest versions'
complete -f -n '__fish_bundle_using_command update' -c bundle -l source -d 'The name of a :git or :path source used in the Gemfile'
complete -f -n '__fish_bundle_using_command update' -c bundle -l local -d 'Do not attempt to fetch gems remotely and use the gem cache instead'
complete -f -n '__fish_bundle_using_command update' -c bundle -l quiet -d 'Only output warnings and errors'
complete -f -n '__fish_bundle_using_command update' -c bundle -l full-index -d 'Use the rubygems modern index instead of the API endpoint'
complete -f -n '__fish_bundle_using_command update' -c bundle -s j -l jobs -d 'Specify the number of jobs to run in parallel'
complete -f -n '__fish_bundle_using_command update' -c bundle -s g -l group -d 'Update a specific group'
complete -f -n '__fish_bundle_using_command update' -c bundle -a '(__fish_bundled_gems)'

# Package
complete -f -n __fish_bundle_no_command -c bundle -a package -d 'Package the .gem files into vendor/cache directory'

# Binstubs
complete -f -n __fish_bundle_no_command -c bundle -a binstubs -d 'Install the binstubs of the listed gem'
complete -f -n '__fish_bundle_using_command binstubs' -c bundle -l path -d 'Binstub destination directory (default bin)'
complete -f -n '__fish_bundle_using_command binstubs' -c bundle -l force -d 'Overwrite existing binstubs if they exist'
complete -f -n '__fish_bundle_using_command binstubs' -c bundle -a '(__fish_bundled_gems)'

# Exec
complete -f -n __fish_bundle_no_command -c bundle -a exec -d 'Execute a script in the context of the current bundle'
complete -f -n '__fish_bundle_using_command exec' -c bundle -l keep-file-descriptors -d 'Exec runs a command, providing it access to the gems in the bundle'

# Help
complete -f -n __fish_bundle_no_command -c bundle -a help -d 'Describe available tasks or one specific task'
complete -f -n '__fish_bundle_using_command help' -c bundle -a install -d 'Install the gems specified by the Gemfile or Gemfile.lock'
complete -f -n '__fish_bundle_using_command help' -c bundle -a update -d 'Update dependencies to their latest versions'
complete -f -n '__fish_bundle_using_command help' -c bundle -a package -d 'Package .gem files into the vendor/cache directory'
complete -f -n '__fish_bundle_using_command help' -c bundle -a exec -d 'Execute a script in the context of the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a config -d 'Specify and read configuration options for bundler'
complete -f -n '__fish_bundle_using_command help' -c bundle -a check -d 'Check bundler requirements for your application'
complete -f -n '__fish_bundle_using_command help' -c bundle -a list -d 'Show all of the gems in the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a show -d 'Show the source location of a particular gem in the bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a outdated -d 'Show all of the outdated gems in the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a console -d 'Start an IRB session in the context of the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a open -d 'Open an installed gem in your $EDITOR'
complete -f -n '__fish_bundle_using_command help' -c bundle -a viz -d 'Generate a visual representation of your dependencies'
complete -f -n '__fish_bundle_using_command help' -c bundle -a init -d 'Generate a simple Gemfile'
complete -f -n '__fish_bundle_using_command help' -c bundle -a gem -d 'Create a simple gem, suitable for development with bundler'
complete -f -n '__fish_bundle_using_command help' -c bundle -a platform -d 'Displays platform compatibility information'
complete -f -n '__fish_bundle_using_command help' -c bundle -a cleanup -d 'Cleans up unused gems in your bundler directory'

##
# Utilities
##

# Check
complete -f -n __fish_bundle_no_command -c bundle -a check -d 'Check if the requirements are installed and available to bundler'
complete -f -n '__fish_bundle_using_command check' -c bundle -l gemfile -d 'The location of the Gemfile bundler should use'
complete -f -n '__fish_bundle_using_command check' -c bundle -l path -d 'Specify a path other than the system default (BUNDLE_PATH or GEM_HOME)'
complete -f -n '__fish_bundle_using_command check' -c bundle -l dry-run -d 'Lock the Gemfile'

# List
complete -f -n __fish_bundle_no_command -c bundle -a list -d 'Show lists the names and versions of all gems that are required by your Gemfile'
complete -f -n '__fish_bundle_using_command list' -c bundle -l paths -d 'List the paths of all gems required by your Gemfile'
complete -f -n '__fish_bundle_using_command list' -c bundle -a '(__fish_bundled_gems)'

# Show
complete -f -n __fish_bundle_no_command -c bundle -a show -d 'Show lists the names and versions of all gems that are required by your Gemfile'
complete -f -n '__fish_bundle_using_command show' -c bundle -l paths -d 'List the paths of all gems required by your Gemfile'
complete -f -n '__fish_bundle_using_command show' -c bundle -a '(__fish_bundled_gems)'

# Outdated
complete -f -n __fish_bundle_no_command -c bundle -a outdated -d 'Show all of the outdated gems in the current bundle'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l pre -d 'Check for newer pre-release gems'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l source -d 'Check against a specific source'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l local -d 'Use cached gems instead of attempting to fetch gems remotely'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l strict -d 'Only list newer versions allowed by your Gemfile requirements'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -a '(__fish_bundled_gems)'

# Console
complete -f -n __fish_bundle_no_command -c bundle -a console -d 'Start an IRB session in the context of the current bundle'

# Open
complete -f -n __fish_bundle_no_command -c bundle -a open -d 'Open an installed gem in your $EDITOR'
complete -f -n '__fish_bundle_using_command open' -c bundle -a '(__fish_bundled_gems)'

# Viz
complete -f -n __fish_bundle_no_command -c bundle -a viz -d 'Generate a visual representation of your dependencies'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s f -l file -d 'The name to use for the generated file (see format option)'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s v -l version -d 'Show each gem version'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s r -l requirements -d 'Show the version of each required dependency'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s F -l format -d 'Output a specific format (png, jpg, svg, dot, …)'

# Init
complete -f -n __fish_bundle_no_command -c bundle -a init -d 'Generate a simple Gemfile, placed in the current directory'
complete -f -n '__fish_bundle_using_command init' -c bundle -l gemspec -d 'Use a specified .gemspec to create the Gemfile'

# Gem
complete -f -n __fish_bundle_no_command -c bundle -a gem -d 'Create a simple gem, suitable for development with bundler'
complete -f -n '__fish_bundle_using_command gem' -c bundle -s b -l bin -d 'Generate a binary for your library'
complete -f -n '__fish_bundle_using_command gem' -c bundle -s t -l test -d 'Generate a test directory for your library (rspec or minitest)'
complete -f -n '__fish_bundle_using_command gem' -c bundle -s e -l edit -d 'Path to your editor'
complete -f -n '__fish_bundle_using_command gem' -c bundle -l ext -d 'Generate the boilerplate for C extension code'

# Platform
complete -f -n __fish_bundle_no_command -c bundle -a platform -d 'Displays platform compatibility information'
complete -f -n '__fish_bundle_using_command platform' -c bundle -l ruby -d 'Only display Ruby directive information'

# Clean
complete -f -n __fish_bundle_no_command -c bundle -a clean -d 'Cleans up unused gems in your bundler directory'
complete -f -n '__fish_bundle_using_command clean' -c bundle -l dry-run -d 'Only print out changes, do not actually clean gems'
complete -f -n '__fish_bundle_using_command clean' -c bundle -l force -d 'Forces clean even if --path is not set'

# Cache
complete -f -n __fish_bundle_no_command -c bundle -a cache -d 'Cache all the gems to vendor/cache'
complete -f -n '__fish_bundle_using_command cache' -c bundle -l no-prune -d 'Do not remove stale gems from the cache'
complete -f -n '__fish_bundle_using_command cache' -c bundle -l all -d 'Include all sources (including path and git)'

# Licenses
complete -f -n __fish_bundle_no_command -c bundle -a licenses -d 'Prints the license of all gems in the bundle'

# Env
complete -f -n __fish_bundle_no_command -c bundle -a env -d 'Print information about the environment Bundler is running under'
