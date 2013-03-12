function __fish_service_print_names
	if type -f systemctl >/dev/null
        command systemctl list-units  -t service | cut -d ' ' -f 1 | grep '\.service$' | sed -e 's/\.service$//'
    end

    command ls /etc/init.d
end

# Fist argument is the names of the service, i.e. a file in /etc/init.d
complete -c service -n "test (count (commandline -poc)) = 1" -xa "(__fish_service_print_names)" --description "Service name"

#The second argument is what action to take with the service
complete -c service -n "test (count (commandline -poc)) -gt 1" -xa '$__fish_service_commands'

