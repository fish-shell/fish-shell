function __fish_print_portage_packages
    # This completes the package name from the portage tree.
    # True for installing new packages. Function for printing
    # installed on the system packages is in completions/emerge.fish

    # eix is MUCH faster than emerge so use it if it is available
    if type -q -f eix
        eix --only-names "^"(commandline -ct) | cut -d/ -f2
        return 0
    else if type -q -f emerge
        emerge -s \^(commandline -ct) | string match -r "^\*.*" | cut -d' ' -f3 | cut -d/ -f2
        return 0
    end
    return 1
end
