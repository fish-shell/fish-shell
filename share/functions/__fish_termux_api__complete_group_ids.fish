function __fish_termux_api__complete_group_ids
    set -l command termux-notification-list
    set ids ($command | jq --raw-output '.[].group')
    set titles ($command | jq --raw-output '.[].title')

    for index in (seq (count $ids))
        set -l id $ids[$index]
        set -l title $titles[$index]

        test -z "$id" && continue
        test -z "$title" && set title "Unknown title"

        printf "%s\t%s\n" $id $title
    end
end
