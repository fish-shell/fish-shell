function __fish_complete_lpr_option --description 'Complete lpr option'
    set -l optstr "$(commandline -t)"
    switch $optstr
        case '*=*'
            string split -m1 = -- "$optstr" | read -l opt val
            set -l descr
            for l in (lpoptions -l 2>/dev/null | string match -- "*$opt*" | string replace -r '.*/(.*):\s*(.*)$' '$1 $2' | string split " ")
                if not set -q descr[1]
                    set descr $l
                    continue
                end
                set -l default ''
                if string match -q '\**' -- $l
                    set default 'Default '
                    set l (string sub -s 2 -- $l)
                end
                echo $opt=$l\t$default$descr
            end
        case '*'
            lpoptions -l 2>/dev/null | string replace -r '(.*)/(.*):.*$' '$1=\t$2'
    end

end
