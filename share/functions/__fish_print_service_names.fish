function __fish_print_service_names -d 'All services known to the system'
    if test -d /run/systemd/system
        command systemctl list-units  -t service | cut -d ' ' -f 1 | grep '\.service$' | sed -e 's/\.service$//'
    else if type -f rc-service 2> /dev/null
        command rc-service -l
    else
        command ls /etc/init.d
    end
end
