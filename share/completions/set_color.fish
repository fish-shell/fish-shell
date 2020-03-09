complete -c set_color -x -d Color -a '(set_color --print-colors)'
complete -c set_color -s b -l background -x -a '(set_color --print-colors)' -d "Change background color"
complete -c set_color -s o -l bold -d 'Make font bold'
complete -c set_color -s i -l italics -d Italicise
complete -c set_color -s d -l dim -d 'Dim text'
complete -c set_color -s r -l reverse -d 'Reverse color text'
complete -c set_color -s u -l underline -d 'Underline text'
complete -c set_color -s h -l help -d 'Display help and exit'
complete -c set_color -s c -l print-colors -d 'Print a list of all accepted color names'
