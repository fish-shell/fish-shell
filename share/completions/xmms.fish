#
# Completions for the xmms command
# Vikas Gorur <vikas@80x25.org>
#

complete -c xmms -s h -l help -d (N_ "Show summary of options")
complete -c xmms -s n -l session -d (N_ "Select XMMS session (Default: 0)")
complete -c xmms -s r -l rew -d (N_ "Skip backwards in playlist")
complete -c xmms -s p -l play -d (N_ "Start playing current playlist")
complete -c xmms -s u -l pause -d (N_ "Pause current song")
complete -c xmms -s s -l stop -d (N_ "Stop current song")
complete -c xmms -s t -l play-pause -d (N_ "Pause if playing, play otherwise")
complete -c xmms -s f -l fwd -d (N_ "Skip forward in playlist")
complete -c xmms -s e -l enqueue -d (N_ "Don't clear the playlist")
complete -c xmms -s m -l show-main-window -d (N_ "Show the main window")
complete -c xmms -s v -l version -d (N_ "Print the version number and exit")