function __setx_complete_args -d 'Function to generate args'
    set -l previous_token (commandline -xc)[-1]

    if test "$previous_token" = /u
        __fish_print_windows_users
        return
    end
end

complete -c setx -f -a '(__setx_complete_args)'

complete -c setx -f -n '__fish_seen_argument -w s' -a /u \
    -d 'Run the script with the credentials of the specified user account'
complete -c setx -f -n '__fish_seen_argument -w u' -a /p \
    -d 'Specify the password of the user account that is specified in the /u parameter'

complete -c setx -f -n 'not __fish_seen_argument -w a -w r -w x' -a /a \
    -d 'Specify absolute coordinates and offset as search parameters'
complete -c setx -f -n 'not __fish_seen_argument -w a -w r -w x' -a /r \
    -d 'Specify relative coordinates and offset'
complete -c setx -f -n 'not __fish_seen_argument -w a -w r -w x' -a /x \
    -d 'Display file coordinates, ignoring the /a, /r, and /d command-line options'

complete -c setx -f -n '__fish_seen_argument -w a -w r' -a /m \
    -d 'Specify to set the variable in the system environment'

complete -c setx -f -a /s -d 'Specify the name or IP address of a remote computer'
complete -c setx -f -a /k \
    -d 'Specify that the variable is set based on information from a registry key'
complete -c setx -f -a /f -d 'Specify the file that you want to use'
complete -c setx -f -a /d -d 'Specify delimiters to be used'
complete -c setx -f -a '/?' -d 'Show help'
