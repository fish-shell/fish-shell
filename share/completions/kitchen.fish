# Fish Shell command-line completions for Test Kitchen

function __fish_kitchen_no_command -d 'Test if kitchen has yet to be given the main command'
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 1
end

function __fish_kitchen_using_command
    set -l cmd (commandline -opc)
    set -q cmd[2]; and test "$argv[1]" = $cmd[2]
end

function __fish_kitchen_using_command_and_no_subcommand
    set -l cmd (commandline -opc)
    test (count $cmd) -eq 2; and test "$argv[1]" = "$cmd[2]"
end

function __fish_kitchen_using_subcommand --argument-names cmd_main cmd_sub
    set -l cmd (commandline -opc)
    set -q cmd[3]; and test "$cmd_main" = $cmd[2] -a "$cmd_sub" = $cmd[3]
end

function __fish_kitchen_list
    command kitchen list --bare
end


# help commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a help -d "Describe available commands or one specific command"

# help help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a help -d "Describe available commands or one specific command"

# list commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a list -d "Lists one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command list' -s "b" -l "bare" -d "List the name of each instance only, one per line"
complete -c kitchen -f -n '__fish_kitchen_using_command list' -s "d" -l "debug" -d "[Deprecated] Please use `kitchen diagnose'"
complete -c kitchen -f -n '__fish_kitchen_using_command list' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# list help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a list -d "Lists one or more instances"

# list instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command list' -a '(__fish_kitchen_list)'

# diagnose commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a diagnose -d "Show computed diagnostic configuration"
complete -c kitchen -f -n '__fish_kitchen_using_command diagnose' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"
complete -c kitchen -f -n '__fish_kitchen_using_command diagnose' -l "loader" -d "Include data loader diagnostics"
complete -c kitchen -f -n '__fish_kitchen_using_command diagnose' -l "instances" -d "Include instances diagnostics"
complete -c kitchen -f -n '__fish_kitchen_using_command diagnose' -l "all" -d "Include all diagnostics"

# diagnose help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a diagnose -d "Show computed diagnostic configuration"

# diagnose instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command diagnose' -a '(__fish_kitchen_list)'

# create commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a create -d "Change instance state to create. Start one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command create' -s "c" -l "concurrency" -d "Run a create against all matching instances concurrently. Only N instances will run at the same time if a number is given. "
complete -c kitchen -f -n '__fish_kitchen_using_command create' -s "p" -l "parallel" -d "[Future DEPRECATION, use --concurrency] Run a create against all matching instances concurrently. "
complete -c kitchen -f -n '__fish_kitchen_using_command create' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# create help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a create -d "Change instance state to create. Start one or more instances"

# create instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command create' -a '(__fish_kitchen_list)'

# converge commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a converge -d "Change instance state to converge. Use a provisioner to configure one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command converge' -s "c" -l "concurrency" -d "Run a converge against all matching instances concurrently. Only N instances will run at the same time if a number is given. "
complete -c kitchen -f -n '__fish_kitchen_using_command converge' -s "p" -l "parallel" -d "[Future DEPRECATION, use --concurrency] Run a converge against all matching instances concurrently. "
complete -c kitchen -f -n '__fish_kitchen_using_command converge' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# converge help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a converge -d "Change instance state to converge. Use a provisioner to configure one or more instances"

# converge instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command converge' -a '(__fish_kitchen_list)'

# setup commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a setup -d "Change instance state to setup. Prepare to run automated tests. Install busser and related gems on one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command setup' -s "c" -l "concurrency" -d "Run a setup against all matching instances concurrently. Only N instances will run at the same time if a number is given. "
complete -c kitchen -f -n '__fish_kitchen_using_command setup' -s "p" -l "parallel" -d "[Future DEPRECATION, use --concurrency] Run a setup against all matching instances concurrently. "
complete -c kitchen -f -n '__fish_kitchen_using_command setup' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# setup help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a setup -d "Change instance state to setup. Prepare to run automated tests. Install busser and related gems on one or more instances"

# setup instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command setup' -a '(__fish_kitchen_list)'

