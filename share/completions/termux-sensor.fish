function __fish_termux_api__complete_sensor_ids_as_list
    set ids (__fish_termux_api__complete_sensor_ids)
    set token (commandline -t -c)

    set delimiter ,

    switch "$token"
        case '*,'
            set delimiter
    end

    test -z "$token" && set delimiter

    for id in $ids
        string unescape -- "$token$delimiter$id"
    end
end

set command termux-sensor

complete -c $command -f

complete -c $command \
    -s h \
    -l help \
    -d 'Show [h]elp'

complete -c $command \
    -s a \
    -l all \
    -d 'Listen to [a]ll sensors'

complete -c $command \
    -s c \
    -l cleanup \
    -d 'Release sensor resources'

complete -c $command \
    -s l \
    -l list \
    -d '[l]ist sensors'

complete -c $command \
    -a '(__fish_termux_api__complete_sensor_ids_as_list)' \
    -s s \
    -l sensors \
    -d 'Specify comma-separated [s]ensors to listen to' \
    -x

complete -c $command \
    -s d \
    -l delay \
    -d 'Specify the [d]elay between sensor updates' \
    -x

complete -c $command \
    -a 'continuous\tdefault' \
    -s n \
    -l limit \
    -d 'Specify a [n]umber of times to read senors' \
    -x
