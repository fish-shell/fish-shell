set command termux-camera-photo

complete -c $command -f

complete -c $command \
    -s h \
    -d 'Show [h]elp'

complete -c $command \
    -a '(__fish_termux_api__complete_camera_ids)' \
    -s c \
    -d 'Specify a [c]amera ID' \
    -x
