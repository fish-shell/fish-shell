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

set command termux-notification

complete -c $command -f

complete -c $command \
    -s h \
    -l help \
    -d 'Show [h]elp'

complete -c $command \
    -l action \
    -d 'Specify the action when pressing a notification' \
    -F -r

complete -c $command \
    -l alert-once \
    -d 'Do not alert when a notification is edited'

set button_names first second third
set index 1

while test $index -le (count $button_names)
    complete -c $command \
        -l button$index \
        -d "Specify the text of the $button_names[$index] notification button" \
        -x

    complete -c $command \
        -l button$index-action \
        -d "Specify the action of the $button_names[$index] notification button" \
        -F -r

    set index (math $index + 1)
end

complete -c $command \
    -s c \
    -l content \
    -d 'Specify the [c]ontent of a notification' \
    -x

complete -c $command \
    -a '(__fish_termux_api__complete_group_ids)' \
    -l group \
    -d 'Specify the group of a notification' \
    -x

complete -c $command \
    -l help-actions \
    -d 'Show [h]elp for the actions of a notification'

complete -c $command \
    -s i \
    -l id \
    -d 'Specify the [i]dentifier of a notification' \
    -x

complete -c $command \
    -l image-path \
    -d 'Specify the image of a notification' \
    -F -r

complete -c $command \
    -a 'none\tdefault' \
    -l led-color \
    -d 'Specify the LED color of a notification' \
    -x

set led_states on off

for state in $led_states
    complete -c $command \
        -a '800\tdefault' \
        -l led-$state \
        -d 'Specify the time for the LED to be $state while flashing of a notification' \
        -x
end

complete -c $command \
    -l on-delete \
    -d 'Specify the action when a notification is cleared' \
    -F -r

complete -c $command \
    -l ongoing \
    -d 'Pin a notification'

complete -c $command \
    -a 'default\tdefault high low max min' \
    -l priority \
    -d 'Specify the priority of a notification' \
    -x

complete -c $command \
    -l sound \
    -d 'Play the sound with a notification'

complete -c $command \
    -s t \
    -l title \
    -d 'Specify the [t]itle of a notification' \
    -x

complete -c $command \
    -l vibrate \
    -d 'Specify the vibrate pattern of a notification' \
    -x

complete -c $command \
    -a 'default\tdefault media' \
    -l type \
    -d 'Specify the style of a notification' \
    -x

set media_options next pause play previous

for option in $media_options
    complete -c $command \
        -l media-$option \
        -d "Specify the action for $option button a notification" \
        -n '__fish_seen_argument -l type' \
        -F -r
end
