complete -f -c yast2 -a '(yast2 -l)' -d Module

complete -f -c yast2 -s h -l help -d 'Show help'
complete -f -c yast2 -s l -l list -d 'List all available modules'
complete -f -c yast2 -l qt -d 'Use the QT graphical user interface'
complete -f -c yast2 -l gtk -d 'Use the GTK graphical user interface'
complete -f -c yast2 -l ncurses -d 'Use the NCURSES text-mode user interface'
complete -f -c yast2 -s g -l geometry -d 'Default window size (qt only)'
complete -f -c yast2 -l noborder -d 'No window manager border for main window'
complete -f -c yast2 -l fullscreen -d 'Use full screen'
