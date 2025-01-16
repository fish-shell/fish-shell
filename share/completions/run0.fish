function __run0_slice
    systemctl -t slice --no-legend --no-pager --plain | string split -nf 1 ' '
    systemctl -t slice --user --no-legend --no-pager --plain | string split -nf 1 ' '
end

complete -c run0 -xa "(__fish_complete_subcommand)"
complete -c run0 -s h -l help -d "Show help"
complete -c run0 -s V -l version -d "Show version"
complete -c run0 -s u -l user -d "Switches to the specified user instead of root" -xa "(__fish_complete_users)"
complete -c run0 -s g -l group -d "Switches to the specified group instead of root" -xa "(__fish_complete_groups)"
complete -c run0 -l no-ask-password -d "Do not query the user for authentication for privileged operations"
complete -c run0 -l machine -d "Execute operation on a local container" -xa "(machinectl list --no-legend --no-pager | string split -f 1 ' ')"
complete -c run0 -l property -d "Sets a property on the service unit that is created" -x
complete -c run0 -l description -d "Description for unit" -x
complete -c run0 -l slice -d "Make the new .service unit part of the specified slice, instead of user.slice." -xa "(__run0_slice)"
complete -c run0 -l slice-inherit -d "Make the new .service unit part of the slice the run0 itself has been invoked in"
complete -c run0 -l nice -d "Runs the invoked session with the specified nice level" -xa "(seq -20 19)"
complete -c run0 -s D -l chdir -d "Set working directory" -xa "(__fish_complete_directories)"
complete -c run0 -l setenv -d "Runs the invoked session with the specified environment variable set" -x
complete -c run0 -l background -d "Change the terminal background color to the specified ANSI color" -x
complete -c run0 -l pty -d "Request allocation of a pseudo TTY for stdio"
complete -c run0 -l pipe -d "Request redirect pipe for stdio"
complete -c run0 -l shell-prompt-prefix -d "Set a shell prompt prefix string" -x
