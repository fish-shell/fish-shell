function __fish_print_pygmentize
    set -l lines (pygmentize -L $argv[1] | string match -r '^(?:\* |    ).*(?:)$' | string replace -r '\* (.*):$' '$1' | string replace -r '^(.*)\.$' '$1' | string trim)

    while set -q lines[2]
        set -l names (string split ", " $lines[1])
        for name in $names
            printf '%s\t%s\n' $name $lines[2]
        end

        set -e lines[1]
        set -e lines[1]
    end
end

complete -c pygmentize -s o -d "Set output file"
complete -c pygmentize -s s -d "Read one line at a time"
complete -c pygmentize -s l -d "Set lexer" -x -a "(__fish_print_pygmentize lexers Lexer)"
complete -c pygmentize -s g -d "Guess lexer"
complete -c pygmentize -s f -d "Set formatter" -x -a "(__fish_print_pygmentize formatters Formatter)"
complete -c pygmentize -s O -d "Set coma-separated options" -x
complete -c pygmentize -s P -d "Set one option" -x
complete -c pygmentize -s F -d "Set filter" -x -a "(__fish_print_pygmentize filters Filter)"
complete -c pygmentize -s S -d "Print style definition for given style" -x -a "(__fish_print_pygmentize styles Style)"
complete -c pygmentize -s L -d "List lexers, formatters, styles or filters" -x -a "lexers formatters styles filters"
complete -c pygmentize -s N -d "Guess and print lexer name based on given file"
complete -c pygmentize -s H -d "Print detailed help" -x -a "lexer formatter filter"
complete -c pygmentize -s v -d "Print detailed traceback on unhandled exceptions"
complete -c pygmentize -s h -d "Print help"
complete -c pygmentize -s V -d "Print package version"
