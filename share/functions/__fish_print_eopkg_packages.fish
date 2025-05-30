function __fish_print_eopkg_packages
    type -q -f eopkg || return 1

    argparse i/installed -- $argv
    or return 1

    # If the cache is less than max_age, we do not recalculate it
    # Determine whether to print installed/available packages

    if set -q _flag_installed
        # Remove package version information from output and pipe into cache file
        __fish_cached -t 500 -k eopkg-installed -- "eopkg list-installed -N | cut -d ' ' -f 1"
        return 0
    else
        # Remove package version information from output and pipe into cache file
        __fish_cached -t 500 -k eopkg-available -- "eopkg list-available -N | cut -d ' ' -f 1"
        return 0
    end
    return 1
end
