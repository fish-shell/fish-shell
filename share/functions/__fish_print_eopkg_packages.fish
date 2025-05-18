function __fish_print_eopkg_packages
    type -q -f eopkg || return 1

    argparse i/installed -- $argv
    or return 1

    if set -q _flag_installed
        __fish_cached -t 500 -k eopkg-installed -- eopkg list-installed -N \| cut -d ' ' -f 1
        return 0
    else
        __fish_cached -t 500 -k eopkg-available -- eopkg list-available -N \| cut -d ' ' -f 1
        return 0
    end
end
