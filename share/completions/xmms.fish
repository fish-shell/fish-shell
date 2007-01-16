#
# Completions for the xmms command
# Vikas Gorur <vikas@80x25.org>
#

complete -c xmms -s h -l help --description "Show summary of options"
complete -c xmms -s n -l session --description "Select XMMS session (Default: 0)"
complete -c xmms -s r -l rew --description "Skip backwards in playlist"
complete -c xmms -s p -l play --description "Start playing current playlist"
complete -c xmms -s u -l pause --description "Pause current song"
complete -c xmms -s s -l stop --description "Stop current song"
complete -c xmms -s t -l play-pause --description "Pause if playing, play otherwise"
complete -c xmms -s f -l fwd --description "Skip forward in playlist"
complete -c xmms -s e -l enqueue --description "Don't clear the playlist"
complete -c xmms -s m -l show-main-window --description "Show the main window"
complete -c xmms -s v -l version --description "Print the version number and exit"