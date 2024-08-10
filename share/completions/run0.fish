#
# Completion for run0
#

complete -c run0 -l no-ask-password -d 'Do not query the user for authentication for privileged operations'
complete -c run0 -l unit -d 'Use this unit name instead of an automatically generated one'
complete -c run0 -l property -d 'Sets a property on the service unit that is created'
complete -c run0 -l description -d 'Provide a description for the service unit that is invoked'
complete -c run0 -l slice -d 'Make the new .service unit part of the specified slice, instead of user.slice.'
complete -c run0 -l slice-inherit -d 'Make the new .service unit part of the slice the run0 itself has been invoked in'
complete -c run0 -s u -l user -a "(__fish_complete_users)" -x -d "Switches to the specified user instead of root"
complete -c run0 -s g -l group -a "(__fish_complete_groups)" -x -d "Switches to the specified group instead of root"
complete -c run0 -l nice -d 'Runs the invoked session with the specified nice level'
complete -c run0 -s D -l chdir -d 'Runs the invoked session with the specified working directory'
complete -c run0 -l setenv -d 'Runs the invoked session with the specified environment variable set'
complete -c run0 -l background -d 'Change the terminal background color to the specified ANSI color'
complete -c run0 -l machine -d 'Execute operation on a local container'
complete -c run0 -s h -l help -d 'Print a short help text and exit'
complete -c run0 -l version -d 'Print a short version string and exit'

complete -c run0 -x -a '(__fish_complete_subcommand)'
