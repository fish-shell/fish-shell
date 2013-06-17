# vagrant autocompletion

function __fish_vagrant_no_command --description 'Test if vagrant has yet to be given the main command'
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 ]
    return 0
  end
  return 1
end

function __fish_vagrant_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

function __fish_vagrant_using_subcommand
  set cmd (commandline -opc)
  set cmd_main $argv[1]
  set cmd_sub $argv[2]

  if [ (count $cmd) -gt 2 ]
    if [ $cmd_main = $cmd[2] ]; and [ $cmd_sub = $cmd[3] ]
      return 0
    end
  end
  return 1
end

function __fish_vagrant_boxes --description 'Lists all available Vagrant boxes'
  command vagrant box list
end

# --version and --help are always active
complete -c vagrant -f -s v -l version -d 'Print the version and exit'
complete -c vagrant -f -s h -l help -d 'Print the help and exit'

# box
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'box' -d 'Manages boxes'
complete -c vagrant -n '__fish_vagrant_using_command box' -a add -d 'Adds a box'
complete -c vagrant -f -n '__fish_vagrant_using_command box' -a list -d 'Lists all the installed boxes'
complete -c vagrant -f -n '__fish_vagrant_using_command box' -a remove -d 'Removes a box from Vagrant'
complete -c vagrant -f -n '__fish_vagrant_using_command box' -a repackage -d 'Repackages the given box for redistribution'
complete -c vagrant -f -n '__fish_vagrant_using_subcommand box add' -l provider -d 'Verifies the box with the given provider'
complete -c vagrant -f -n '__fish_vagrant_using_subcommand box remove' -a '(__fish_vagrant_boxes)'

# destroy
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'destroy' -d 'Destroys the running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command destroy' -s f -l force -d 'Destroy without confirmation'

# gem
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'gem' -d 'Install Vagrant plugins through ruby gems'

# halt
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'halt' -d 'Shuts down the running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command halt' -s f -l force -d 'Force shut down'

# init
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'init' -d 'Initializes the Vagrant environment'
complete -c vagrant -f -n '__fish_vagrant_using_command init' -a '(__fish_vagrant_boxes)'

# package
complete -c vagrant -n '__fish_vagrant_no_command' -a 'package' -d 'Packages a running machine into a reusable box'
complete -c vagrant -n '__fish_vagrant_using_command package' -l base -d 'Name or UUID of the virtual machine'
complete -c vagrant -n '__fish_vagrant_using_command package' -l output -d 'Output file name'
complete -c vagrant -n '__fish_vagrant_using_command package' -l include -r -d 'Additional files to package with the box'
complete -c vagrant -n '__fish_vagrant_using_command package' -l vagrantfile -r -d 'Include the given Vagrantfile with the box'

# plugin
complete -c vagrant -n '__fish_vagrant_no_command' -a 'plugin' -d 'Manages plugins'
complete -c vagrant -n '__fish_vagrant_using_command plugin' -a install -r -d 'Installs a plugin'
complete -c vagrant -n '__fish_vagrant_using_command plugin' -a license -r -d 'Installs a license for a proprietary Vagrant plugin'
complete -c vagrant -f -n '__fish_vagrant_using_command plugin' -a list -d 'Lists installed plugins'
complete -c vagrant -n '__fish_vagrant_using_command plugin' -a uninstall -r -d 'Uninstall the given plugin'

# provision
complete -c vagrant -n '__fish_vagrant_no_command' -a 'provision' -d 'Runs configured provisioners on the running machine'
complete -c vagrant -n '__fish_vagrant_using_command provision' -l provision-with -r -d 'Run only the given provisioners'

# reload
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'reload' -d 'Restarts the running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command reload' -l no-provision -r -d 'Provisioners will not run'
complete -c vagrant -f -n '__fish_vagrant_using_command reload' -l provision-with -r -d 'Run only the given provisioners'

# resume
complete -c vagrant -x -n '__fish_vagrant_no_command' -a 'resume' -d 'Resumes a previously suspended machine'

# ssh
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'ssh' -d 'SSH into a running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command ssh' -s c -l command -r -d 'Executes a single SSH command'
complete -c vagrant -f -n '__fish_vagrant_using_command ssh' -s p -l plain -r -d 'Does not authenticate'

# ssh-config
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'ssh-config' -d 'Outputs configuration for an SSH config file'
complete -c vagrant -f -n '__fish_vagrant_using_command ssh-config' -l host -r -d 'Name of the host for the outputted configuration'

# status
complete -c vagrant -x -n '__fish_vagrant_no_command' -a 'status' -d 'Status of the machines Vagrant is managing'

# suspend
complete -c vagrant -x -n '__fish_vagrant_no_command' -a 'suspend' -d 'Suspends the running machine'

# up
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'up' -d 'Creates and configures guest machines'
complete -c vagrant -f -n '__fish_vagrant_using_command up' -l provision -d 'Enable provision'
complete -c vagrant -f -n '__fish_vagrant_using_command up' -l no-provision -d 'Disable provision'
complete -c vagrant -f -n '__fish_vagrant_using_command up' -l provision-with -r -d 'Enable only certain provisioners, by type'
