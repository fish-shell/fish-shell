function __fish_composer_needs_command
  set cmd (commandline -opc)

  if [ (count $cmd) -eq 1 ]
    return 0
  end

  return 1
end

function __fish_composer_using_command
  set cmd (commandline -opc)

  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end

  return 1
end

#add cmds list
set --local composer_cmds 'about' 'archive' 'browse' 'clear-cache' 'clearcache' 'config' 'create-project' 'depends' 'diagnose' 'dump-autoload' 'dumpautoload' 'global' 'help' 'home' 'init' 'install' 'licenses' 'list' 'remove' 'require' 'run-script' 'search' 'self-update' 'selfupdate' 'show' 'status' 'update' 'validate'

#help
complete -f -c composer -n '__fish_composer_using_command help' -a "$composer_cmds"

#popisky
complete -f -c composer -n '__fish_composer_needs_command' -a 'about' -d 'Short information about Composer'
complete -f -c composer -n '__fish_composer_needs_command' -a 'archive' -d 'Create an archive of this composer package'
complete -f -c composer -n '__fish_composer_needs_command' -a 'browse' -d 'Opens the package\'s repository URL or homepage in your browser.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'clear-cache' -d 'Clears composer\'s internal package cache.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'clearcache' -d 'Clears composer\'s internal package cache.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'config' -d 'Set config options'
complete -f -c composer -n '__fish_composer_needs_command' -a 'create-project' -d 'Create new project from a package into given directory.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'depends' -d 'Shows which packages depend on the given package'
complete -f -c composer -n '__fish_composer_needs_command' -a 'diagnose' -d 'Diagnoses the system to identify common errors.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'dump-autoload' -d 'Dumps the autoloader'
complete -f -c composer -n '__fish_composer_needs_command' -a 'dumpautoload' -d 'Dumps the autoloader'
complete -f -c composer -n '__fish_composer_needs_command' -a 'global' -d 'Allows running commands in the global composer dir (\$COMPOSER_HOME).'
complete -f -c composer -n '__fish_composer_needs_command' -a 'help' -d 'Displays help for a command'
complete -f -c composer -n '__fish_composer_needs_command' -a 'home' -d 'Opens the package\'s repository URL or homepage in your browser.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'init' -d 'Creates a basic composer.json file in current directory.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'install' -d 'Installs the project dependencies from the composer.lock file if present, or falls back on the composer.json.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'licenses' -d 'Show information about licenses of dependencies'
complete -f -c composer -n '__fish_composer_needs_command' -a 'list' -d 'Lists commands'
complete -f -c composer -n '__fish_composer_needs_command' -a 'remove' -d 'Removes a package from the require or require-dev'
complete -f -c composer -n '__fish_composer_needs_command' -a 'require' -d 'Adds required packages to your composer.json and installs them'
complete -f -c composer -n '__fish_composer_needs_command' -a 'run-script' -d 'Run the scripts defined in composer.json.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'search' -d 'Search for packages'
complete -f -c composer -n '__fish_composer_needs_command' -a 'self-update' -d 'Updates composer.phar to the latest version.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'selfupdate' -d 'Updates composer.phar to the latest version.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'show' -d 'Show information about packages'
complete -f -c composer -n '__fish_composer_needs_command' -a 'status' -d 'Show a list of locally modified packages'
complete -f -c composer -n '__fish_composer_needs_command' -a 'update' -d 'Updates your dependencies to the latest version according to composer.json, and updates the composer.lock file.'
complete -f -c composer -n '__fish_composer_needs_command' -a 'validate' -d 'Validates a composer.json'

complete -f -c composer -n '__fish_composer_needs_command' -s 'h' -l 'help' -d 'Displays composer\'s help.'
complete -f -c composer -n '__fish_composer_needs_command' -s 'q' -l 'quiet' -d 'Do not output any message.'
complete -f -c composer -n '__fish_composer_needs_command' -s 'v' -l 'verbose' -d 'Increase the verbosity of messages: 1 for normal output (-v), 2 for more verbose output (-vv) and 3 for debug (-vvv).'
complete -f -c composer -n '__fish_composer_needs_command' -s 'V' -l 'version' -d 'Display composer\'s application version.'
complete -f -c composer -n '__fish_composer_needs_command' -l 'ansi' -d 'Force ANSI output.'
complete -f -c composer -n '__fish_composer_needs_command' -l 'np-ansi' -d 'Disable ANSI output.'
complete -f -c composer -n '__fish_composer_needs_command' -s 'n' -l 'no-interaction' -d 'Do not ask any interactive question.'
complete -f -c composer -n '__fish_composer_needs_command' -l 'profile' -d 'Display timing and memory usage information.'
complete -f -c composer -n '__fish_composer_needs_command' -s 'd' -l 'working-dir' -d 'If specified, use the given directory as working directory.'

