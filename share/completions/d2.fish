function __fish_d2__complete_themes
    d2 themes |
        sed -n '3,$p' |
        string match --entire --regex '^-' |
        string replace --regex -- '- ([^:]+): (.+)' '$2\t$1'
end

function __fish_d2__complete_layouts
    set -l layouts (d2 layout |
        sed -n '3,$p' |
        sed -n '/^$/,$d; p' |
        string replace --regex '^(\w+)\s+\(bundled\)' '$1')
    
    set -l identifiers
    set -l descriptions

    for layout in $layouts
        set -a identifiers (string replace --regex ' - .+$' '' -- $layouts)
        set -a descriptions (string replace --regex '^\w+ - ' '' -- $layouts)
    end

    for index in (seq (count $layouts))
        printf '%s\t%s\n' $identifiers[$index] $descriptions[$index]
    end
end

set -l command kroki

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s v -l version -d 'Show version'

set -l subcommands_with_descriptions 'layout\t"Show layout information"' \
    'themes\t"List themes information"' \
    'fmt\t"Format files"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)
set -l root_condition "not __fish_seen_subcommand_from $subcommands"

complete -c $command -a "$subcommands_with_descriptions" -n $root_condition

complete -c $command -s w -l watch -x \
    -a 'false\tdefault true' \
    -d 'Whether to watch for changes of a diagram' \
    -n $root_condition

complete -c $command -s h -l host -x \
    -a 'localhost\tdefault' \
    -d 'Specify the host address for a diagram' \
    -n $root_condition

complete -c $command -s p -l port -x \
    -a '0\tdefault' \
    -d 'Specify the port address for a diagram' \
    -n $root_condition

complete -c $command -s b -l bundle -x \
    -a 'true\tdefault false' \
    -d 'Whether to bundle all assets and layers of a diagram' \
    -n $root_condition

complete -c $command -l force-appendix -x \
    -a 'false\tdefault true' \
    -d 'Whether to add the appendix for tooltips and links for a diagram' \
    -n $root_condition

complete -c $command -s d -l debug -x \
    -a 'false\tdefault true' \
    -d 'Whether to show debug logs for a diagram' \
    -n $root_condition

complete -c $command -s d -l debug -x \
    -a 'false\tdefault true' \
    -d 'Whether to show debug logs for a diagram' \
    -n $root_condition

complete -c $command -l img-cache -x \
    -a 'true\tdefault false' \
    -d 'Whether to cache images of a diagram' \
    -n $root_condition

complete -c $command -s l -l layout -x \
    -a '(__fish_d2__complete_layouts)' \
    -d 'Specify the layout for a diagram' \
    -n $root_condition

complete -c $command -s t -l theme -x \
    -a '(__fish_d2__complete_themes)' \
    -d 'Specify the theme for a diagram' \
    -n $root_condition

complete -c $command -l dark-theme -x \
    -a '(__fish_d2__complete_themes)' \
    -d 'Specify the theme for a diagram in dark mode' \
    -n $root_condition

complete -c $command -l pad -x \
    -a '100\tdefault' \
    -d 'Padding of a diagram' \
    -n $root_condition

complete -c $command -l animate-interval -x \
    -a '0\tdefault' \
    -d 'Specify the animation interval for a diagram' \
    -n $root_condition

complete -c $command -l timeout -x \
    -a '120\tdefault' \
    -d 'Specify the timeout for a render' \
    -n $root_condition

complete -c $command -s s -l sketch -x \
    -a 'false\tdefault true' \
    -d 'Whether to style a diagram like sketched by hand' \
    -n $root_condition

complete -c $command -l browser -x \
    -a '0\tdefault' \
    -d 'Specify the browser to open a diagram' \
    -n $root_condition

complete -c $command -s c -l center -x \
    -a 'false\tdefault true' \
    -d 'Whether to center a diagram' \
    -n $root_condition

complete -c $command -l scale -x \
    -a '-1\tdefault' \
    -d 'Specify the scale factor of a diagram' \
    -n $root_condition

complete -c $command -l scale -x \
    -a '*\tdefault' \
    -d 'Specify the target board for render' \
    -n $root_condition

complete -c $command -l font-regular -r \
    -d 'Specify the path for a regular font' \
    -n $root_condition

complete -c $command -l font-italic -r \
    -d 'Specify the path for a italic font' \
    -n $root_condition

complete -c $command -l font-bold -r \
    -d 'Specify the path for a bold font' \
    -n $root_condition

complete -c $command -l font-semibold -r \
    -d 'Specify the path for a semibold font' \
    -n $root_condition

complete -c $command -a '(__fish_d2__complete_layouts)' \
    -d 'Specify the name of a layout' \
    -n '__fish_seen_subcommand_from layout'

complete -c $command -F -n '__fish_seen_subcommand_from fmt'
