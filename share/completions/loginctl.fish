#loginctl (systemd 254)

#variables
set -l seen __fish_seen_subcommand_from
set -l commands activate attach disable-linger enable-linger flush-devices kill-session kill-user list-seats list-sessions list-users lock-session lock-sessions seat-status session-status show-seat show-session show-user terminate-seat terminate-session terminate-user unlock-session unlock-sessions user-status
set -l output cat export json json-pretty json-seq json-sse short short-full short-iso short-iso-precise short-monotonic short-precise short-unix verbose with-unit

complete -c loginctl -f

#commands
complete -c loginctl -x -n "not $seen $commands" -a "$commands"

#options
complete -c loginctl -f -n "not $seen $commands" -l all -s a -d "Show all properties, including empty ones"
complete -c loginctl -f -n "not $seen $commands" -l full -s l -d "Do not ellipsize output"
complete -c loginctl -f -n "not $seen $commands" -l help -s h -d "Show this help"
complete -c loginctl -x -n "not $seen $commands" -l host -s H -d "Operate on remote host"
complete -c loginctl -x -n "not $seen $commands" -l kill-who -d "Who to send signal to"
complete -c loginctl -x -n "not $seen $commands" -l lines -s n -d "Number of journal entries to show"
complete -c loginctl -x -n "not $seen $commands" -l machine -s M -d "Operate on local container"
complete -c loginctl -f -n "not $seen $commands" -l no-ask-password -d "Don't prompt for password"
complete -c loginctl -f -n "not $seen $commands" -l no-legend -d "Do not show the headers and footers"
complete -c loginctl -f -n "not $seen $commands" -l no-pager -d "Do not pipe output into a pager"
complete -c loginctl -x -n "not $seen $commands" -l output -s o -a "$output" -d "Change journal output mode"
complete -c loginctl -x -n "not $seen $commands" -l property -s p -d "Show only properties by this name"
complete -c loginctl -x -n "not $seen $commands" -s P -d "Equivalent to --value --property"
complete -c loginctl -x -n "not $seen $commands" -l signal -s s -d "Which signal to send"
complete -c loginctl -f -n "not $seen $commands" -l value -d "When showing properties, only print the value"
complete -c loginctl -f -n "not $seen $commands" -l version -d "Show package version"

function __fish_loginctl_list_sessions
    loginctl list-sessions --no-legend --no-pager --output=short | string replace -r '^\s*(\d+)\s+\d+\s+(\S+)\s+(\S+\s+)?(\S+\d+).*' '$1\t$2 at $4'
end

function __fish_loginctl_list_users
    loginctl list-users --no-legend --no-pager --output=short | string replace -r '(\d+) (\S+) .*' '$1\t$2'
end

function __fish_loginctl_list_seats
    loginctl list-seats --no-legend --no-pager --output=short
end

complete -c loginctl -n "$seen session-status show-session activate lock-session unlock-session terminate-session kill-session" -a '(__fish_loginctl_list_sessions)'
complete -c loginctl -n "$seen user-status show-user enable-linger disable-linger terminate-user kill-user" -a '(__fish_loginctl_list_users)'
complete -c loginctl -n "$seen seat-status show-seat attach terminate-seat" -a '(__fish_loginctl_list_seats)'
