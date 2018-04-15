
function __fish_complete_groups --description "Print a list of local groups, with group members as the description"
    if command -sq getent
        getent group | cut -d ':' -f 1,4 | string replace : \t
    else
        cut -d ':' -f 1,4 /etc/group | string replace : \t
    end
end
