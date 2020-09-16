function isatty -d "Tests if a file descriptor is a tty"
    set -l options h/help
    argparse -n isatty $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help isatty
        return 0
    end

    if set -q argv[2]
        printf (_ "%s: Too many arguments") isatty >&2
        return 1
    end

    set -l fd
    switch "$argv"
        case stdin ''
            set fd 0
        case stdout
            set fd 1
        case stderr
            set fd 2
        case '*'
            set fd $argv[1]
    end

    test -t "$fd"
end
