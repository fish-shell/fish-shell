complete -c cmd -f -a '(__fish_cmd__complete_args)'

complete -c cmd -f -n 'not __fish_seen_argument -w c -w k' -a /c \
    -d 'Carry out the command specified by string and then stop'
complete -c cmd -f -n 'not __fish_seen_argument -w c -w k' -a /k \
    -d 'Carry out the command specified by string and continue'

complete -c cmd -f -a /s -d 'Modify the treatment of string after /c or /k'
complete -c cmd -f -a /q -d 'Turn the echo off'
complete -c cmd -f -a /d -d 'Disable execution of AutoRun commands'

complete -c cmd -f -n 'not __fish_seen_argument -w a -w u' -a /a \
    -d 'Format internal command output as ANSI'
complete -c cmd -f -n 'not __fish_seen_argument -w a -w u' -a /u \
    -d 'Format internal command output as Unicode'

complete -c cmd -f -a /t -d 'Set the background and foreground color'
complete -c cmd -f -a /e -d 'Manage command extensions'
complete -c cmd -f -a /f -d 'Manage file and directory name completion'
complete -c cmd -f -a /v -d 'Manage delayed environment variable expansion'
complete -c cmd -f -a '/?' -d 'Show help'
