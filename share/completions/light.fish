# Light is a program to control backlight controllers under GNU/Linux.
# See: https://github.com/haikarainen/light

complete -c light -s h -s -H -d 'Print help and exit'
complete -c light -s V -d 'Print version info and exit'
complete -c light -s L -d 'List controllers'
complete -c light -s A -x -d 'Add value'
complete -c light -s U -x -d 'Subtract value'
complete -c light -s T -d 'Save brightness'
complete -c light -s S -x -d 'Set value'
complete -c light -s G -d 'Get value (default)'
complete -c light -s N -d 'Set minimum brightness'
complete -c light -s P -d 'Get minimum brightness'
complete -c light -s O -d 'Save brightness'
complete -c light -s I -d 'Restore brightness after saving'

complete -c light -s r -d 'Interpret & output values in raw mode'
complete -c light -s s -a '(command light -L)' -x -d 'Specify controller to use'
complete -c light -s v -x -d 'Sets the verbosity level' -a '0\tRead\ values 1\tRead\ values\ \&\ errors 2\tRead\ values,\ errors\ \&\ warnings 3\tRead\ values,\ errors,\ warnings\ \&\ notices'
