# localization: skip(private)
function __fish_print_port_packages
    type -q -f port || return 1

    __fish_cached -t 250 -k port '
        printf "all\ncurrent\nactive\ninactive\ninstalled\nuninstalled\noutdated\n"
        port echo all | awk \'{$1=$1};1\'
    '
    return 0
end
