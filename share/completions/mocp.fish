complete -c mocp -s V -l version -d "Print program version"
complete -c mocp -s h -l help -d "Print usage"
complete -c mocp -s D -l debug -d "Turn on logging to a file"
complete -c mocp -s S -l server -d "Run only the server"
complete -c mocp -s F -l foreground -d "Run server in fg, log to stdout"
complete -c mocp -s R -l sound-driver -d "Use the specified sound driver" -xa 'oss alsa jack null'
complete -c mocp -s m -l music-dir -r -d "Start in MusicDir"
complete -c mocp -s a -l append -r -d "Append to the playlist"
complete -c mocp -s q -l enqueue -r -d "Add files to the queue"
complete -c mocp -s c -l clear -d "Clear the playlist"
complete -c mocp -s p -l play -r -d "Start playing from the first item on the playlist"
complete -c mocp -s l -l playit -r -d "Play files without modifying the playlist"
complete -c mocp -s s -l stop -d "Stop playing"
complete -c mocp -s f -l next -d "Play next song"
complete -c mocp -s r -l previous -d "Play previous song"
complete -c mocp -s x -l exit -d "Shutdown the server"
complete -c mocp -s T -l theme -r -d "Use selected theme file"
complete -c mocp -s C -l config -r -d "Use config file instead of the default"
complete -c mocp -s O -l set-option -r -d "NAME=VALUE Override configuration option NAME"
complete -c mocp -s M -l moc-dir -r -d "Use MOC directory instead of the default"
complete -c mocp -s P -l pause -d Pause
complete -c mocp -s U -l unpause -d Unpause
complete -c mocp -s G -l toggle-pause -d "Toggle between play/pause"
complete -c mocp -s v -l volume -d "(+/-)LEVEL Adjust PCM volume" -xa '+ -'
complete -c mocp -s y -l sync -d "Synchronize the playlist with other clients"
complete -c mocp -s n -l nosync -d "Don't synchronize the playlist with other clients"
complete -c mocp -s A -l ascii -d "Use ASCII characters to draw lines"
complete -c mocp -s i -l info -d "Information of the currently played file"
complete -c mocp -s Q -l format -rf -d "Formatted information about currently played file"
complete -c mocp -s e -l recursively -d "Alias for -a"
complete -c mocp -s k -l seek -rf -d "Seek by N seconds (can be negative)"
complete -c mocp -s j -l jump -rf -d "N{%,s} Jump to some position of the current track"
complete -c mocp -s o -l on -d "Turn on a control" -xa 'shuffle autonext repeat'
complete -c mocp -s u -l off -d "Turn off a control" -xa 'shuffle autonext repeat'
complete -c mocp -s t -l toggle -d "Toggle a control" -xa '(__fish_append , shuffle autonext repeat s\tshuffle r\trepeat n\tautonext)'
