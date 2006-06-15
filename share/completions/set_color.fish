complete -c set_color -x -d (N_ "Color") -a '(set_color --print-colors)'
complete -c set_color -s b -l background -x -a '(set_color --print-colors)' -d (N_ "Change background color")
complete -c set_color -s o -l bold -d (N_ 'Make font bold')
complete -c set_color -s u -l underline -d (N_ 'Underline text')
complete -c set_color -s v -l version -d (N_ 'Display version and exit')
complete -c set_color -s h -l help -d (N_ 'Display help and exit')
complete -c set_color -s c -l print-colors -d (N_ 'Print a list of all accepted color names')

