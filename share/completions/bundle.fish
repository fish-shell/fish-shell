# Completion for bundler

function __fish_bundle_no_command --description 'Test if bundle has been given no subcommand'
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 ]
    return 0
  end
  return 1
end

function __fish_bundle_using_command --description 'Test if bundle has been given a specific subcommand'
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

function __fish_bundled_gems
  bundle list | sed '1 d' | sed -e 's/\*//g' -e 's/(.*)//g' -e 's/^ *//g' -e 's/ *$//g'
end

# Options for all commands
complete -c bundle -s r -l retry --description 'Specify the number of times you wish to attempt network commands'
complete -c bundle -l no-color --description 'Disable colorization in output'
complete -c bundle -s V -l verbose --description 'Enable verbose output mode'

# No command
complete -f -n '__fish_bundle_no_command' -c bundle -l help --description 'Display a help page'
complete -c bundle -s v -l version --description 'Prints version information'

##
# Primary Commands
##

# Install
complete -f -n '__fish_bundle_no_command' -c bundle -a 'install' --description 'Install the gems specified by the Gemfile or Gemfile.lock'
complete -f -n '__fish_bundle_using_command install' -c bundle -l gemfile --description 'The location of the Gemfile bundler should use'
complete -f -n '__fish_bundle_using_command install' -c bundle -l path --description 'The location to install the gems in the bundle to'
complete -f -n '__fish_bundle_using_command install' -c bundle -l system --description 'Installs the gems in the bundle to the system location'
complete -f -n '__fish_bundle_using_command install' -c bundle -l without --description 'A space-separated list of groups to skip installing'
complete -f -n '__fish_bundle_using_command install' -c bundle -l local --description 'Use cached gems instead of connecting to rubygems.org'
complete -f -n '__fish_bundle_using_command install' -c bundle -l deployment --description "Switches bundler's defaults into deployment mode."
complete -f -n '__fish_bundle_using_command install' -c bundle -l binstubs --description 'Create a directory containing executabes that run in the context of the bundle'
complete -f -n '__fish_bundle_using_command install' -c bundle -l shebang --description 'Specify a ruby executable to use with generated binstubs'
complete -f -n '__fish_bundle_using_command install' -c bundle -l standalone --description 'Make a bundle that can work without RubyGems or Bundler at run-time'
complete -f -n '__fish_bundle_using_command install' -c bundle -s P -l trust-policy --description 'Apply a RubyGems security policy: {High,Medium,Low,No}Security'
complete -f -n '__fish_bundle_using_command install' -c bundle -s j -l jobs --description 'Install  gems parallely by starting size number of parallel workers'
complete -f -n '__fish_bundle_using_command install' -c bundle -l no-cache --description 'Do not update the cache in vendor/cache with the newly bundled gems'
complete -f -n '__fish_bundle_using_command install' -c bundle -l quiet --description 'Do not print progress information to stdout'
complete -f -n '__fish_bundle_using_command install' -c bundle -l clean --description 'Run bundle clean automatically after install'
complete -f -n '__fish_bundle_using_command install' -c bundle -l full-index --description 'Use the rubygems modern index instead of the API endpoint'
complete -f -n '__fish_bundle_using_command install' -c bundle -l no-prune --description 'Do not remove stale gems from the cache'
complete -f -n '__fish_bundle_using_command install' -c bundle -l frozen --description 'Do not allow the Gemfile.lock to be updated after this install'

# Update
complete -f -n '__fish_bundle_no_command' -c bundle -a 'update'  --description 'Update dependencies to their latest versions'
complete -f -n '__fish_bundle_using_command update' -c bundle -l source --description 'The name of a :git or :path source used in the Gemfile'
complete -f -n '__fish_bundle_using_command update' -c bundle -l local --description 'Do not attempt to fetch gems remotely and use the gem cache instead'
complete -f -n '__fish_bundle_using_command update' -c bundle -l quiet --description 'Only output warnings and errors'
complete -f -n '__fish_bundle_using_command update' -c bundle -l full-index --description 'Use the rubygems modern index instead of the API endpoint'
complete -f -n '__fish_bundle_using_command update' -c bundle -s j -l jobs --description 'Specify the number of jobs to run in parallel'
complete -f -n '__fish_bundle_using_command update' -c bundle -s g -l group --description 'Update a specific group'
complete -f -n '__fish_bundle_using_command update' -c bundle -a '(__fish_bundled_gems)'

# Package
complete -f -n '__fish_bundle_no_command' -c bundle -a 'package' --description 'Package the .gem files required by your application into the vendor/cache directory'

# Binstubs
complete -f -n '__fish_bundle_no_command' -c bundle -a 'binstubs' --description 'Install the binstubs of the listed gem'
complete -f -n '__fish_bundle_using_command binstubs' -c bundle -l path --description 'Binstub destination directory (default bin)'
complete -f -n '__fish_bundle_using_command binstubs' -c bundle -l force --description 'Overwrite existing binstubs if they exist'
complete -f -n '__fish_bundle_using_command binstubs' -c bundle -a '(__fish_bundled_gems)'

# Exec
complete -f -n '__fish_bundle_no_command' -c bundle -a 'exec' --description 'Execute a script in the context of the current bundle'
complete -f -n '__fish_bundle_using_command exec' -c bundle -l keep-file-descriptors --description 'Exec runs a command, providing it access to the gems in the bundle'

