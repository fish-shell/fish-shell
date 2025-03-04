set -l command termux-sensor

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'

complete -c $command -s a -l all -d 'Listen to all sensors'
complete -c $command -s c -l cleanup -d 'Release sensor resources'
complete -c $command -s l -l list -d 'List sensors'

complete -c $command -s s -l sensors -x \
    -a '(__fish_complete_list , __fish_termux_api__complete_sensor_ids)' \
    -d 'Specify comma-separated sensors to listen to'

complete -c $command -s d -l delay -x \
    -d 'Specify the delay between sensor updates'

complete -c $command -s n -l limit -x \
    -a 'continuous\tdefault' \
    -d 'Specify a number of times to read senors'
