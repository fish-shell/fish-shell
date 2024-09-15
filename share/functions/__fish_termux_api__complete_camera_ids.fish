function __fish_termux_api__complete_camera_ids
    set -l command termux-camera-info
    set ids ($command | jq --raw-output '.[].id')
    set types ($command | jq --raw-output '.[].facing')

    printf "0\tdefault\n"

    for index in (seq (count $ids))
        printf "%s\t%s\n" $ids[$index] $types[$index]
    end
end
