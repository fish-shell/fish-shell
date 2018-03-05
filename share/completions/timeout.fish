
# magic completion safety check (do not remove this comment)
if not type -q timeout
    exit
end

complete -c timeout -l foreground -d 'Run COMMAND in the foreground'
complete -c timeout -s k -l kill-after -d 'Send a KILL signal after DURATION'
complete -c timeout -s s -l signal -d 'Specify the signal to be sent'
complete -c timeout -l help -d 'Display this help and exit'
complete -c timeout -l version -d 'Output version information and exit'

