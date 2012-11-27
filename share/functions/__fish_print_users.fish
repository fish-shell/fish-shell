
function __fish_print_users --description "Print a list of local users"
	if test -x /usr/bin/getent
		getent passwd | cut -d : -f 1
	else
		sgrep -ve '^#' /etc/passwd | cut -d : -f 1
	end
end

