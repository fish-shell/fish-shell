function __fish_print_xbps_packages
    # Caches for 5 minutes
    type -q -f xbps-query || return 1

    argparse i/installed -- $argv
    or return 1

    if not set -q _flag_installed
        # prints: <package name>	Package
        __fish_cached -t 250 -- "xbps-query -Rs '' | sed 's/^... \\([^ ]*\\)-.* .*/\\1/; s/\$/\\t'Package'/'"
        return 0
    else
        xbps-query -l | sed 's/^.. \([^ ]*\)-.* .*/\1/' # TODO: actually put package versions in tab for locally installed packages
        return 0
    end
end
