function __fish_termux_api__complete_sensor_ids
    set -l command termux-sensor -l
    set ids ($command | jq --raw-output '.sensors[]')

    string join \n -- $ids
end
