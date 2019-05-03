# xinput, a tool for managing X11 input devices
set -l cmds version list get-feedbacks set-pointer set-mode set-ptr-feedback set-integer-feedback set-button-map query-state list-props set-int-prop set-float-prop set-prop watch-props delete-prop test test-xi2 create-master remove-master reattach float set-cp map-to-output enable disable

function __fish_xinput_devices
    # Yeah, this includes a "↳" char
    # There's either this or printing only name or id, not both
    xinput list --short | string replace -r '^[\W↳]*([\w/ ]+)\s+id=([0-9]+).*$' '$2\t$1'
end

complete -c xinput -f -n "not __fish_seen_subcommand_from $cmds" -a "$cmds"
complete -c xinput -f -n "__fish_seen_subcommand_from list get-feedbacks set-pointer set-mode set-ptr-feedback set-integer-feedback set-button-map query-state list-props set-int-prop set-float-prop set-prop watch-props" -a "(__fish_xinput_devices)"
