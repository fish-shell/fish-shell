# xclip is a command line interface to X selections (clipboard).
# See: xclip(1)

complete -c xclip -o in -s i -d 'Read text into X selection from stdin'
complete -c xclip -o out -s o -d 'Prints the selection to stdout'
complete -c xclip -o loops -s l -d 'Number of selection requests to wait for before exiting'
complete -c xclip -o display -s d -d 'X display to connect to'
complete -c xclip -o help -s h -d 'Usage information'
complete -c xclip -o selection -x -d 'Selection to access' -a 'primary\tDefault secondary\t clipboard\t buffer-cut\t'
complete -c xclip -o target -x -d 'Use the given target atom'
complete -c xclip -o version -d 'Version information'
complete -c xclip -o silent -d 'Errors only, run in background (default)'
complete -c xclip -o quiet -d 'Run in foreground, show what\'s happening'
complete -c xclip -o verbose -d 'Running commentary'
