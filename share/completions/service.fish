# Fist argument is the names of the service, i.e. a file in /etc/init.d

# magic completion safety check (do not remove this comment)
if not type -q service
    exit
end
complete -c service -n "test (count (commandline -poc)) = 1" -xa "(__fish_print_service_names)" -d "Service name"

#The second argument is what action to take with the service
complete -c service -n "test (count (commandline -poc)) -gt 1" -xa '$__fish_service_commands'

