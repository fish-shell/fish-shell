#
# Command specific completions for the acpi command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c acpi -s b -l battery --description 'Show battery information'
complete -c acpi -s B -l without-battery --description 'Suppress battery information'
complete -c acpi -s t -l thermal --description 'Show thermal information'
complete -c acpi -s T -l without-thermal --description 'Suppress thermal information'
complete -c acpi -s a -l ac-adapter --description 'Show ac adapter information'
complete -c acpi -s A -l without-ac-adapter --description 'Suppress ac-adapter information'
complete -c acpi -s V -l everything --description 'Show every device, overrides above options'
complete -c acpi -s s -l show-empty --description 'Show non-operational devices'
complete -c acpi -s S -l hide-empty --description 'Hide non-operational devices'
complete -c acpi -s c -l celcius --description 'Use celcius as the temperature unit'
complete -c acpi -s f -l fahrenheit --description 'Use fahrenheit as the temperature unit'
complete -c acpi -s k -l kelvin --description 'Use kelvin as the temperature unit'
complete -c acpi -s d -l directory --description '<dir> path to ACPI info (/proc/acpi)'
complete -c acpi -s h -l help --description 'Display help and exit'
complete -c acpi -s v -l version --description 'Output version information and exit'
