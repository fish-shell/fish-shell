set command termux-job-scheduler

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -s p \
    -l pending \
    -d 'List [p]ending jobs'

complete -c $command \
    -l cancel-all \
    -d 'Cancel all pending jobs'

complete -c $command \
    -s s \
    -l script \
    -d 'Specify a [s]cript path to schedule' \
    -r

complete -c $command \
    -l job-id \
    -d 'Specify the ID of a job' \
    -x

complete -c $command \
    -a '0\tdefault' \
    -l period-ms \
    -d 'Specify the interval between script runs' \
    -x

complete -c $command \
    -a 'any\tdefault unmetered cellular not_roaming none' \
    -l network \
    -d 'Require the network of a specific type to be available' \
    -x

complete -c $command \
    -a 'true\tdefault false' \
    -l battery-not-low \
    -d 'Whether run a script when a battery is low' \
    -x

complete -c $command \
    -a 'false\tdefault true' \
    -l storage-not-low \
    -d 'Whether run a script when a storage is low' \
    -x

complete -c $command \
    -a 'false\tdefault true' \
    -l charging \
    -d 'Whether run a script when the device is charging' \
    -x

complete -c $command \
    -a 'false\tdefault true' \
    -l persisted \
    -d 'Whether unschedule script on reboots' \
    -x

complete -c $command \
    -l trigger-content-uri \
    -x

complete -c $command \
    -a '1\tdefault' \
    -l trigger-content-flag \
    -x
