#
# Completions for the xmms command
# Vikas Gorur <vikas@80x25.org>
#

complete -c xmms -s h -l help -d "Show summary of options"
complete -c xmms -s n -l session -d "Select XMMS session (Default: 0)"
complete -c xmms -s r -l rew -d "Skip backwards in playlist"
complete -c xmms -s p -l play -d "Start playing current playlist"
complete -c xmms -s u -l pause -d "Pause current song"
complete -c xmms -s s -l stop -d "Stop current song"
complete -c xmms -s t -l play-pause -d "Pause if playing, play otherwise"
complete -c xmms -s f -l fwd -d "Skip forward in playlist"
complete -c xmms -s e -l enqueue -d "Don't clear the playlist"
complete -c xmms -s m -l show-main-window -d "Show the main window"
complete -c xmms -s v -l version -d "Print the version number and exit"
