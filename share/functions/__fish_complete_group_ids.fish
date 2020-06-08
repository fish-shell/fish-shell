function __fish_complete_group_ids --description "Complete group IDs with group name as description"
    if command -sq getent
        getent group | string replace -f -r '^([[:alpha:]_][^:]*):[^:]*:(\d+).*' '$2\t$1'
    else if test -r /etc/group
        string replace -f -r '^([[:alpha:]_][^:]*):[^:]*:(\d+).*' '$2\t$1' </etc/group
    end
end
