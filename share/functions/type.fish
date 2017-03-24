function type --description 'Print the type of a command'
    # For legacy reasons, no argument simply causes an unsuccessful return.
    if not set -q argv[1]
        return 1
    end

    # Initialize
    set -l res 1
    set -l mode normal
    set -l multi no
    set -l selection all

    # Parse options
    set -l names
    while set -q argv[1]
        set -l arg $argv[1]
        set -e argv[1]
        switch $arg
            case -t --type
                # This could also be an error
                # - printing type without printing anything
                # doesn't make sense.
                if test $mode != quiet
                    set mode type
                end
            case -p --path
                if test $mode != quiet
                    set mode path
                end
            case -P --force-path
                if test $mode != quiet
                    set mode path
                end
                set selection files
            case -a --all
                set multi yes
            case -f --no-functions
                set selection files
            case -q --quiet
                set mode quiet
            case -h --help
                __fish_print_help type
                return 0
            case --
                set names $argv
                break
            case '-?' '--*'
                printf (_ "%s: Unknown option %s\n" ) type $arg
                return 1
            case '-??*'
                # Grouped options
                set argv -(string sub -s 2 -- $arg | string split "") $argv
            case '*'
                set names $arg $argv
                break
        end
    end

    # Check all possible types for the remaining arguments
    for i in $names
        # Found will be set to 1 if a match is found
        set -l found 0

        if test $selection != files
            if functions -q -- $i
                set res 0
                set found 1
                switch $mode
                    case normal
                        printf (_ '%s is a function with definition\n') $i
                        if isatty stdout
                            functions $i | fish_indent --ansi
                        else
                            functions $i | fish_indent
                        end
                    case type
                        echo (_ 'function')
                end
                if test $multi != yes
                    continue
                end
            end

            if contains -- $i (builtin -n)
                set res 0
                set found 1
                switch $mode
                    case normal
                        printf (_ '%s is a builtin\n') $i

                    case type
                        echo (_ 'builtin')
                end
                if test $multi != yes
                    continue
                end
            end
        end

        set -l paths
        if test $multi != yes
            set paths (command -s -- $i)
        else
            # TODO: This should really be `command -sa`.
            # TODO: If #3914 ('Treat empty $PATH component as equivalent to "." ')
            # is implemented, this needs to change as well.
            for file in $PATH/*
                test -x $file -a ! -d $file
                and set paths $paths $file
            end
        end
        for path in $paths
            set res 0
            set found 1
            switch $mode
                case normal
                    printf (_ '%s is %s\n') $i $path
                case type
                    echo (_ 'file')
                case path
                    echo $path
            end
            if test $multi != yes
                continue
            end
        end

        if test $found = 0
            and test $mode != quiet
            printf (_ "%s: Could not find '%s'\n") type $i >&2
        end
    end

    return $res
end
