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
