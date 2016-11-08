# completion for caffeinate (macOS)

complete -c caffeinate -s d -f -d 'Create an assertion to prevent the display from sleeping'
complete -c caffeinate -s i -f -d 'Create an assertion to prevent the system from idle sleeping'
complete -c caffeinate -s m -f -d 'Create an assertion to prevent the disk from idle sleeping'
complete -c caffeinate -s s -f -d 'Create an assertion to prevent the system from sleeping (AC power)'
complete -c caffeinate -s u -f -d 'Create an assertion to declare that user is active'
complete -c caffeinate -s t -x -a '10 60 300 600 1800 3600' -d 'Specifies the timeout value in seconds'
complete -c caffeinate -s w -x -a '(__fish_complete_pids)' -d 'Waits for the process with the specified PID to exit'

complete -c caffeinate -x -a '(__fish_complete_subcommand)'
