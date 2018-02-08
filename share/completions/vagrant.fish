# vagrant autocompletion

function __fish_vagrant_no_command -d 'Test if vagrant has yet to be given the main command'
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 1
end

function __fish_vagrant_using_command
    set -l cmd (commandline -opc)
    set -q cmd[2]
    and test "$argv[1]" = $cmd[2]
end

function __fish_vagrant_using_command_and_no_subcommand
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 2
    and test "$argv[1]" = "$cmd[2]"
end

function __fish_vagrant_using_subcommand --argument-names cmd_main cmd_sub
    set -l cmd (commandline -opc)
    set -q cmd[3]
    and test "$cmd_main" = $cmd[2] -a "$cmd_sub" = $cmd[3]
end

function __fish_vagrant_boxes -d 'Lists all available Vagrant boxes'
    command vagrant box list | while read -l name _
        echo $name
    end
end

# --version and --help are always active
complete -c vagrant -f -s v -l version -d 'Print the version and exit'
complete -c vagrant -f -s h -l help -d 'Print the help and exit'

# box
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'box' -d 'Manages boxes'
complete -c vagrant -n '__fish_vagrant_using_command_and_no_subcommand box' -a add -d 'Adds a box'
complete -c vagrant -f -n '__fish_vagrant_using_command_and_no_subcommand box' -a list -d 'Lists all the installed boxes'
complete -c vagrant -f -n '__fish_vagrant_using_command_and_no_subcommand box' -a remove -d 'Removes a box from Vagrant'
complete -c vagrant -f -n '__fish_vagrant_using_command_and_no_subcommand box' -a repackage -d 'Repackages the given box for redistribution'
complete -c vagrant -f -n '__fish_vagrant_using_subcommand box add' -l provider -d 'Verifies the box with the given provider'
complete -c vagrant -f -n '__fish_vagrant_using_subcommand box remove' -a '(__fish_vagrant_boxes)'

# connect
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'connect' -d 'Connect to a remotely shared Vagrant environment'
complete -c vagrant -f -n '__fish_vagrant_using_command connect' -l disable-static-ip -d 'No static IP, only a SOCKS proxy'
complete -c vagrant -f -n '__fish_vagrant_using_command connect' -l static-ip -r -d 'Manually override static IP chosen'
complete -c vagrant -f -n '__fish_vagrant_using_command connect' -l ssh -d 'SSH into the remote machine'


# destroy
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'destroy' -d 'Destroys the running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command destroy' -s f -l force -d 'Destroy without confirmation'

# global-status
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'global-status' -d 'Global status of Vagrant environments'
complete -c vagrant -f -n '__fish_vagrant_using_command global-status' -l prune -d 'Prune invalid entries'

# gem
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'gem' -d 'Install Vagrant plugins through ruby gems'

# halt
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'halt' -d 'Shuts down the running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command halt' -s f -l force -d 'Force shut down'

# init
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'init' -d 'Initializes the Vagrant environment'
complete -c vagrant -f -n '__fish_vagrant_using_command init' -a '(__fish_vagrant_boxes)'

# login
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'login' -d 'Log in to Vagrant Cloud'
complete -c vagrant -f -n '__fish_vagrant_using_command login ' -s c -l check -d "Only checks if you're logged in"
complete -c vagrant -f -n '__fish_vagrant_using_command login ' -s k -l logout -d "Logs you out if you're logged in"

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

# rdp
complete -c vagrant -n '__fish_vagrant_no_command' -a 'rdp' -d 'Connects to machine via RDP'

# reload
complete -c vagrant -f -n '__fish_vagrant_no_command' -a 'reload' -d 'Restarts the running machine'
complete -c vagrant -f -n '__fish_vagrant_using_command reload' -l no-provision -r -d 'Provisioners will not run'
complete -c vagrant -f -n '__fish_vagrant_using_command reload' -l provision-with -r -d 'Run only the given provisioners'

# resume
complete -c vagrant -x -n '__fish_vagrant_no_command' -a 'resume' -d 'Resumes a previously suspended machine'

# share
complete -c vagrant -n '__fish_vagrant_no_command' -a 'share' -d 'Share your Vagrant environment with anyone in the world'
complete -c vagrant -n '__fish_vagrant_using_command share' -l disable-http -d 'Disable publicly visible HTTP endpoint'
complete -c vagrant -n '__fish_vagrant_using_command share' -l domain -r -d 'Domain the share name will be a subdomain of'
complete -c vagrant -n '__fish_vagrant_using_command share' -l http -r -d 'Local HTTP port to forward to'
complete -c vagrant -n '__fish_vagrant_using_command share' -l https -r -d 'Local HTTPS port to forward to'
complete -c vagrant -n '__fish_vagrant_using_command share' -l name -r -d 'Specific name for the share'
complete -c vagrant -n '__fish_vagrant_using_command share' -l ssh -d "Allow 'vagrant connect --ssh' access"
complete -c vagrant -n '__fish_vagrant_using_command share' -l ssh-no-password -d "Key won't be encrypted with --ssh"
complete -c vagrant -n '__fish_vagrant_using_command share' -l ssh-port -r -d 'Specific port for SSH when using --ssh'
complete -c vagrant -n '__fish_vagrant_using_command share' -l ssh-once -d "Allow 'vagrant connect --ssh' only one time"

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
