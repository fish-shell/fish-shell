function __fish_print_pkg_add_packages
    # pkg_info on OpenBSD provides versioning info which we want for
    # installed packages but, calling it directly can cause delays in
    # returning information if another pkg_* tool have a lock.
    # Listing /var/db/pkg is a clean alternative.
    if type -q -f pkg_add
        set -l files /var/db/pkg/*
        string replace /var/db/pkg/ '' -- $files
        return 0
    end
    return 1
end
