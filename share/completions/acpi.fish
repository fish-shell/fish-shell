#
# Command specific completions for the acpi command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c acpi -s b -l battery -d 'Show battery information'
complete -c acpi -s B -l without-battery -d 'Suppress battery information'
complete -c acpi -s t -l thermal -d 'Show thermal information'
complete -c acpi -s T -l without-thermal -d 'Suppress thermal information'
complete -c acpi -s a -l ac-adapter -d 'Show ac adapter information'
complete -c acpi -s A -l without-ac-adapter -d 'Suppress ac-adapter information'
complete -c acpi -s V -l everything -d 'Show every device, overrides above options'
complete -c acpi -s s -l show-empty -d 'Show non-operational devices'
complete -c acpi -s S -l hide-empty -d 'Hide non-operational devices'
complete -c acpi -s c -l cooling -d 'Show cooling device information'
complete -c acpi -s f -l fahrenheit -d 'Use fahrenheit as the temperature unit'
complete -c acpi -s k -l kelvin -d 'Use kelvin as the temperature unit'
complete -c acpi -s d -l directory -d '<dir> path to ACPI info (/proc/acpi)'
complete -c acpi -s h -l help -d 'Display help and exit'
complete -c acpi -s v -l version -d 'Output version information and exit'
