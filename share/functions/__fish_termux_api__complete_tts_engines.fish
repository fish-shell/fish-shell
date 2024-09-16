function __fish_termux_api__complete_tts_engines
    set -l command termux-tts-engines
    set ids ($command | jq --raw-output '.[].name')
    set labels ($command | jq --raw-output '.[].label')

    for index in (seq (count $ids))
        printf "%s\t%s\n" $ids[$index] $labels[$index]
    end
end