# verify commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a verify -d "Change instance state to verify. Run automated tests on one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command verify' -s "c" -l "concurrency" -d "Run a verify against all matching instances concurrently. Only N instances will run at the same time if a number is given. "
complete -c kitchen -f -n '__fish_kitchen_using_command verify' -s "p" -l "parallel" -d "[Future DEPRECATION, use --concurrency] Run a verify against all matching instances concurrently. "
complete -c kitchen -f -n '__fish_kitchen_using_command verify' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# verify help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a verify -d "Change instance state to verify. Run automated tests on one or more instances"

# verify instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command verify' -a '(__fish_kitchen_list)'

# destroy commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a destroy -d "Change instance state to destroy. Delete all information for one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command destroy' -s "c" -l "concurrency" -d "Run a destroy against all matching instances concurrently. Only N instances will run at the same time if a number is given. "
complete -c kitchen -f -n '__fish_kitchen_using_command destroy' -s "p" -l "parallel" -d "[Future DEPRECATION, use --concurrency] Run a destroy against all matching instances concurrently. "
complete -c kitchen -f -n '__fish_kitchen_using_command destroy' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# destroy help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a destroy -d "Change instance state to destroy. Delete all information for one or more instances"

# destroy instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command destroy' -a '(__fish_kitchen_list)'

# test commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a test -d "Test (destroy, create, converge, setup, verify and destroy) one or more instances"
complete -c kitchen -f -n '__fish_kitchen_using_command test' -s "c" -l "concurrency" -d "Run a test against all matching instances concurrently. Only N instances will run at the same time if a number is given. "
complete -c kitchen -f -n '__fish_kitchen_using_command test' -s "p" -l "parallel" -d "[Future DEPRECATION, use --concurrency] Run a test against all matching instances concurrently. "
complete -c kitchen -f -n '__fish_kitchen_using_command test' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"
complete -c kitchen -f -n '__fish_kitchen_using_command test' -s "d" -l "destroy" -d "Destroy strategy to use after testing (passing, always, never)."
complete -c kitchen -f -n '__fish_kitchen_using_command test' -l "auto_init" -d "Invoke init command if .kitchen.yml is missing"

# test help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a test -d "Test (destroy, create, converge, setup, verify and destroy) one or more instances"

# test instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command test' -a '(__fish_kitchen_list)'

# login commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a login -d "Log in to one instance"
complete -c kitchen -f -n '__fish_kitchen_using_command login' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"

# login help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a login -d "Log in to one instance"

# login instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command login' -a '(__fish_kitchen_list)'

# exec commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a exec -d "Execute command on one or more instance"
complete -c kitchen -f -n '__fish_kitchen_using_command exec' -s "l" -l "log_level" -d "Set the log level (debug, info, warn, error, fatal)"
complete -c kitchen -f -n '__fish_kitchen_using_command exec' -s "c" -l command -d "execute via ssh"

# exec help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a exec -d "Execute command on one or more instance"

# exec instance-based commands
complete -c kitchen -f -n '__fish_kitchen_using_command exec' -a '(__fish_kitchen_list)'

# version commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a version -d "Print Kitchen's version information"

# version help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a version -d "Print Kitchen's version information"

# sink commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a sink -d "Show the Kitchen sink!"

# sink help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a sink -d "Show the Kitchen sink!"

# console commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a console -d "Kitchen Console!"

# console help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a console -d "Kitchen Console!"

# init commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a init -d "Adds some configuration to your cookbook so Kitchen can rock"
complete -c kitchen -f -n '__fish_kitchen_using_command init' -s "D" -l "driver" -d "One or more Kitchen Driver gems to be installed or added to a Gemfile "
complete -c kitchen -f -n '__fish_kitchen_using_command init' -s "P" -l "provisioner" -d "The default Kitchen Provisioner to use "
complete -c kitchen -f -n '__fish_kitchen_using_command init' -l "create_gemfile" -d "Whether or not to create a Gemfile if one does not exist. Default: false "

# init help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a init -d "Adds some configuration to your cookbook so Kitchen can rock"

# driver commands
complete -c kitchen -f -n '__fish_kitchen_no_command' -a driver -d "Driver subcommands"

# driver help
complete -c kitchen -f -n '__fish_kitchen_using_command help' -a driver -d "Driver subcommands"
