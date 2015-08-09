function __fish_print_arch_daemons --description 'Print arch daemons'
    find /etc/rc.d/ -executable -type f -printf '%f\n'

end
