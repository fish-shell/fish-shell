
function __fish_print_users --description "Print a list of local users"
	cat /etc/passwd | cut -d : -f 1
end

