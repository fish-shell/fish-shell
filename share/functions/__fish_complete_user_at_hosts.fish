function __ghoti_complete_user_at_hosts -d "Print list host-names with user@"
    for user_at in (commandline -ct | string match -r '.*@'; or echo "")(__ghoti_print_hostnames)
        echo $user_at
    end
end
