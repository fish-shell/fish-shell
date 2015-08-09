function __fish_complete_convert_options --description 'Complete Convert options' --argument what
    switch $what
        case format Format
            convert -list Format | sed '1,/----/d; /^$/,$d; /^$/d; s/^\s*\([a-zA-Z0-9-]\+\)\**\s*\S\+\s\+\\(\S\+\)\s\+\(.\+\S\)\s*$/\1\t\2 \3/'
        case color Color
            convert -list color | awk '{ print $1"\t"$2" "$3} ' | sed '1,/----/d'
        case family
            convert -list Font | grep family | sed 's/^\s*.\+: //' | sort -u
        case font Font
            convert -list Font | grep Font | sed 's/^\s*.\+: //' | sort -u
        case '*'
            convert -list $what
    end


end
