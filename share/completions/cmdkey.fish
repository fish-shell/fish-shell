function __cmdkey_complete_args -d 'Function to generate args'
    set -l current_token (commandline -tc)
    set -l previous_token (commandline -xc)[-1]

    switch $current_token
        case '/user:*'
            __fish_print_windows_users | awk "{ printf \"%s%s\n\", \"$current_token\", \$0 }"
        case '*'
            if test "$previous_token" = /delete
                echo -e '/ras\tDelete remote access entry'
                return
            end
    end
end

complete -c cmdkey -f -a '(__cmdkey_complete_args)'

complete -c cmdkey -f -n "not __fish_seen_argument -w add: -w generic:" -a /add: \
    -d 'Add a user name and password'
complete -c cmdkey -f -n "not __fish_seen_argument -w add: -w generic:" -a /generic: \
    -d 'Add generic credentials'

complete -c cmdkey -f -n "not __fish_seen_argument -w smartcard -w user:" -a /smartcard \
    -d 'Retrieve the credential'
complete -c cmdkey -f -n "not __fish_seen_argument -w smartcard -w user:" -a /user: \
    -d 'Specify the user or account name'

complete -c cmdkey -f -n "__fish_seen_argument -w user:" -a /pass: -d 'Specify the password'

complete -c cmdkey -f -a /delete: -d 'Specify the user or account name'
complete -c cmdkey -f -a /delete -d 'Specify the user or account name'

complete -c cmdkey -f -a /list: -d 'Display the list of stored user names and credentials'
complete -c cmdkey -f -a '/?' -d 'Show help'
