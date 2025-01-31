set -l command termux-notification

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'

complete -c $command -l action -F -r \
    -d 'Specify the action when pressing a notification'

complete -c $command -l alert-once \
    -d 'Do not alert when a notification is edited'

set -l button_names first second third
set -l index 1

while test $index -le (count $button_names)
    complete -c $command -l button$index -x \
        -d "Specify the text of a $button_names[$index] notification button"

    complete -c $command -l button$index-action -F -r \
        -d "Specify the action of a $button_names[$index] notification button"

    set index (math $index + 1)
end

complete -c $command -s c -l content -x \
    -d 'Specify the content of a notification'

complete -c $command -l group -x \
    -a '(__fish_termux_api__complete_group_ids)' \
    -d 'Specify the group of a notification'

complete -c $command -l help-actions \
    -d 'Show help for the actions of a notification'

complete -c $command -s i -l id -x \
    -d 'Specify the identifier of a notification'

complete -c $command -l image-path -F -r \
    -d 'Specify the image of a notification'

complete -c $command -l led-color -x \
    -a 'none\tdefault' \
    -d 'Specify the LED color of a notification'

set -l led_states on off

for state in $led_states
    complete -c $command -l led-$state -x \
        -a '800\tdefault' \
        -d "Specify the time for the LED to be $state while flashing of a notification"
end

complete -c $command -l on-delete -F -r \
    -d 'Specify the action when a notification is cleared'

complete -c $command -l ongoing -d 'Pin a notification'

complete -c $command -l priority -x \
    -a 'default\tdefault high low max min' \
    -d 'Specify the priority of a notification'

complete -c $command -l sound -d 'Play the sound with a notification'
complete -c $command -s t -l title -x -d 'Specify the title of a notification'

complete -c $command -l vibrate -x \
    -d 'Specify the vibrate pattern of a notification'

complete -c $command -l type -x \
    -a 'default\tdefault media' \
    -d 'Specify the style of a notification'

set -l media_options next pause play previous

for option in $media_options
    complete -c $command -l media-$option -F -r \
        -d "Specify the action for $option button a notification" \
        -n '__fish_seen_argument -l type'
end
