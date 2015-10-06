# Completions for foreman
#
# https://github.com/ddollar/foreman

function __fish_foreman_no_command
  set cmd (commandline -opc)

  if [ (count $cmd) -eq 1 ]
    return 0
  end

  return 1
end

function __fish_foreman_using_command
  set cmd (commandline -opc)

  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end

  return 1
end

function __fish_foreman_export_formats
  echo bluepill daemon inittab launchd runit supervisord systemd upstart | tr ' ' '\n'
end

# options for all commands
complete -c foreman -s f -l procfile --description 'Specify an alternate Procfile to load'
complete -c foreman -s d -l root     --description 'Specify an alternate application root'

# check
complete -f -n '__fish_foreman_no_command' -c foreman -a check -d "Validate your application's Procfile"

# export
complete -f -n '__fish_foreman_no_command' -c foreman -a export -d 'Export the application to another process management format'
complete -f -n '__fish_foreman_using_command export' -c foreman -a "(__fish_foreman_export_formats)"
complete -f -n '__fish_foreman_using_command export' -c foreman -s a -l app -d "Use this name rather than the application's root directory name as the name of the application when exporting"
complete -f -n '__fish_foreman_using_command export' -c foreman -s l -l log -d 'Specify the directory to place process logs in'
complete -f -n '__fish_foreman_using_command export' -c foreman -s r -l run -d 'Specify the pid file directory, defaults to /var/run/<application>'
complete -f -n '__fish_foreman_using_command export' -c foreman -s e -l env -d 'Specify an environment file to load, defaults to .env'
complete -f -n '__fish_foreman_using_command export' -c foreman -s p -l port -d 'Specify which port to use as the base for this application'
complete -f -n '__fish_foreman_using_command export' -c foreman -s u -l user -d 'Specify the user the application should be run as, defaults to the app name'
complete -f -n '__fish_foreman_using_command export' -c foreman -s t -l template -d 'Specify an alternate template to use for creating export files'
complete -f -n '__fish_foreman_using_command export' -c foreman -s c -l concurrency -d 'Specify the number of each process type to run in the format process=num,process=num'

# help
complete -f -n '__fish_foreman_no_command' -c foreman -a 'help' -d 'Describe available commands or one specific command'
complete -f -n '__fish_foreman_using_command help' -c foreman -a 'check' -d "Validate your application's Procfile"
complete -r -n '__fish_foreman_using_command help' -c foreman -a 'export' -d 'Export the application to another process management format'
complete -f -n '__fish_foreman_using_command help' -c foreman -a 'run' -d "Run a command using your application's environment"
complete -f -n '__fish_foreman_using_command help' -c foreman -a 'start' -d "Start the application (or a specific process)"
complete -f -n '__fish_foreman_using_command help' -c foreman -a 'version' -d 'Display Foreman gem version'

# run
complete -f -n '__fish_foreman_no_command' -c foreman -a run -d "Run a command using your application's environment"
complete -f -n '__fish_foreman_using_command run' -c foreman -s e -l env -d 'Specify an environment file to load, defaults to .env'

# start
complete -f -n '__fish_foreman_no_command' -c foreman -a start -d "Start the application (or a specific process)"
complete -f -n '__fish_foreman_using_command start' -c foreman -s c -l color -d 'Force color to be enabled'
complete -f -n '__fish_foreman_using_command start' -c foreman -l no-color
complete -f -n '__fish_foreman_using_command start' -c foreman -s e -l env -d 'Specify an environment file to load, defaults to .env'
complete -f -n '__fish_foreman_using_command start' -c foreman -s m -l formation -d 'Specify the number of each process type to run, process=num,process=num'
complete -f -n '__fish_foreman_using_command start' -c foreman -s p -l port -d 'Specify which port to use as the base for this application'
complete -f -n '__fish_foreman_using_command start' -c foreman -s t -l timeout \
  -d 'Specify the amount of time (in seconds) processes have to shutdown gracefully before receiving a SIGKILL, defaults to 5'

# version
complete -f -n '__fish_foreman_no_command' -c foreman -a version -d 'Display Foreman gem version'
