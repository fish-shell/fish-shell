
function __fish_complete_users --description "Print a list of local users, with the real user name as a description"
	if test -x /usr/bin/getent
		getent passwd | cut -d : -f 1,5 | string replace -r ':' \t
	else if test -x /usr/bin/dscl
		dscl . -list /Users RealName | string match -r -v '^_' | string replace -r ' {2,}' \t
	else
		string match -v -r '^\s*#' < /etc/passwd | cut -d : -f 1,5 | string replace ':' \t
	end
end
