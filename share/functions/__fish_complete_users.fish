
function __fish_complete_users --description "Print a list of local users, with the real user name as a description"
	if test -x /usr/bin/getent
		getent passwd | cut -d : -f 1,5 | sed 's/:/\t/'
	else
		__fish_sgrep -ve '^#' /etc/passwd | cut -d : -f 1,5 | sed 's/:/\t/'
	end
end
