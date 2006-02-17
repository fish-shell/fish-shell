complete -c set_color -x -d (_ "Color") -a '(set_color --print-colors)'
complete -c set_color -s b -l background -x -a '(set_color --print-colors)' -d (_ "Change background color")
complete -c set_color -s o -l bold -d (_ 'Make font bold')
