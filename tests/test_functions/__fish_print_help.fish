function __fish_print_help
    if set -q argv[2]
        echo Error-message: $argv[2] >&2
    end
    echo Documentation for $argv[1] >&2
    return 1
end
