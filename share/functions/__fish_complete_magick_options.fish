# localization: skip(private)
function __fish_complete_magick_options --description 'Complete Magick options' --argument-names what
    switch $what
        case format Format
            magick -list Format | string replace -rf '^\s*([\w-]+)\*?\s+\S+\s+([rw+-]{3})\s+(.+)$' '\1\t\2 \3'
        case color Color
            magick -list color | string replace -rf '^(\w+)\s+(\w+\(.+\))\s+(.+)$' '\1\t\2 \3'
        case family
            magick -list Font | string replace -rf '^\s*family: ' '' | sort -u
        case font Font
            magick -list Font | string replace -rf '^\s*Font: ' '' | sort -u
        case '*'
            magick -list $what
    end
end
