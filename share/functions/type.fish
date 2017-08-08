function type --description 'Print the type of a command'
    # For legacy reasons, no argument simply causes an unsuccessful return.
    set -q argv[1]
    or return 1

    set -l options 'h/help' 'a/all' 'f/no-functions' 't/type' 'p/path' 'P/force-path' 'q/quiet'
    argparse -n type --min-args=1 -x t,p,P $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help type
        return 0
    end

    set -l res 1
    set -l mode normal
    set -l multi no
    set -l selection all

    # Technically all four of these flags are mutually exclusive. However, we allow -q to be used
    # with the other three because old versions of this function explicitly allowed it by making
    # --quiet have precedence.
    if set -q _flag_quiet
        set mode quiet
    else if set -q _flag_type
        set mode type
    else if set -q _flag_path
        set mode path
    else if set -q _flag_force_path
        set mode path
        set selection files
    end

    set -q _flag_all
    and set multi yes

    set -q _flag_no_functions
    and set selection files

    # Check all possible types for the remaining arguments.
    for i in $argv
        # Found will be set to 1 if a match is found.
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
            set paths (command -sa -- $i)
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
