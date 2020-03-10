complete -c gvim -w vim

complete -c gvim -s g -d 'Run using GUI'
complete -c gvim -s f -l nofork -d 'Don\'t fork when starting GUI'
complete -c gvim -o font -o fn -x -d 'Use <font> for normal text'
complete -c gvim -o geometry -o geom -x -d 'Use <geom> for initial geometry'
complete -c gvim -o reverse -o rv -d 'Use reverse video'
complete -c gvim -o display -l display -x -d 'Run vim on <display>'
complete -c gvim -l role -x -d 'Set role for identifying the window in X11'
complete -c gvim -l socketid -x -d 'Open Vim inside another GTK widget'
complete -c gvim -l echo-wid -d 'Echo the Window ID on stdout'
