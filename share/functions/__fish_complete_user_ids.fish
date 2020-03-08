function __fish_complete_user_ids --description "Complete user IDs with user name as description"
    if command -sq getent
        getent passwd | string replace -f -r '^([[:alpha:]_][^:]*):[^:]*:(\d+).*' '$2\t$1'
    else if test -r /etc/passwd
        string replace -f -r '^([[:alpha:]_][^:]*):[^:]*:(\d+).*' '$2\t$1' </etc/passwd
    end
end
