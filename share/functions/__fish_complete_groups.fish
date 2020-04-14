function __fish_complete_groups --description "Print a list of local groups, with group members as the description"
    if command -sq getent
        getent group | while read -l line
            string split -f 1,4 : -- $line | string join \t
        end
    else
        while read -l line
            string split -f 1,4 : -- $line | string join \t
        end </etc/group
    end
end
