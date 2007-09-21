
function __fish_complete_groups --description "Print a list of local groups, with group members as the description"
	cat /etc/group | sed -e "s/^\([^:]*\):[^:]*:[^:]*:\(.*\)/\1\tMembers: \2/"
end
