# localization: skip(private)
function __fish_print_pacman_packages
    # Caches for 5 minutes
    type -q -f pacman || return 1

    argparse i/installed r/repo=+ -- $argv
    or return 1

    if set -q _flag_installed
        pacman -Q | string replace ' ' \t
        return 0
    else if test (count $_flag_repo) -gt 0
        pacman -Sl $_flag_repo | string replace --regex '^(\S+) (\S+) .*' '$1/$2\tPackage'
        return 0
    else
        __fish_cached -t 250 -- "pacman -Ssq | sed -e 's/\$/\\tPackage/'"
        return 0
    end
end
