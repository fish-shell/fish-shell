function __fish_print_pkg_packages
    # Pkg is fast on FreeBSD and provides versioning info which we want for
    # installed packages
    if type -q -f pkg
        pkg query "%n-%v"
        return 0
    end
    return 1
end
