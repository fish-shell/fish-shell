function __fish_print_pacman_packages
    # Caches for 5 minutes
    type -q -f pacman || return 1

    argparse i/installed -- $argv
    or return 1

    if not set -q _flag_installed
        __fish_cached -t 250 -- "pacman -Ssq | sed -e 's/\$/\\tPackage/'"
        return 0
    else
        pacman -Q | string replace ' ' \t
        return 0
    end
end