# Help
complete -f -n '__fish_bundle_no_command' -c bundle -a 'help' --description 'Describe available tasks or one specific task'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'install' --description 'Install the gems specified by the Gemfile or Gemfile.lock'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'update' --description 'Update dependencies to their latest versions'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'package' --description 'Package .gem files into the vendor/cache directory'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'exec' --description 'Execute a script in the context of the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'config' --description 'Specify and read configuration options for bundler'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'check' --description 'Check bundler requirements for your application'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'list' --description 'Show all of the gems in the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'show' --description 'Show the source location of a particular gem in the bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'outdated' --description 'Show all of the outdated gems in the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'console' --description 'Start an IRB session in the context of the current bundle'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'open' --description 'Open an installed gem in your $EDITOR'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'viz' --description 'Generate a visual representation of your dependencies'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'init' --description 'Generate a simple Gemfile'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'gem' --description 'Create a simple gem, suitable for development with bundler'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'platform' --description 'Displays platform compatibility information'
complete -f -n '__fish_bundle_using_command help' -c bundle -a 'cleanup' --description 'Cleans up unused gems in your bundler directory'

##
# Utilities
##

# Check
complete -f -n '__fish_bundle_no_command' -c bundle -a 'check' --description 'Determine whether the requirements for your application are installed and available to bundler'
complete -f -n '__fish_bundle_using_command check' -c bundle -l gemfile --description 'The location of the Gemfile bundler should use'
complete -f -n '__fish_bundle_using_command check' -c bundle -l path --description 'Specify a path other than the system default (BUNDLE_PATH or GEM_HOME)'
complete -f -n '__fish_bundle_using_command check' -c bundle -l dry-run --description 'Lock the Gemfile'

# List
complete -f -n '__fish_bundle_no_command' -c bundle -a 'list' --description 'Show lists the names and versions of all gems that are required by your Gemfile'
complete -f -n '__fish_bundle_using_command list' -c bundle -l paths --description 'List the paths of all gems required by your Gemfile'
complete -f -n '__fish_bundle_using_command list' -c bundle -a '(__fish_bundled_gems)'

# Show
complete -f -n '__fish_bundle_no_command' -c bundle -a 'show' --description 'Show lists the names and versions of all gems that are required by your Gemfile'
complete -f -n '__fish_bundle_using_command show' -c bundle -l paths --description 'List the paths of all gems required by your Gemfile'
complete -f -n '__fish_bundle_using_command show' -c bundle -a '(__fish_bundled_gems)'

# Outdated
complete -f -n '__fish_bundle_no_command' -c bundle -a 'outdated' --description 'Show all of the outdated gems in the current bundle'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l pre --description 'Check for newer pre-release gems'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l source --description 'Check against a specific source'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l local --description 'Use cached gems instead of attempting to fetch gems remotely'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -l strict --description 'Only list newer versions allowed by your Gemfile requirements'
complete -f -n '__fish_bundle_using_command outdated' -c bundle -a '(__fish_bundled_gems)'

# Console
complete -f -n '__fish_bundle_no_command' -c bundle -a 'console' --description 'Start an IRB session in the context of the current bundle'

# Open
complete -f -n '__fish_bundle_no_command' -c bundle -a 'open' --description 'Open an installed gem in your $EDITOR'
complete -f -n '__fish_bundle_using_command open' -c bundle -a '(__fish_bundled_gems)'

# Viz
complete -f -n '__fish_bundle_no_command' -c bundle -a 'viz' --description 'Generate a visual representation of your dependencies'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s f -l file --description 'The name to use for the generated file (see format option)'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s v -l version --description 'Show each gem version'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s r -l requirements --description 'Show the version of each required dependency'
complete -f -n '__fish_bundle_using_command viz' -c bundle -s F -l format --description 'Output a specific format (png, jpg, svg, dot, ...)'

# Init
complete -f -n '__fish_bundle_no_command' -c bundle -a 'init' --description 'Generate a simple Gemfile, placed in the current directory'
complete -f -n '__fish_bundle_using_command init' -c bundle -l gemspec --description 'Use a specified .gemspec to create the Gemfile'

# Gem
complete -f -n '__fish_bundle_no_command' -c bundle -a 'gem' --description 'Create a simple gem, suitable for development with bundler'
complete -f -n '__fish_bundle_using_command gem' -c bundle -s b -l bin  --description 'Generate a binary for your library'
complete -f -n '__fish_bundle_using_command gem' -c bundle -s t -l test --description 'Generate a test directory for your library (rspec or minitest)'
complete -f -n '__fish_bundle_using_command gem' -c bundle -s e -l edit --description 'Path to your editor'
complete -f -n '__fish_bundle_using_command gem' -c bundle -l ext --description 'Generate the boilerplate for C extension code'

# Platform
complete -f -n '__fish_bundle_no_command' -c bundle -a 'platform' --description 'Displays platform compatibility information'
complete -f -n '__fish_bundle_using_command platform' -c bundle -l ruby --description 'Only display Ruby directive information'

# Clean
complete -f -n '__fish_bundle_no_command' -c bundle -a 'clean'    --description 'Cleans up unused gems in your bundler directory'
complete -f -n '__fish_bundle_using_command clean' -c bundle -l dry-run --description 'Only print out changes, do not actually clean gems'
complete -f -n '__fish_bundle_using_command clean' -c bundle -l force --description 'Forces clean even if --path is not set'

# Cache
complete -f -n '__fish_bundle_no_command' -c bundle -a 'cache' --description 'Cache all the gems to vendor/cache'
complete -f -n '__fish_bundle_using_command cache' -c bundle -l no-prune --description 'Do not remove stale gems from the cache'
complete -f -n '__fish_bundle_using_command cache' -c bundle -l all --description 'Include all sources (including path and git)'

# Licenses
complete -f -n '__fish_bundle_no_command' -c bundle -a 'licenses' --description 'Prints the license of all gems in the bundle'

# Env
complete -f -n '__fish_bundle_no_command' -c bundle -a 'env' --description 'Print information about the environment Bundler is running under'
