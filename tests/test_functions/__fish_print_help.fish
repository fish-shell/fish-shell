function __fish_print_help
    echo >&2 "fish: $argv[1]: missing man page"
    echo >&2 "Documentation may not be installed."
    echo >&2 "`help $argv[1]` will show an online version"
    return 1
end
