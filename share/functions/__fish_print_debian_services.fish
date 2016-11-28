function __fish_print_debian_services --description 'Prints services installed'
    for service in /etc/init.d/*
        if [ -x $service ]
            basename $service
        end
    end
end
