function __fish_termux_api__complete_camera_ids
    set -l command termux-camera-info
    set ids ($command | jq --raw-output '.[].id')
    set types ($command | jq --raw-output '.[].facing')

    printf "0\tdefault\n"

    for index in (seq (count $ids))
        printf "%s\t%s\n" $ids[$index] $types[$index]
    end
end

set command termux-camera-photo

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '(__fish_termux_api__complete_camera_ids)' \
    -s c \
    -d 'Specify a [c]amera ID' \
    -x
