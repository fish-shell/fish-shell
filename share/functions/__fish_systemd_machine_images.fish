# localization: skip(private)
#
# Like for running machines, I'm assuming machinectl doesn't allow spaces in image names
# This does not include the special image ".host" since it isn't valid for most operations
function __fish_systemd_machine_images
    # We don't want to complete with ANSI color codes
    set -lu SYSTEMD_COLORS

    machinectl --no-legend --no-pager list-images | while read -l a b
        echo $a
    end
end
