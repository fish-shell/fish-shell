set -l command termux-job-scheduler

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s p -l pending -d 'List pending jobs'
complete -c $command -l cancel-all -d 'Cancel all pending jobs'
complete -c $command -s s -l script -r -d 'Specify a script path to schedule'
complete -c $command -l job-id -x -d 'Specify the ID of a job'

complete -c $command -l period-ms -x \
    -a '0\tdefault' \
    -d 'Specify the interval between script runs'

complete -c $command -l network -x \
    -a 'any\tdefault unmetered cellular not_roaming none' \
    -d 'Require the network of a specific type to be available'

complete -c $command -l battery-not-low -x \
    -a 'true\tdefault false' \
    -d 'Whether run a script when a battery is low'

complete -c $command -l storage-not-low -x \
    -a 'false\tdefault true' \
    -d 'Whether run a script when a storage is low'

complete -c $command -l charging -x \
    -a 'false\tdefault true' \
    -d 'Whether run a script when the device is charging'

complete -c $command -l persisted -x \
    -a 'false\tdefault true' \
    -d 'Whether unschedule script on reboots'

complete -c $command -l trigger-content-uri -x
complete -c $command -l trigger-content-flag -x -a '1\tdefault'
