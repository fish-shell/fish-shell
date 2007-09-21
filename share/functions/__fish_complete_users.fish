
function __fish_complete_users --description "Print a list of local users, with the real user name as a description"
	cat /etc/passwd | sed -e "s/^\([^:]*\):[^:]*:[^:]*:[^:]*:\([^:]*\):.*/\1\t\2/"
end
