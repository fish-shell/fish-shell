__fish_make_completion_signals

complete -c timeout -l foreground -d 'Run COMMAND in the foreground'
complete -c timeout -s k -l kill-after -d 'Send a KILL signal after DURATION' -x
complete -c timeout -s s -l signal -d 'Specify the signal to be sent' -xa "$__kill_signals"
complete -c timeout -l preserve-status -d 'Exit with same status as COMMAND'

complete -c timeout -r -a '(__fish_complete_command)' -d 'Specify which command to run'

# GNU coreutils ver
if timeout --version &>/dev/null
    complete -c timeout -l help -d 'Display this help and exit'
    complete -c timeout -l version -d 'Output version and exit'
    complete -c timeout -s v -l verbose -d 'Send diagnostic info to stderr'
end
