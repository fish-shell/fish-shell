function __fish_print_debian_services --description 'Prints services installed'
    path filter -fxZ /etc/init.d/* | path basename
end

function __fish_invoke_rcd_has_service
    set -l tokens (commandline -xpc)
    if test (count $tokens) -eq 2
        return 0
    else
        return 1
    end
end

complete -f -c invoke-rc.d -n 'not __fish_invoke_rcd_has_service' -a '(__fish_print_debian_services)'
complete -f -c invoke-rc.d -n __fish_invoke_rcd_has_service -a start -d 'Start the service'
complete -f -c invoke-rc.d -n __fish_invoke_rcd_has_service -a stop -d 'Stop the service'
complete -f -c invoke-rc.d -n __fish_invoke_rcd_has_service -a restart -d 'Restart the service'
complete -f -c invoke-rc.d -n __fish_invoke_rcd_has_service -a reload -d 'Reload Configuration'
complete -f -c invoke-rc.d -n __fish_invoke_rcd_has_service -a force-reload -d 'Force reloading configuration'
complete -f -c invoke-rc.d -n __fish_invoke_rcd_has_service -a status -d 'Print the status of the service'
