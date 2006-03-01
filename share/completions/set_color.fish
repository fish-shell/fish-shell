complete -c set_color -x -d (N_ "Color") -a '(set_color --print-colors)'
complete -c set_color -s b -l background -x -a '(set_color --print-colors)' -d (N_ "Change background color")
complete -c set_color -s o -l bold -d (N_ 'Make font bold')
