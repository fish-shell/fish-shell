function __fish_complete_abook_formats --description 'Complete abook formats'
    set -l pat
    switch $argv[1]
        case in
            set pat '/output:/,$d; /input:\|^$/d'
        case out
            set pat '/input:/,/output:/d; /^$/d'
        case '*'
            return 1
    end
    abook --formats | sed -e $pat -e 's/^\s\+//'

end
