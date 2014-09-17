complete -c set_color -x --description "Color" -a '(set_color --print-colors)'
complete -c set_color -s b -l background -x -a '(set_color --print-colors)' --description "Change background color"
complete -c set_color -s o -l bold --description 'Make font bold'
complete -c set_color -s u -l underline --description 'Underline text'
complete -c set_color -s h -l help --description 'Display help and exit'
complete -c set_color -s c -l print-colors --description 'Print a list of all accepted color names'
