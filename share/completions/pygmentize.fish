function __fish_print_pygmentize --argument-names type description
    for line in (pygmentize -L $type | sed -e "s%^\* \(.*\):%\1%;tx;d;:x" | string split ", ")
        printf "%s\t%s\n" $line $description
    end
end

complete -c pygmentize -s o -d "Set output file"
complete -c pygmentize -s s -d "Read one line at a time"
complete -c pygmentize -s l -d "Set lexer" -x -a "(__fish_print_pygmentize lexers Lexer)"
complete -c pygmentize -s g -d "Guess lexer"
complete -c pygmentize -s f -d "Set formater" -x -a "(__fish_print_pygmentize formaters Formater)"
complete -c pygmentize -s O -d "Set coma-seperated options" -x
complete -c pygmentize -s P -d "Set one option" -x
complete -c pygmentize -s F -d "Set filter" -x -a "(__fish_print_pygmentize filters Filter)"
complete -c pygmentize -s S -d "Print style definition for given style" -x -a "(__fish_print_pygmentize styles Style)"
complete -c pygmentize -s L -d "List lexers, formaters, styles or filters" -x -a "lexers formaters styles filters"
complete -c pygmentize -s N -d "Guess and print lexer name based on given file"
complete -c pygmentize -s H -d "Print detailed help" -x -a "lexer formatter filter"
complete -c pygmentize -s v -d "Print detailed traceback on unhandled exceptions"
complete -c pygmentize -s h -d "Print help"
complete -c pygmentize -s V -d "Print package version"

function __test_args
    echo ... >> /tmp/args
    __fish_print_cmd_args >> /tmp/args
    return 1
end

complete -n "__test_args" -d Hello

