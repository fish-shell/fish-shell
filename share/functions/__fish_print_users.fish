
function __fish_print_users --description "Print a list of local users"
	if test -x /usr/bin/getent
		getent passwd | cut -d : -f 1
	else if test -x /usr/bin/dscl # OS X support
		dscl . -list /Users | string match -r -v '^_'
	else
		string match -v -r '^\w*#' < /etc/passwd | cut -d : -f 1
	end
end

