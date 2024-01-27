# xinput, a tool for managing X11 input devices
set -l cmds version list get-feedbacks set-pointer set-mode set-ptr-feedback set-integer-feedback set-button-map query-state list-props set-int-prop set-float-prop set-prop watch-props delete-prop test test-xi2 create-master remove-master reattach float set-cp map-to-output enable disable

function __fish_xinput_devices
    # Yeah, this includes a "↳" char
    # There's either this or printing only name or id, not both
    xinput list --short | string replace -r '^[\W↳]*(.*?)\s+id=([0-9]+).*$' '$2\t$1'
    xinput list --short | string replace -r '^[\W↳]*(.*?)\s+id=([0-9]+).*$' '$1'
end

function __fish_xinput_device_props
    set -l device $argv[1]
    if test -n "$device"
        xinput list-props $device | string replace -rf '\s+(.*?)\s+\((\d+)\).*' '$2\t$1'
        xinput list-props $device | string replace -rf '\s+(.*?)\s+\((\d+)\).*' '$1'
    end
end

function __fish_xinput_nth_token
    set -l n $argv[1]
    set -l tokens (commandline -cx)
    set -e tokens[1] # remove command name
    # remove options
    set -l tokens (string replace -r --filter '^([^-].*)' '$1' -- $tokens)
    if set -q tokens[$n]
        echo $tokens[$n]
    end
end

complete -c xinput -f -n "not __fish_seen_subcommand_from $cmds" -a "$cmds"
complete -c xinput -f -n "__fish_is_nth_token 2; and __fish_seen_subcommand_from list get-feedbacks set-pointer set-mode set-ptr-feedback set-integer-feedback set-button-map query-state list-props set-int-prop set-float-prop set-prop watch-props" -a "(__fish_xinput_devices)"
complete -c xinput -f -n "__fish_seen_subcommand_from list set-int-prop set-float-prop set-prop watch-props; and not __fish_is_nth_token 2" -xa "(__fish_xinput_device_props (__fish_xinput_nth_token 2))"
