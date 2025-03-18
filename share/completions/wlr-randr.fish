# `wlr-randr` completions.
# See: https://gitlab.freedesktop.org/emersion/wlr-randr

function __fish_print_wlr-randr_outputs --argument-names exclude
    if command -q jq
        wlr-randr --json | jq \
            --raw-output \
            --arg exclude "$exclude" \
            '.[] | select(.name != $exclude) | "\(.name)\t\(.make), \(.model)"'
    end
end

function __fish_get_wlr-randr-current_output
    set -l last_output
    set -l tokens (commandline -xpc)

    while set -q tokens[1]
        if test "$tokens[1]" = --output
            set last_output "$tokens[2]"
            set -e tokens[1]
        else if string match -qr -- '^--output=' "$tokens[1]"
            set last_output (string replace -r -- '--output=(.*)' '$1' "$tokens[1]")
        end

        set -e tokens[1]
    end

    printf "%s" $last_output
end

function __fish_complete_wlr-randr_modes
    if command -q jq
        set -l output (__fish_get_wlr-randr-current_output)

        wlr-randr --json | jq \
            --raw-output \
            --arg output "$output" \
            --arg preferred_str Preferred \
            --arg empty_str '' \
            '.[] | select(.name == $output) | .modes[] | "\(.width)x\(.height)\t\(if .preferred then $preferred_str else $empty_str end)"'
    end
end

complete -c wlr-randr -f
complete -c wlr-randr -s h -l help -d 'Show help'
complete -c wlr-randr -l json -d 'Print as JSON'
complete -c wlr-randr -l dryrun -d 'Dry run'
complete -c wlr-randr -l output -x -d Output -a '(__fish_print_wlr-randr_outputs)'
complete -c wlr-randr -l on -d 'Turn on'
complete -c wlr-randr -l off -d 'Turn off'
complete -c wlr-randr -l mode -x -d Mode -a '(__fish_complete_wlr-randr_modes)'
complete -c wlr-randr -l pos -r -d Position
complete -c wlr-randr -l left-of -x -d 'Relative left position' -a '(__fish_print_wlr-randr_outputs (__fish_get_wlr-randr-current_output))'
complete -c wlr-randr -l right-of -x -d 'Relative right position' -a '(__fish_print_wlr-randr_outputs (__fish_get_wlr-randr-current_output))'
complete -c wlr-randr -l above -x -d 'Relative top position' -a '(__fish_print_wlr-randr_outputs (__fish_get_wlr-randr-current_output))'
complete -c wlr-randr -l below -x -d 'Relative bottom position' -a '(__fish_print_wlr-randr_outputs (__fish_get_wlr-randr-current_output))'
complete -c wlr-randr -l transform -x -d Transformation -a 'normal\t 90\t 180\t 270\t flipped\t flipped-90\t flipped-180\t flipped-270\t'
complete -c wlr-randr -l scale -x -d Scale
