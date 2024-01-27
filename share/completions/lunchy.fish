function __fish_lunchy_needs_command
    set -l cmd (commandline -xpc)

    if test (count $cmd) -eq 1
        return 0
    end

    return 1
end

function __fish_lunchy_using_command
    set -l cmd (commandline -xpc)
    set -l cmd_count (count $cmd)

    if test $cmd_count -lt 2
        return 1
    end

    for arg in $argv
        if test $arg = $cmd[2]
            return 0
        end
    end

    return 1
end

complete -f -c lunchy -s v -l verbose -d 'Show command executions'

# Commands
complete -f -c lunchy -n __fish_lunchy_needs_command -a install -d 'Installs [file] to ~/Library/LaunchAgents or /Library/LaunchAgents'
complete -f -c lunchy -n __fish_lunchy_needs_command -a 'ls list' -d 'Show the list of installed agents'
complete -f -c lunchy -n __fish_lunchy_needs_command -a start -d 'Start the first matching agent'
complete -f -c lunchy -n __fish_lunchy_needs_command -a stop -d 'Stop the first matching agent'
complete -f -c lunchy -n __fish_lunchy_needs_command -a restart -d 'Stop and start the first matching agent'
complete -f -c lunchy -n __fish_lunchy_needs_command -a status -d 'Show the PID and label for all agents'
complete -f -c lunchy -n __fish_lunchy_needs_command -a edit -d 'Opens the launchctl daemon file in the default editor'

# Commands with service completion
complete -f -c lunchy -n '__fish_lunchy_using_command ls list start stop restart status edit' -a '(lunchy ls)' -d Service

# Command: start
complete -f -c lunchy -n '__fish_lunchy_using_command start' -s w -l write -d 'Persist command'
complete -f -c lunchy -n '__fish_lunchy_using_command start' -s F -l force -d 'Force start (disabled) agents'

# Command: stop
complete -f -c lunchy -n '__fish_lunchy_using_command stop' -s w -l write -d 'Persist command'
