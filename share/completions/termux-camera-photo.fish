set -l command termux-camera-photo

complete -c $command -f

complete -c $command -s h -d 'Show help'

complete -c $command -s c -x \
    -a '(__fish_termux_api__complete_camera_ids)' \
    -d 'Specify the ID of a camera'
