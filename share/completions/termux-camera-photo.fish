set command termux-camera-photo

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show help'

complete -c $command \
    -a '(__fish_termux_api__complete_camera_ids)' \
    -s c \
    -d 'Specify the ID of a camera' \
    -x
