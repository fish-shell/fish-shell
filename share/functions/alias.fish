function alias --description 'Creates a function wrapping a command'
    if count $argv > /dev/null
        switch $argv[1]
            case -h --h --he --hel --help
                __fish_print_help alias
                return 0
        end
    end

    set -l name
    set -l body
    set -l prefix
    set -l first_word
    switch (count $argv)

        case 0
            for func in (functions -n)
                functions $func | string match -- "function * --description 'alias *" | string replace -r -- "function .* --description '" ''| string trim -c\'
            end
            return 0
        case 1
            set -l tmp (string replace -r "=" '\n' -- $argv) ""
            set name $tmp[1]
            set body $tmp[2]

        case 2
            set name $argv[1]
            set body $argv[2]

        case \*
            printf ( _ "%s: Expected one or two arguments, got %d\n") alias (count $argv)
            return 1
    end

    # sanity check
    if test -z "$name"
        printf ( _ "%s: Name cannot be empty\n") alias
        return 1
    else if test -z "$body"
        printf ( _ "%s: Body cannot be empty\n") alias
        return 1
    end

    # Extract the first command from the body
    # This is supposed to replace all non-escaped (i.e. preceded by an odd number of `\`) spaces with a newline
    # so it splits on them
    set -l tmp (string replace -ra "([^\\\ ])((\\\\\\\)*) " '$1\n' $body)
    set first_word (string trim $tmp[1])
    if set -q tmp[2]
        set body $tmp[2..-1]
    else
        set body
    end

    # Prevent the alias from immediately running into an infinite recursion if
    # $body starts with the same command as $name.

    if test $first_word = $name
        if contains $name (builtin --names)
            set prefix builtin
        else
            set prefix command
        end
    end
    echo "function $name --wraps $first_word --description \"alias $argv\"; $prefix $first_word $body \$argv; end" | source
end
